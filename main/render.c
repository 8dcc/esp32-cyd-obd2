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

#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_heap_caps.h" /* MALLOC_CAP_DMA, MALLOC_CAP_8BIT */

/*
 * ESP32-CYD (Cheap Yellow Display) hardware pin definitions.
 * This board uses an ILI9341 LCD controller connected via SPI.
 */
#define LCD_HOST      SPI2_HOST /* SPI peripheral to use */
#define LCD_PIXEL_CLK 40000000  /* 40 MHz SPI clock */
#define LCD_MOSI      13        /* Master Out Slave In data line */
#define LCD_SCLK      14        /* SPI clock line */
#define LCD_CS        15        /* Chip Select (active low) */
#define LCD_DC        2         /* Data/Command selection pin */
#define LCD_RST       -1        /* Reset pin (not used, -1 = disabled) */
#define LCD_BL        21        /* Backlight control pin */

/*----------------------------------------------------------------------------*/

/*
 * Callback used for detecting when the DMA transfer to the LCD screen is done.
 *
 * This function is specified when creating the 'esp_lcd_panel_io_spi_config_t'
 * structure in 'render_init'.
 */
static bool on_lcd_transfer_done(esp_lcd_panel_io_handle_t panel_io,
                                 esp_lcd_panel_io_event_data_t* edata,
                                 void* user_ctx) {
    RenderCtx* ctx             = user_ctx;
    SemaphoreHandle_t sem      = ctx->flush_done_semaphore;
    BaseType_t high_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(sem, &high_task_woken);
    return high_task_woken == pdTRUE;
}

/*
 * Synchronous wrapper for 'esp_lcd_panel_draw_bitmap', which waits for
 * 'flush_done_semaphore' to be increased (signaled) from the
 * 'on_lcd_transfer_done' callback.
 */
static void draw_bitmap_synchronously(const RenderCtx* ctx,
                                      int x0,
                                      int y0,
                                      int x1,
                                      int y1,
                                      const void* data) {
    /* Start the asynchronous DMA transfer from the data buffer to the LCD */
    esp_lcd_panel_draw_bitmap(ctx->lcd_panel, x0, y0, x1, y1, data);

    /*
     * Wait for the counter of the binary semaphore to increase. This counter
     * will be increased from the 'on_lcd_transfer_done' callback.
     */
    xSemaphoreTake(ctx->flush_done_semaphore, portMAX_DELAY);
}

/*----------------------------------------------------------------------------*/

void render_init(RenderCtx* ctx, size_t width, size_t height) {
    ctx->width                = width;
    ctx->height               = height;
    ctx->lcd_panel            = NULL;
    ctx->flush_done_semaphore = xSemaphoreCreateBinary();

    /*
     * Configure the backlight GPIO pin as output and turn it on.
     * The backlight must be enabled for the display to be visible.
     */
    gpio_set_direction(LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_BL, 1);

    /*
     * Initialize the SPI bus for communication with the LCD controller.
     * The ILI9341 uses SPI for both command and data transfer.
     */
    spi_bus_config_t buscfg = {
        .mosi_io_num     = LCD_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = LCD_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = ctx->width * ctx->height * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /*
     * Configure the LCD panel IO layer, which handles the SPI protocol
     * specifics for communicating with the ILI9341 controller (commands,
     * parameters, and data transfer).
     */
    esp_lcd_panel_io_handle_t io_handle     = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num         = LCD_DC,
        .cs_gpio_num         = LCD_CS,
        .pclk_hz             = LCD_PIXEL_CLK,
        .lcd_cmd_bits        = 8,
        .lcd_param_bits      = 8,
        .spi_mode            = 0,
        .trans_queue_depth   = 10,
        .on_color_trans_done = on_lcd_transfer_done,
        .user_ctx            = ctx,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                             &io_config,
                                             &io_handle));

    /*
     * Create and configure the ILI9341 LCD panel driver.
     * This sets up the hardware-specific parameters for the display controller.
     */
    esp_lcd_panel_handle_t panel_handle     = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_endian     = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(
      esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

    /*
     * Initialize and configure the display controller.
     * These operations reset the controller, initialize its internal state,
     * configure the display orientation as landscape, and enable mirroring to
     * match the physical mounting of the LCD on the ESP32-CYD board.
     */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));

    /* Turn on the display (enables output to the LCD panel) */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ctx->lcd_panel = panel_handle;
}

void render_destroy(RenderCtx* ctx) {
    /* TODO: Call ESP-IDF functions for freeing LCD and SPI resources */
}

void render_clear(const RenderCtx* ctx) {
    /* TODO: Clear display immediately, without using too much memory */
}

void render_draw_framebuffer(const RenderCtx* ctx,
                             const Framebuffer* framebuffer,
                             int x,
                             int y) {
    /* Ensure the start coordinates are inside the display */
    assert(x >= 0 && x < ctx->width && y >= 0 && y < ctx->height);

    /*
     * Note that the 'x_end' and 'y_end' arguments of
     * 'esp_lcd_panel_draw_bitmap' are not inclusive, so they can be equal to
     * the display width or height.
     */
    assert(x + framebuffer->width <= ctx->width &&
           y + framebuffer->height <= ctx->height);

    /* Transfer the framebuffer to the LCD, using our synchronous wrapper */
    draw_bitmap_synchronously(ctx,
                              x,
                              y,
                              x + framebuffer->width,
                              y + framebuffer->height,
                              framebuffer->data);
}
