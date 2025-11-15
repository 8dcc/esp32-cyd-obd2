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

#ifndef RENDER_H_
#define RENDER_H_ 1

#include <stdint.h>

#include "freertos/FreeRTOS.h" /* SemaphoreHandle_t */
#include "esp_lcd_panel_ops.h" /* esp_lcd_panel_handle_t */

typedef struct RenderCtx {
    /* Resolution of the LCD */
    size_t width, height;

    /* LCD panel handle used to call 'esp_lcd_panel_*' functions */
    esp_lcd_panel_handle_t lcd_panel;

    /* Framebuffer for off-screen rendering (RGB565 format) */
    uint16_t* framebuffer;

    /*
     * Binary semaphore used to notify 'render_flush' that the LCD drawing is
     * complete. This semaphore will be decreased (waited) from 'render_flush',
     * and increased (signaled) from the DMA callback.
     */
    SemaphoreHandle_t flush_done_semaphore;
} RenderCtx;

/*----------------------------------------------------------------------------*/

/*
 * Initialize a 'RenderCtx' structure for an LCD with the specified width and
 * height.
 *
 * This function will:
 *   1. Initialize the LCD backlight GPIO.
 *   2. Initialize the SPI bus for communicating with the LCD.
 *   3. Initialize the ESP LCD panel handle.
 */
void render_init(RenderCtx* ctx, size_t width, size_t height);

/*
 * Deinitialize the specified render context, freeing all its necessary
 * members. This function does not free the 'RenderCtx' structure itself.
 */
void render_destroy(RenderCtx* ctx);

/*
 * Clear the framebuffer associated to the specified render context, resetting
 * all pixels to black.
 *
 * This does not update the physical display; the caller should use
 * 'render_flush' to transfer the framebuffer to the LCD.
 */
void render_clear(const RenderCtx* ctx);

/*
 * Draw a line of the specified RGB888 color from (x0, y0) to (x1, y1) in the
 * framebuffer associated to the specified render context.
 *
 * The line is drawn using Bresenham's line algorithm, which is an efficient
 * method for rasterizing lines that uses only integer arithmetic. It determines
 * which pixels should be selected to form a close approximation to a straight
 * line between two points.
 *
 * This does not update the physical display; the caller should use
 * 'render_flush' to transfer the framebuffer to the LCD.
 */
void render_draw_line(const RenderCtx* ctx,
                      int x0,
                      int y0,
                      int x1,
                      int y1,
                      uint32_t color);

/*
 * Synchronously flush the framebuffer to the physical LCD.
 *
 * Transfers the entire framebuffer to the LCD in a single DMA transaction,
 * while also waiting for the DMA callback, therefore ensuring the caller can't
 * modify/free data that is being processed through DMA.
 */
void render_flush(const RenderCtx* ctx);

/*
 * Get the width of the specified render context.
 */
static inline int render_get_width(const RenderCtx* ctx) {
    return ctx->width;
}

/*
 * Get the height of the specified render context.
 */
static inline int render_get_height(const RenderCtx* ctx) {
    return ctx->height;
}

#endif /* RENDER_H_ */
