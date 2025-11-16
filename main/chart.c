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

void chart_init(ChartCtx* chart_ctx, int num_channels, int history_size) {
    chart_ctx->num_channels = num_channels;
    chart_ctx->history_size = history_size;
    chart_ctx->write_pos    = 0;
    chart_ctx->min_value    = 0;
    chart_ctx->max_value    = 0;

    const size_t circular_buffer_size =
      chart_ctx->num_channels * chart_ctx->history_size * sizeof(float);
    chart_ctx->data = malloc(circular_buffer_size);
    if (chart_ctx->data == NULL) {
        fprintf(stderr,
                "Failed to allocate circular buffer for chart (%d channels of "
                "%d history values; %zu bytes)\n",
                chart_ctx->num_channels,
                chart_ctx->history_size,
                circular_buffer_size);
        abort();
    }

    for (size_t i = 0; i < chart_ctx->num_channels * chart_ctx->history_size;
         i++)
        chart_ctx->data[i] = 0.f;
}

void chart_destroy(ChartCtx* chart_ctx) {
    if (chart_ctx->data != NULL) {
        free(chart_ctx->data);
        chart_ctx->data = NULL;
    }
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

void chart_update_minmax(ChartCtx* chart_ctx) {
    assert(chart_ctx->num_channels > 0);

    float min = chart_ctx->data[0];
    float max = chart_ctx->data[0];

    for (int cur_channel = 0; cur_channel < chart_ctx->num_channels;
         cur_channel++) {
        for (int i = 0; i < chart_ctx->history_size; i++) {
            const float val =
              chart_ctx->data[chart_ctx->history_size * cur_channel + i];
            if (val < min)
                min = val;
            if (val > max)
                max = val;
        }
    }

    /* Add 10% margin to avoid clipping at edges */
    const float range  = max - min;
    const float margin = range * 0.1f;

    chart_ctx->min_value = min - margin;
    chart_ctx->max_value = max + margin;
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

    /* Prevent division by zero if all values are identical */
    float min_value = chart_ctx->min_value;
    float max_value = chart_ctx->max_value;
    if (min_value == max_value) {
        min_value -= 1.0f;
        max_value += 1.0f;
    }

    /* Calculate scale factor based on min/max values */
    const int display_height = render_get_height(render_ctx);
    const float value_range  = max_value - min_value;
    const float scale        = (float)display_height / value_range;

    /* Draw each channel */
    for (int cur_channel = 0; cur_channel < chart_ctx->num_channels;
         cur_channel++) {
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
              display_height - (int)((val_prev - min_value) * scale);
            const int y_cur =
              display_height - (int)((val_cur - min_value) * scale);

            /* Draw line segment */
            const uint32_t cur_color =
              channel_colors[cur_channel % LENGTH(channel_colors)];
            render_draw_line(render_ctx, x - 1, y_prev, x, y_cur, cur_color);
        }
    }
}
