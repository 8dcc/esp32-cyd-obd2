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

#ifndef FRAMEBUFFER_H_
#define FRAMEBUFFER_H_ 1

#include <stddef.h>
#include <stdint.h>

#include "util.h"

/*
 * Structure representing a framebuffer.
 */
typedef struct Framebuffer {
    /* Resolution of the framebuffer */
    size_t width, height;

    /* Framebuffer data. Each pixel is a 16-bit RGB565 color. */
    uint16_t* data;
} Framebuffer;

/*----------------------------------------------------------------------------*/

void framebuffer_init(Framebuffer* framebuffer, size_t width, size_t height);

/*
 * Deinitialize the specified framebuffer, freeing all its necessary
 * members. This function does not free the 'Framebuffer' structure itself.
 */
void framebuffer_destroy(Framebuffer* framebuffer);

/*
 * Clear the specified framebuffer, resetting all pixels to black.
 */
void framebuffer_clear(const Framebuffer* framebuffer);

/*
 * Draw a line of the specified RGB888 color from (x0, y0) to (x1, y1) in the
 * specified framebuffer.
 *
 * The line is drawn using Bresenham's line algorithm, which is an efficient
 * method for rasterizing lines that uses only integer arithmetic. It determines
 * which pixels should be selected to form a close approximation to a straight
 * line between two points.
 */
void framebuffer_draw_line(const Framebuffer* framebuffer,
                           int x0,
                           int y0,
                           int x1,
                           int y1,
                           uint32_t color);

/*----------------------------------------------------------------------------*/

/*
 * Get the width of the specified framebuffer.
 */
static inline int framebuffer_get_width(const Framebuffer* framebuffer) {
    return framebuffer->width;
}

/*
 * Get the height of the specified framebuffer.
 */
static inline int framebuffer_get_height(const Framebuffer* framebuffer) {
    return framebuffer->height;
}

/*
 * Write one pixel at (x, y) with the specified RGB888 'color' into 'fb'.
 * Out-of-bounds coordinates are silently ignored.
 */
static inline void framebuffer_set_pixel(const Framebuffer* fb,
                                         size_t x,
                                         size_t y,
                                         uint32_t color) {
    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height)
        return;
    fb->data[fb->width * y + x] = rgb888_to_rgb565(color);
}

#endif /* FRAMEBUFFER_H_ */
