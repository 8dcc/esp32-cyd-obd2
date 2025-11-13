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
 * Clamp the specified number N to a minimum and maximum value.
 */
#define CLAMP(N, MIN, MAX)                                                     \
    (((MIN) >= (N)) ? (MIN) : ((N) >= (MAX)) ? (MAX) : (N))

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
 * Convert RGB888 (24-bit) color to RGB565 (16-bit) format.
 *
 * RGB888 format:
 *   - Red:   bits [23..16]
 *   - Green: bits [15..8]
 *   - Blue:  bits [7..0]
 *
 * RGB565 format:
 *   - Red:   bits [15..11]
 *   - Green: bits [10..5]
 *   - Blue:  bits [4..0]
 */
static inline uint16_t rgb888_to_rgb565(uint32_t rgb888) {
    const uint8_t r = (rgb888 >> 16) & 0xFF;
    const uint8_t g = (rgb888 >> 8) & 0xFF;
    const uint8_t b = rgb888 & 0xFF;

    /* Scale [00..FF] range to [00..1F] or [00..3F] */
    const uint16_t r5 = (r * 0x1F / 0xFF) & 0x1F;
    const uint16_t g6 = (g * 0x3F / 0xFF) & 0x3F;
    const uint16_t b5 = (b * 0x1F / 0xFF) & 0x1F;

    return (r5 << 11) | (g6 << 5) | b5;
}

/*----------------------------------------------------------------------------*/

void render_init(RenderCtx* ctx, size_t width, size_t height) {
    ctx->width     = width;
    ctx->height    = height;
    ctx->lcd_panel = NULL;

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
        .dc_gpio_num       = LCD_DC,
        .cs_gpio_num       = LCD_CS,
        .pclk_hz           = LCD_PIXEL_CLK,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
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

    /*
     * Allocate framebuffer for off-screen rendering in DMA-capable memory.
     * This allows all drawing operations to occur in RAM, then the entire
     * frame can be transferred to the LCD in a single DMA transaction.
     */
    const size_t fb_size = ctx->width * ctx->height * sizeof(uint16_t);
    ctx->framebuffer =
      heap_caps_malloc(fb_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);

    if (ctx->framebuffer == NULL) {
        fprintf(stderr,
                "Failed to allocate %zux%zu framebuffer (%zu bytes)\n",
                ctx->width,
                ctx->height,
                fb_size);
        abort();
    }

    /* Initialize framebuffer to black */
    memset(ctx->framebuffer, 0x00, fb_size);
}

void render_clear(const RenderCtx* ctx) {
    /* Clear the framebuffer to black. This is a fast in-memory operation. */
    memset(ctx->framebuffer, 0x00, ctx->width * ctx->height * sizeof(uint16_t));
}

void render_draw_line(const RenderCtx* ctx,
                      int x0,
                      int y0,
                      int x1,
                      int y1,
                      uint32_t color) {
    /* Convert RGB888 to RGB565 color, used by the display */
    const uint16_t rgb565_color = rgb888_to_rgb565(color);

    /* Clamp the coordinates, to ensure they are within screen bounds */
    x0 = CLAMP(x0, 0, ctx->width - 1);
    y0 = CLAMP(y0, 0, ctx->height - 1);
    x1 = CLAMP(x1, 0, ctx->width - 1);
    y1 = CLAMP(y1, 0, ctx->height - 1);

    /* Calculate absolute differences and step directions */
    const int dx = abs(x1 - x0);       /* Horizontal distance */
    const int dy = abs(y1 - y0);       /* Vertical distance */
    const int sx = (x0 < x1) ? 1 : -1; /* Step direction for X */
    const int sy = (y0 < y1) ? 1 : -1; /* Step direction for Y */
    int err      = dx - dy;            /* Initial error term */

    for (;;) {
        /* Write pixel directly to framebuffer */
        ctx->framebuffer[ctx->width * y0 + x0] = rgb565_color;

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

void render_flush(const RenderCtx* ctx) {
    /*
     * Transfer the entire framebuffer to the LCD in a single DMA transaction.
     * This is much faster than individual pixel updates (~100x improvement).
     */
    esp_lcd_panel_draw_bitmap(ctx->lcd_panel,
                              0,
                              0,
                              ctx->width,
                              ctx->height,
                              ctx->framebuffer);
}
