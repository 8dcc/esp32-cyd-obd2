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

#include "chart.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "render.h"

void chart_init(ChartCtx* chart_ctx,
                int num_channels,
                int x,
                int y,
                int width,
                int height) {
    /*
     * In this chart, since we have enough memory, the full value history will
     * be kept, and rendered each frame. This enables useful features, such as
     * Y-axis autoscaling.
     */
    chart_ctx->num_channels = num_channels;
    chart_ctx->history_size = width;
    chart_ctx->write_pos    = 0;
    chart_ctx->x            = x;
    chart_ctx->y            = y;

    /* Allocate the circular buffer and per-channel min/max arrays */
    const size_t data_size =
      chart_ctx->num_channels * chart_ctx->history_size * sizeof(float);
    const size_t minmax_size = chart_ctx->num_channels * sizeof(float);
    chart_ctx->data          = malloc(data_size);
    chart_ctx->min_values    = malloc(minmax_size);
    chart_ctx->max_values    = malloc(minmax_size);
    if (chart_ctx->data == NULL || chart_ctx->min_values == NULL ||
        chart_ctx->max_values == NULL) {
        fprintf(stderr,
                "Failed to allocate chart buffers (%d channels of "
                "%d history values; %zu bytes)\n",
                chart_ctx->num_channels,
                chart_ctx->history_size,
                data_size);
        abort();
    }

    for (size_t i = 0; i < chart_ctx->num_channels * chart_ctx->history_size;
         i++)
        chart_ctx->data[i] = 0.f;
    for (int i = 0; i < chart_ctx->num_channels; i++) {
        chart_ctx->min_values[i] = 0.f;
        chart_ctx->max_values[i] = 0.f;
    }

    /*
     * Initialize the framebuffer, which will be used for rendering the chart
     * into the display. Again, since we have enough memory, we allocate a full
     * display framebuffer, which will be cleared and re-drawn each frame.
     */
    framebuffer_init(&chart_ctx->framebuffer, width, height);
}

void chart_destroy(ChartCtx* chart_ctx) {
    if (chart_ctx->data != NULL) {
        free(chart_ctx->data);
        chart_ctx->data = NULL;
    }
    if (chart_ctx->min_values != NULL) {
        free(chart_ctx->min_values);
        chart_ctx->min_values = NULL;
    }
    if (chart_ctx->max_values != NULL) {
        free(chart_ctx->max_values);
        chart_ctx->max_values = NULL;
    }

    framebuffer_destroy(&chart_ctx->framebuffer);
}

void chart_push(ChartCtx* chart_ctx, const float* values, int num_values) {
    /* This function must receive a value per chart channel */
    assert(num_values == chart_ctx->num_channels);

    /*
     * Write each value from the received array into the circular buffer of the
     * corresponding channel.
     */
    for (int cur_channel = 0; cur_channel < chart_ctx->num_channels;
         cur_channel++) {
        const size_t raw_data_idx =
          chart_ctx->history_size * cur_channel + chart_ctx->write_pos;
        chart_ctx->data[raw_data_idx] = values[cur_channel];
    }

    /* Advance write position */
    chart_ctx->write_pos++;
    if (chart_ctx->write_pos >= chart_ctx->history_size)
        chart_ctx->write_pos = 0;
}

/*
 * TODO: Possibly make static and call from 'chart_render'.
 * TODO: Possibly return values, rather than writing them into the context
 * structure.
 */
void chart_update_minmax(ChartCtx* chart_ctx) {
    assert(chart_ctx->num_channels > 0);

    for (int cur_channel = 0; cur_channel < chart_ctx->num_channels;
         cur_channel++) {
        const float* channel_data =
          &chart_ctx->data[chart_ctx->history_size * cur_channel];

        float min = channel_data[0];
        float max = channel_data[0];
        for (int i = 1; i < chart_ctx->history_size; i++) {
            if (channel_data[i] < min)
                min = channel_data[i];
            if (channel_data[i] > max)
                max = channel_data[i];
        }

        /* Add 10% margin to avoid clipping at edges */
        const float range  = max - min;
        const float margin = range * 0.1f;

        chart_ctx->min_values[cur_channel] = min - margin;
        chart_ctx->max_values[cur_channel] = max + margin;
    }
}

void chart_render(const ChartCtx* chart_ctx, const RenderCtx* render_ctx) {
    /* Color palette for different channels */
    static const uint32_t channel_colors[] = {
        0xFF0000, /* Red */
        0x00FF00, /* Green */
        0x0000FF, /* Blue */
        0xFFFF00, /* Yellow */
        0xFF00FF, /* Magenta */
        0x00FFFF, /* Cyan */
        0xFFFFFF, /* White */
        0xFF8800, /* Orange */
    };

    assert(chart_ctx->num_channels > 0);

    const int framebuffer_height =
      framebuffer_get_height(&chart_ctx->framebuffer);

    /* Clear the framebuffer, before redrawing */
    framebuffer_clear(&chart_ctx->framebuffer);

    /* Draw each channel to the framebuffer */
    for (int cur_channel = 0; cur_channel < chart_ctx->num_channels;
         cur_channel++) {
        /* Prevent division by zero if all values in channel are identical */
        float min_value = chart_ctx->min_values[cur_channel];
        float max_value = chart_ctx->max_values[cur_channel];
        if (min_value == max_value) {
            min_value -= 1.0f;
            max_value += 1.0f;
        }

        const float value_range = max_value - min_value;
        const float scale       = (float)framebuffer_height / value_range;

        for (int x = 1; x < chart_ctx->history_size; x++) {
            /* Get indices in circular buffer */
            const int idx_prev =
              (chart_ctx->write_pos + x - 1) % chart_ctx->history_size;
            const int idx_cur =
              (chart_ctx->write_pos + x) % chart_ctx->history_size;

            /* Get values and scale to screen coordinates */
            const float val_prev =
              chart_ctx->data[chart_ctx->history_size * cur_channel + idx_prev];
            const float val_cur =
              chart_ctx->data[chart_ctx->history_size * cur_channel + idx_cur];

            /* Convert to screen Y coordinates (inverted, 0 at top) */
            const int y_prev =
              framebuffer_height - (int)((val_prev - min_value) * scale);
            const int y_cur =
              framebuffer_height - (int)((val_cur - min_value) * scale);

            /* Draw line segment */
            const uint32_t cur_color =
              channel_colors[cur_channel % LENGTH(channel_colors)];
            framebuffer_draw_line(&chart_ctx->framebuffer,
                                  x - 1,
                                  y_prev,
                                  x,
                                  y_cur,
                                  cur_color);
        }
    }

    /*
     * Draw the entire framebuffer we just filled into the LCD, filling it
     * entirely.
     */
    render_draw_framebuffer(render_ctx,
                            &chart_ctx->framebuffer,
                            chart_ctx->x,
                            chart_ctx->y);
}
