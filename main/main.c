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

/*
 * ESP-IDF application entry point.
 *
 * Initializes the display and UART, then enters a loop to read CSV data from
 * serial and plot it as a scrolling multi-channel line chart.
 */
void app_main(void) {
    /* Initialize rendering */
    RenderCtx render_ctx;
    render_init(&render_ctx, LCD_WIDTH, LCD_HEIGHT);
    render_clear(&render_ctx);
    render_flush(&render_ctx);

    /* Initialize chart context, which will contain the data being plotted */
    ChartCtx chart_ctx;
    chart_init(&chart_ctx, CHANNEL_NUM, render_get_width(&render_ctx));

    /* Initialize serial communication, which will be used to receive data */
    serial_uart_init();

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
                fprintf(stderr,
                        "Failed to read serial data in channel #%d\n",
                        i);
                continue;
            }
            values[i] = received_value;
        }

        /* Push the received values to the chart context */
        chart_push(&chart_ctx, values, LENGTH(values));

        /* Update auto-scaling of the chart */
        chart_update_minmax(&chart_ctx);

        /* Redraw chart to framebuffer and flush to display */
        render_clear(&render_ctx);
        chart_render(&chart_ctx, &render_ctx);
        render_flush(&render_ctx);
    }

    chart_destroy(&chart_ctx);
    render_destroy(&render_ctx);
}
