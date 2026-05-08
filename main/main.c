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

#include <stdbool.h>
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
#include "serial_bluetooth.h"
#include "util.h"
#include "elm327.h"
#include "obd2.h"

/*
 * Display resolution in pixels.
 * The ILI9341 controller supports 320x240 in landscape orientation.
 */
#define LCD_WIDTH  320
#define LCD_HEIGHT 240

/*
 * Delay in miliseconds for fetching PID data from the OBD2 adapter.
 */
#define FETCH_DELAY_MS 10

/*
 * Height of the OBD2 HUD row at the top of the screen.
 */
#define HUD_HEIGHT ((int)(LCD_HEIGHT / 4))

/*
 * Vertical padding (in pixels) added above and below the chart area, so that
 * plotted lines have room to breathe near the top and bottom edges.
 */
#define CHART_PADDING 5

/*
 * MAC address of the target Bluetooth SPP device.
 */
static const uint8_t TARGET_MAC[6] = { 0x1C, 0x1B, 0xB5, 0x71, 0x80, 0x9E };

/*----------------------------------------------------------------------------*/

/*
 * Associates an OBD2 PID with the short label displayed above its value in the
 * HUD, a flag controlling whether it is plotted in the chart, and the RGB888
 * color used for both the HUD text and chart line.
 */
typedef struct RenderedPid {
    EObdPid pid;
    const char* label;
    uint32_t color;
} RenderedPid;

/*----------------------------------------------------------------------------*/

/*
 * Query each PID in 'rendered_pids' sequentially, storing the decoded result
 * into the corresponding slot of 'obd_values'. Both arrays have 'num_pids'
 * elements.
 */
static void fetch_pid_values(Elm327Ctx* elm_ctx,
                             const RenderedPid* rendered_pids,
                             float* obd_values,
                             size_t num_pids) {
    uint8_t buf[64];

    for (size_t i = 0; i < num_pids; i++) {
        vTaskDelay(pdMS_TO_TICKS(FETCH_DELAY_MS));

        const EObdPid pid    = rendered_pids[i].pid;
        const char* pid_name = obd_pid_name(pid);

        const size_t req_len = obd_build_request(pid, buf, sizeof(buf));
        if (req_len == 0) {
            LOGE("Failed to build OBD2 request for PID '%s'.", pid_name);
            continue;
        }
        elm327_write(elm_ctx, buf, req_len);

        const size_t resp_len =
          elm327_read_response(elm_ctx, buf, sizeof(buf), 5000);
        if (resp_len == 0) {
            LOGW("No BT response received for PID '%s'.", pid_name);
            continue;
        }

        if (!obd_decode_response(pid, buf, resp_len, &obd_values[i])) {
            LOGW("Failed to decode OBD2 response for PID '%s':", pid_name);
            HEXDUMP(buf, resp_len);
        }
    }
}

/*
 * Render each value from 'values' centered in its own cell, using the label
 * from the corresponding 'rendered_pids' entry. Both arrays have 'num_values'
 * elements.
 */
static void draw_value_hud(Framebuffer* fb,
                           const RenderedPid* rendered_pids,
                           const float* values,
                           size_t num_values) {
    const int cell_width = LCD_WIDTH / num_values;

    framebuffer_clear(fb);

    for (int i = 0; i < num_values; i++) {
        const int cx       = (i * cell_width) + (cell_width / 2);
        const int label_cy = HUD_HEIGHT / 4;
        const int value_cy = 3 * HUD_HEIGHT / 4;

        draw_text_centered(fb,
                           &FONT_8X16,
                           cx,
                           label_cy,
                           rendered_pids[i].color,
                           rendered_pids[i].label);

        char val_str[8];
        snprintf(val_str, sizeof(val_str), "%.0f", values[i]);
        draw_text_centered(fb,
                           &FONT_8X16,
                           cx,
                           value_cy,
                           rendered_pids[i].color,
                           val_str);
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

    static const RenderedPid rendered_pids[] = {
#if 1 /* Pastel color palette */
        { OBD_PID_RPM, "RPM", 0x70FF70 },
        { OBD_PID_SPEED, "SPD", 0xFF7070 },
        { OBD_PID_THROTTLE, "THR", 0xFFFF70 },
        { OBD_PID_INTAKE_PRESSURE, "MAP", 0x7070FF },
        { OBD_PID_ENGINE_LOAD, "LOD", 0xFF70FF },
        { OBD_PID_COOLANT_TEMP, "CLT", 0x70FFFF },
#else /* Neon color palette */
        { OBD_PID_RPM, "RPM", 0x39FF14 },
        { OBD_PID_SPEED, "SPD", 0xFF3C3C },
        { OBD_PID_THROTTLE, "THR", 0xFFD700 },
        { OBD_PID_INTAKE_PRESSURE, "MAP", 0x5B9EFF },
        { OBD_PID_ENGINE_LOAD, "LOD", 0xBF5FFF },
        { OBD_PID_COOLANT_TEMP, "CLT", 0x00E5CC },
#endif
    };
    const size_t hud_num_pids = LENGTH(rendered_pids);
    const size_t num_plotted  = 5; /* First 5 are plotted into the chart */

    /* Initialize global render context for the entire screen */
    RenderCtx render_ctx;
    render_init(&render_ctx, LCD_WIDTH, LCD_HEIGHT);
    render_clear(&render_ctx);
    LOGI("Display initialized: %dx%d", LCD_WIDTH, LCD_HEIGHT);

    /* Initialize framebuffer for the HUD (numbers on top) */
    Framebuffer hud_fb;
    framebuffer_init(&hud_fb, LCD_WIDTH, HUD_HEIGHT);
    LOGI("HUD framebuffer initialized.");

    /* Copy chart colors into separate array for the chart initialization */
    uint32_t chart_colors[num_plotted];
    for (size_t i = 0; i < num_plotted; i++)
        chart_colors[i] = rendered_pids[i].color;

    /* Initialize chart context for the graph */
    ChartCtx chart_ctx;
    chart_init(&chart_ctx,
               num_plotted,
               chart_colors,
               0,
               HUD_HEIGHT + CHART_PADDING,
               LCD_WIDTH,
               LCD_HEIGHT - HUD_HEIGHT - 2 * CHART_PADDING);
    LOGI("Chart initialized for %d channels.", num_plotted);

    if (!serial_bt_init()) {
        LOGE("Failed to initialize Bluetooth.");
        return;
    }
    LOGI("Initialized Bluetooth.");

    Elm327Ctx elm_ctx;
    elm327_init(&elm_ctx, serial_bt_write, serial_bt_read_blocking);
    LOGI("Initialized ELM327 context.");

    /*
     * NOTE: We can reuse the same array for HUD and chart values, since the
     * plotted values always come first.
     */
    float obd_values[hud_num_pids];
    for (size_t i = 0; i < hud_num_pids; i++)
        obd_values[i] = 0.f;

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

        /* Read each PID value from 'rendered_pids' into 'obd_values' */
        fetch_pid_values(&elm_ctx, rendered_pids, obd_values, hud_num_pids);

        /* Draw the HUD on top (numeric values) */
        draw_value_hud(&hud_fb, rendered_pids, obd_values, hud_num_pids);
        render_draw_framebuffer(&render_ctx, &hud_fb, 0, 0);

        /* Plot the first N values into the chart */
        chart_push(&chart_ctx, obd_values, num_plotted);
        chart_render(&chart_ctx, &render_ctx);
    }
}
