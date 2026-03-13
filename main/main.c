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

#include "render.h"
#include "chart.h"
#include "serial_uart.h"
#include "serial_bluetooth.h"
#include "util.h"

/*
 * Display resolution in pixels.
 * The ILI9341 controller supports 320x240 in landscape orientation.
 */
#define LCD_WIDTH  320 /* Horizontal resolution */
#define LCD_HEIGHT 240 /* Vertical resolution */

/*
 * TODO: Don't hard-code channel number, obtain from number of rendered OBD2
 * fields.
 */
#define CHANNEL_NUM 4

#if 0
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
/* MAC address of the target Bluetooth SPP device */
static const uint8_t TARGET_MAC[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

void app_main(void) {
    LOGI("Booted ESP32 CYD OBD2.");

    serial_uart_init();
    LOGI("UART initialized.");

    if (!serial_bt_init()) {
        LOGE("Failed to initialize Bluetooth.");
        return;
    }

    if (!serial_bt_connect(TARGET_MAC))
        LOGW("Initial BT connect failed, will retry in loop.");

    uint8_t buf[128];
    for (;;) {
        if (!serial_bt_is_connected()) {
            LOGI("BT disconnected, reconnecting...");
            serial_bt_connect(TARGET_MAC);
            continue;
        }

        /* UART → BT */
        int n = serial_uart_read(buf, sizeof(buf));
        if (n > 0)
            serial_bt_write(buf, n);

        /* BT → UART */
        n = serial_bt_read(buf, sizeof(buf));
        if (n > 0)
            serial_uart_write(buf, n);
    }
}
#endif
