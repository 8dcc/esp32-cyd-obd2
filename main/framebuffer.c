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

#include "framebuffer.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

/*
 * Clamp the specified number N to a minimum and maximum value.
 */
#define CLAMP(N, MIN, MAX)                                                     \
    (((MIN) >= (N)) ? (MIN) : ((N) >= (MAX)) ? (MAX) : (N))

/*----------------------------------------------------------------------------*/

/*
 * Convert RGB888 (24-bit) color to RGB565 (16-bit) format, as expected by the
 * ILI9341 chip.
 */
static inline uint16_t rgb888_to_rgb565(uint32_t rgb888) {
    const uint8_t r = (rgb888 >> 16) & 0xFF;
    const uint8_t g = (rgb888 >> 8) & 0xFF;
    const uint8_t b = rgb888 & 0xFF;

    /* Scale [00..FF] range to [00..1F] or [00..3F] */
    const uint8_t r5 = (r * 0x1F / 0xFF) & 0x1F;
    const uint8_t g6 = (g * 0x3F / 0xFF) & 0x3F;
    const uint8_t b5 = (b * 0x1F / 0xFF) & 0x1F;

    const uint16_t little_endian = (b5 << 11) | (g6 << 5) | r5;
    return (little_endian >> 8) | (little_endian << 8);
}

/*----------------------------------------------------------------------------*/

void framebuffer_init(Framebuffer* framebuffer, size_t width, size_t height) {
    framebuffer->width  = width;
    framebuffer->height = height;

    /*
     * Allocate framebuffer for off-screen rendering in DMA-capable memory.
     * This allows all drawing operations to occur in RAM, then the entire
     * frame can be transferred to the LCD in a single DMA transaction.
     */
    const size_t fb_size =
      framebuffer->width * framebuffer->height * sizeof(uint16_t);
    framebuffer->data =
      heap_caps_malloc(fb_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

    if (framebuffer->data == NULL) {
        fprintf(stderr,
                "Failed to allocate %zux%zu framebuffer (%zu bytes)\n",
                framebuffer->width,
                framebuffer->height,
                fb_size);
        abort();
    }

    /* Initialize framebuffer to black */
    memset(framebuffer->data, 0x00, fb_size);
}

void framebuffer_destroy(Framebuffer* framebuffer) {
    if (framebuffer->data != NULL) {
        free(framebuffer->data);
        framebuffer->data = NULL;
    }
}

void framebuffer_clear(const Framebuffer* framebuffer) {
    /* Clear the framebuffer to black. This is a fast in-memory operation. */
    memset(framebuffer->data,
           0x00,
           framebuffer->width * framebuffer->height * sizeof(uint16_t));
}

void framebuffer_draw_line(const Framebuffer* framebuffer,
                           int x0,
                           int y0,
                           int x1,
                           int y1,
                           uint32_t color) {
    /* Convert RGB888 to RGB565 color, used by the display */
    const uint16_t rgb565_color = rgb888_to_rgb565(color);

    /* Clamp the coordinates, to ensure they are within screen bounds */
    x0 = CLAMP(x0, 0, framebuffer->width - 1);
    y0 = CLAMP(y0, 0, framebuffer->height - 1);
    x1 = CLAMP(x1, 0, framebuffer->width - 1);
    y1 = CLAMP(y1, 0, framebuffer->height - 1);

    /* Calculate absolute differences and step directions */
    const int dx = abs(x1 - x0);       /* Horizontal distance */
    const int dy = abs(y1 - y0);       /* Vertical distance */
    const int sx = (x0 < x1) ? 1 : -1; /* Step direction for X */
    const int sy = (y0 < y1) ? 1 : -1; /* Step direction for Y */
    int err      = dx - dy;            /* Initial error term */

    for (;;) {
        /* Write pixel directly to framebuffer */
        framebuffer->data[framebuffer->width * y0 + x0] = rgb565_color;

        /* Check if we've reached the endpoint */
        if (x0 == x1 && y0 == y1)
            break;

        /*
         * Calculate error adjustment and step to next pixel.
         * The error term determines whether to step horizontally,
         * vertically, or diagonally.
         */
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx; /* Step horizontally */
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy; /* Step vertically */
        }
    }
}
