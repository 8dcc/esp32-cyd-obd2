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
#include <stdlib.h>

#include "util.h"
#include "render.h"

/*
 * Number of pixels between consecutive data points on the X axis. A value
 * of 1 maps each sample to a single pixel; a value of N spreads each sample
 * over N pixels, reducing the history size proportionally.
 *
 * NOTE: This could be moved into 'ChartCtx' to allow per-instance spacing.
 */
#define CHART_POINT_SPACING 5

void chart_init(ChartCtx* chart_ctx,
                int num_channels,
                const uint32_t* channel_colors,
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
    chart_ctx->history_size = width / CHART_POINT_SPACING;
    chart_ctx->write_pos    = 0;
    chart_ctx->x            = x;
    chart_ctx->y            = y;

    const size_t channels_size = num_channels * sizeof(ChartChannel);
    chart_ctx->channels        = malloc(channels_size);
    if (chart_ctx->channels == NULL) {
        LOGE("Failed to allocate channel array (%zu bytes)", channels_size);
        abort();
    }

    const size_t data_size = chart_ctx->history_size * sizeof(float);
    for (int i = 0; i < num_channels; i++) {
        ChartChannel* ch = &chart_ctx->channels[i];

        ch->data = malloc(data_size);
        if (ch->data == NULL) {
            LOGE("Failed to allocate data buffer for channel %d (%zu bytes)",
                 i,
                 data_size);
            abort();
        }

        for (int j = 0; j < chart_ctx->history_size; j++)
            ch->data[j] = 0.f;

        ch->min_value = 0.f;
        ch->max_value = 0.f;
        ch->color     = channel_colors[i];
    }

    /*
     * Initialize the framebuffer, which will be used for rendering the chart
     * into the display. Again, since we have enough memory, we allocate a full
     * display framebuffer, which will be cleared and re-drawn each frame.
     */
    framebuffer_init(&chart_ctx->framebuffer, width, height);
}

void chart_destroy(ChartCtx* chart_ctx) {
    if (chart_ctx->channels != NULL) {
        for (int i = 0; i < chart_ctx->num_channels; i++)
            if (chart_ctx->channels[i].data != NULL)
                free(chart_ctx->channels[i].data);

        free(chart_ctx->channels);
        chart_ctx->channels = NULL;
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
    for (int i = 0; i < chart_ctx->num_channels; i++)
        chart_ctx->channels[i].data[chart_ctx->write_pos] = values[i];

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

    for (int i = 0; i < chart_ctx->num_channels; i++) {
        ChartChannel* cur_channel = &chart_ctx->channels[i];

        float min = cur_channel->data[0];
        float max = cur_channel->data[0];
        for (int i = 1; i < chart_ctx->history_size; i++) {
            if (cur_channel->data[i] < min)
                min = cur_channel->data[i];
            if (cur_channel->data[i] > max)
                max = cur_channel->data[i];
        }

        /*
         * Add 10% margin to avoid clipping at edges
         *
         * TODO: Shouldn't we have zero "padding" on the graph itself, and add
         * actual "margin" to the render coordinates? The current approach
         * causes some allocated chart bytes to always be black.
         */
        const float range  = max - min;
        const float margin = range * 0.1f;

        cur_channel->min_value = min - margin;
        cur_channel->max_value = max + margin;
    }
}

void chart_render(const ChartCtx* chart_ctx, const RenderCtx* render_ctx) {
    assert(chart_ctx->num_channels > 0);

    const int framebuffer_height =
      framebuffer_get_height(&chart_ctx->framebuffer);

    /* Clear the framebuffer, before redrawing */
    framebuffer_clear(&chart_ctx->framebuffer);

    /* Draw each channel to the framebuffer */
    for (int i = 0; i < chart_ctx->num_channels; i++) {
        const ChartChannel* cur_channel = &chart_ctx->channels[i];

        /* Prevent division by zero if all values in channel are identical */
        float min_value = cur_channel->min_value;
        float max_value = cur_channel->max_value;
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
            const float val_prev = cur_channel->data[idx_prev];
            const float val_cur  = cur_channel->data[idx_cur];

            /* Convert to screen Y coordinates (inverted, 0 at top) */
            const int y_prev =
              framebuffer_height - (int)((val_prev - min_value) * scale);
            const int y_cur =
              framebuffer_height - (int)((val_cur - min_value) * scale);

            /* Draw line segment */
            framebuffer_draw_line(&chart_ctx->framebuffer,
                                  (x - 1) * CHART_POINT_SPACING,
                                  y_prev,
                                  x * CHART_POINT_SPACING,
                                  y_cur,
                                  cur_channel->color);
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
