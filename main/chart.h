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

#ifndef CHART_H_
#define CHART_H_ 1

#include "render.h"

/*
 * Structure representing the context for a multi-channel scrolling chart.
 */
typedef struct ChartCtx {
    /*
     * Pointer to an array of 'num_channels' arrays, each with 'history_size'
     * values. Each of these arrays will be used as a circular buffer with the
     * actual data to be plotted.
     */
    float* data;
    int num_channels;
    int history_size;

    /* Position in the circular buffer where the next value will be written */
    int write_pos;

    /* Minimum and maximum values in the entire graph, for auto-scaling */
    float min_value;
    float max_value;
} ChartCtx;

/*----------------------------------------------------------------------------*/

/*
 * Initialize the specified chart context, allocating the necessary data for a
 * chart of the specified number of channels, with the specified history size.
 */
void chart_init(ChartCtx* ctx, int num_channels, int history_size);

/*
 * Deinitialize a chart context, freeing its necessary members. This function
 * does not free the 'ChartCtx' structure itself.
 */
void chart_destroy(ChartCtx* ctx);

/*
 * Push a set of values to all channels of the specified chart context. The
 * 'values' argument should point to a float array of 'num_values'
 * elements. This array must contain exactly the number of channels that were
 * specified when calling 'chart_init'.
 */
void chart_push(ChartCtx* ctx, const float* values, int num_values);

/*
 * Update the minimum and maximum stored values of the specified chart context,
 * based on the current data.
 *
 * TODO: Does this need to be exposed? Couldn't it be a static function called
 * by 'chart_render'? Are the 'min_value' and 'max_value' members of 'ChartCtx'
 * even needed?
 */
void chart_update_minmax(ChartCtx* ctx);

/*
 * Render all data in the specified chart context into the display referenced by
 * the specified render context.
 */
void chart_render(const ChartCtx* chart_ctx, const RenderCtx* render_ctx);

#endif /* CHART_H_ */
