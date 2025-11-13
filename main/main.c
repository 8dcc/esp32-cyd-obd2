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
#include <math.h> /* sin, cos, M_PI */

#include "render.h"

/*
 * Display resolution in pixels.
 * The ILI9341 controller supports 320x240 in landscape orientation.
 */
#define LCD_WIDTH  320 /* Horizontal resolution */
#define LCD_HEIGHT 240 /* Vertical resolution */

/*----------------------------------------------------------------------------*/

/*
 * Draw sine and cosine wave visualization on the display, using the specified
 * render context.
 *
 * The width and the height of the display must be specified, as well as the
 * wave amplitude (max. height) and the frequency (number of cycles to show on
 * the screen).
 */
static void draw_sine_waves(const RenderCtx* render_ctx,
                            int width,
                            int height,
                            int amplitude,
                            int frequency) {
    /* Vertical center of the screen */
    const int center_y = height / 2;

    /* Number of radians that fit in a 360º cycle */
    const float radians_per_cycle = M_PI * 2.0;

    /*
     * Total radians that "fit" in the screen, that is, the angle of the
     * right-most pixel of the screen in radians.
     *
     * If the frequency is 5, then 5 cycles fit in the screen, or
     * (5*PI*2=31.4) radians, or (31.4*180/PI=1800) degrees, or (5*360=1800)
     * degrees, or (1800*PI/180=31.4) radians.
     */
    const float max_angular_range = frequency * radians_per_cycle;

    int prev_x;
    int prev_y_offset;

    /* Draw the normal and inverted-phase sine waves */
    for (int x = 0; x < width; x++) {
        /*
         * The angle in radians is the current X percentage in the screen
         * divided by the value of the right-most pixel in radians.
         */
        const float angle = (float)x / width * max_angular_range;

        /* Current Y coordinate relative to the middle */
        const int cur_y_offset = (int)(amplitude * sin(angle));

        if (x > 1) {
            render_draw_line(render_ctx,
                             prev_x,
                             center_y + prev_y_offset,
                             x,
                             center_y + cur_y_offset,
                             0xFFFFFF);

            /*
             * Also draw inverted angle, essentially shifting the phase 180
             * degrees (i.e. PI radians).
             */
            render_draw_line(render_ctx,
                             prev_x,
                             center_y - prev_y_offset,
                             x,
                             center_y - cur_y_offset,
                             0xFF0000);
        }

        prev_x        = x;
        prev_y_offset = cur_y_offset;
    }

    /* Draw the normal and inverted-phase cosine waves */
    for (int x = 0; x < width; x++) {
        const float angle      = (float)x / width * max_angular_range;
        const int cur_y_offset = (int)(amplitude * cos(angle));

        if (x > 1) {
            render_draw_line(render_ctx,
                             prev_x,
                             center_y + prev_y_offset,
                             x,
                             center_y + cur_y_offset,
                             0x00FF00);
            render_draw_line(render_ctx,
                             prev_x,
                             center_y - prev_y_offset,
                             x,
                             center_y - cur_y_offset,
                             0x0000FF);
        }

        prev_x        = x;
        prev_y_offset = cur_y_offset;
    }
}

/*----------------------------------------------------------------------------*/

/*
 * ESP-IDF entry point.
 */
void app_main(void) {
    RenderCtx render_ctx;
    render_init(&render_ctx, LCD_WIDTH, LCD_HEIGHT);
    render_clear(&render_ctx);

    /* Amplitude (max. height) of the wave */
    const int amplitude = 60;

    /* Wave frequency (cycles that fit in the screen) */
    const float frequency = 2.0;

    /*
     * Draw multiple sine and cosine waves with different phases to create an
     * example visualization.
     */
    draw_sine_waves(&render_ctx, LCD_WIDTH, LCD_HEIGHT, amplitude, frequency);

    /*
     * Flush the framebuffer to the LCD, since the drawing functions operate on
     * an off-screen framebuffer.
     */
    render_flush(&render_ctx);

    /* Free the render context */
    render_destroy(&render_ctx);
}
