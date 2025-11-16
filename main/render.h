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

#include "framebuffer.h"

typedef struct RenderCtx {
    /* Resolution of the physical LCD */
    size_t width, height;

    /* LCD panel handle used to call 'esp_lcd_panel_*' functions */
    esp_lcd_panel_handle_t lcd_panel;

    /*
     * Binary semaphore used to notify 'render_flush' that the LCD drawing is
     * complete. This semaphore will be decreased (waited) from 'render_flush',
     * and increased (signaled) from the DMA callback.
     */
    SemaphoreHandle_t flush_done_semaphore;

    /*
     * Counter tracking pending asynchronous transfers. The DMA callback only
     * signals the semaphore if this counter is > 0, allowing async transfers
     * to complete without affecting the semaphore.
     */
    volatile int pending_async_transfers;
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
 * Clear the display associated to the specified render context, resetting
 * all pixels to black immediately.
 */
void render_clear(RenderCtx* ctx);

/*
 * Synchronously draw the specified framebuffer to the physical LCD associated
 * to the specified render context, at the specified X and Y starting
 * coordinates.
 *
 * Transfers the entire framebuffer to the LCD in a single DMA transaction,
 * while also waiting for the DMA callback, therefore ensuring the caller can't
 * modify/free data that is being processed through DMA.
 */
void render_draw_framebuffer(const RenderCtx* ctx,
                             const Framebuffer* framebuffer,
                             int x,
                             int y);

/*----------------------------------------------------------------------------*/

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
