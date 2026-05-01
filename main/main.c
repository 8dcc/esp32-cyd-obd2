/*
 * Copyright 2025 8dcc
 *
 * This file is part of ESP32 CYD OBD2.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h> /* memset, strtok */
#include <stdlib.h> /* atof */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "render.h"
#include "framebuffer.h"
#include "chart.h"
#include "draw_text.h"
#include "fonts.h"
#include "serial_uart.h"
#include "serial_bluetooth.h"
#include "util.h"
#include "elm327.h"
#include "obd2.h"

/*
 * Display resolution in pixels.
 * The ILI9341 controller supports 320x240 in landscape orientation.
 */
#define LCD_WIDTH  320 /* Horizontal resolution */
#define LCD_HEIGHT 240 /* Vertical resolution */

#if 0
/*
 * TODO: Don't hard-code channel number, obtain from number of rendered OBD2
 * fields.
 */
#define CHANNEL_NUM 4

/*
 * ESP-IDF application entry point.
 *
 * Initializes the display and UART, then enters a loop to read CSV data from
 * serial and plot it as a scrolling multi-channel line chart.
 */
void app_main(void) {
    LOGI("Booted ESP32 CYD OBD2.");

    /* Initialize rendering context */
    RenderCtx render_ctx;
    render_init(&render_ctx, LCD_WIDTH, LCD_HEIGHT);
    render_clear(&render_ctx);
    LOGI("Display initialized: %dx%d", LCD_WIDTH, LCD_HEIGHT);

    /* Initialize chart context, which will contain the data being plotted */
    ChartCtx chart_ctx;
    chart_init(&chart_ctx,
               CHANNEL_NUM,
               render_get_width(&render_ctx),
               render_get_height(&render_ctx));
    LOGI("Initialized chart context.");

    /* Initialize serial communication, which will be used to receive data */
    serial_uart_init();
    LOGI("Initialized UART serial for data.");

    /*
     * Array of values read each iteration. It is declared outside of the main
     * loop, so the old values are stored in case one read fails.
     */
    float values[CHANNEL_NUM];
    for (int i = 0; i < LENGTH(values); i++)
        values[i] = 0.f;

    for (;;) {
        /* Read values from serial */
        for (int i = 0; i < LENGTH(values); i++) {
            float received_value;
            if (!serial_uart_read_value(&received_value)) {
                LOGE("Failed to read serial data in channel #%d\n", i);
                continue;
            }
            values[i] = received_value;
        }

        /* Push the received values to the chart context */
        chart_push(&chart_ctx, values, LENGTH(values));

        /* Update auto-scaling of the chart */
        chart_update_minmax(&chart_ctx);

        /* Draw the chart to the display associated to the render context */
        chart_render(&chart_ctx, &render_ctx);
    }

    LOGI("Deinitializing...");
    chart_destroy(&chart_ctx);
    render_destroy(&render_ctx);
}
#else
/*
 * MAC address of the target Bluetooth SPP device.
 */
static const uint8_t TARGET_MAC[6] = { 0x1C, 0x1B, 0xB5, 0x71, 0x80, 0x9E };

/*
 * Delay in miliseconds for fetching PID data from the OBD2 adapter.
 */
#define FETCH_DELAY_MS 100

/*
 * Height of the OBD2 HUD row at the top of the screen (1/5 of total).
 */
#define HUD_HEIGHT (LCD_HEIGHT / 5)

/*
 * Render each value from 'values' centered in its own cell, alongside the
 * corresponding label from 'labels'. Both arrays have 'num_pids' elements.
 */
static void draw_value_hud(Framebuffer* fb,
                           const char* const* labels,
                           const float* values,
                           size_t num_values) {
    const int cell_width = LCD_WIDTH / num_values;

    framebuffer_clear(fb);

    for (int i = 0; i < num_values; i++) {
        const int cx       = (i * cell_width) + (cell_width / 2);
        const int label_cy = HUD_HEIGHT / 4;
        const int value_cy = 3 * HUD_HEIGHT / 4;

        draw_text_centered(fb, &FONT_8X16, cx, label_cy, 0xFFFFFF, labels[i]);

        char val_str[8];
        snprintf(val_str, sizeof(val_str), "%.0f", values[i]);
        draw_text_centered(fb, &FONT_8X16, cx, value_cy, 0xFFFFFF, val_str);
    }

    /* Draw 1px vertical separators between cells */
    for (int i = 1; i < num_values; i++) {
        const int x = i * cell_width;
        framebuffer_draw_line(fb, x, 0, x, HUD_HEIGHT - 1, 0x404040);
    }

    /* Draw 1px horizontal line along the bottom of the HUD row */
    framebuffer_draw_line(fb,
                          0,
                          HUD_HEIGHT - 1,
                          LCD_WIDTH - 1,
                          HUD_HEIGHT - 1,
                          0x404040);
}

void app_main(void) {
    LOGI("Booted ESP32 CYD OBD2.");

    serial_uart_init();
    LOGI("UART initialized.");

    RenderCtx render_ctx;
    render_init(&render_ctx, LCD_WIDTH, LCD_HEIGHT);
    render_clear(&render_ctx);
    LOGI("Display initialized: %dx%d", LCD_WIDTH, LCD_HEIGHT);

    Framebuffer hud_fb;
    framebuffer_init(&hud_fb, LCD_WIDTH, HUD_HEIGHT);
    LOGI("HUD framebuffer initialized.");

    static const char* const hud_labels[] = {
        "RPM", "SPD", "THR", "MAP", "LOD", "CLT",
    };
    static const EObdPid hud_pids[] = {
        OBD_PID_RPM,         OBD_PID_SPEED,
        OBD_PID_THROTTLE,    OBD_PID_INTAKE_PRESSURE,
        OBD_PID_ENGINE_LOAD, OBD_PID_COOLANT_TEMP,
    };
    const size_t hud_num_pids = LENGTH(hud_pids);

    if (!serial_bt_init()) {
        LOGE("Failed to initialize Bluetooth.");
        return;
    }

    Elm327Ctx elm_ctx;
    elm327_init(&elm_ctx, serial_bt_write, serial_bt_read_blocking);

    float hud_values[hud_num_pids];
    uint8_t buf[64];

    for (;;) {
        if (!serial_bt_is_connected()) {
            LOGI("Attempting bluetooth connection...");

            if (!serial_bt_connect(TARGET_MAC)) {
                LOGE("Failed to connect to OBD2 adapter.");
                continue;
            }
            LOGI("Connected to OBD2 adapter.");

            if (!elm327_setup(&elm_ctx)) {
                LOGE("ELM327 setup failed.");
                continue;
            }
            LOGI("ELM327 setup complete.");
        }

        for (size_t i = 0; i < hud_num_pids; i++) {
            vTaskDelay(pdMS_TO_TICKS(FETCH_DELAY_MS));

            const EObdPid pid    = hud_pids[i];
            const char* pid_name = obd_pid_name(pid);

            const size_t req_len = obd_build_request(pid, buf, sizeof(buf));
            if (req_len == 0) {
                LOGE("Failed to build OBD2 request for PID '%s'.", pid_name);
                continue;
            }
            elm327_write(&elm_ctx, buf, req_len);

            const size_t resp_len =
              elm327_read_response(&elm_ctx, buf, sizeof(buf), 5000);
            if (resp_len == 0) {
                LOGW("No BT response received for PID '%s'.", pid_name);
                continue;
            }

            if (!obd_decode_response(pid, buf, resp_len, &hud_values[i])) {
                LOGW("Failed to decode OBD2 response for PID '%s':", pid_name);
                HEXDUMP(buf, resp_len);
                continue;
            }
        }

        draw_value_hud(&hud_fb, hud_labels, hud_values, hud_num_pids);
        render_draw_framebuffer(&render_ctx, &hud_fb, 0, 0);
    }
}
#endif
