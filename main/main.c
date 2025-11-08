
#include <stdio.h>

#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"

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

/*
 * Display resolution in pixels.
 * The ILI9341 controller supports 320x240 in landscape orientation.
 */
#define LCD_H_RES 320 /* Horizontal resolution (width) */
#define LCD_V_RES 240 /* Vertical resolution (height) */

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

/*
 * Draw a line from (x0, y0) to (x1, y1) using Bresenham's line algorithm.
 *
 * Bresenham's algorithm is an efficient method for rasterizing lines that uses
 * only integer arithmetic. It determines which pixels should be selected to
 * form a close approximation to a straight line between two points.
 *
 * The algorithm works by tracking an error term that represents the distance
 * between the ideal line and the actual pixel positions, adjusting the path
 * incrementally to minimize this error.
 */
static void draw_line(esp_lcd_panel_handle_t panel,
                      int x0,
                      int y0,
                      int x1,
                      int y1,
                      uint16_t color) {
    /* Calculate absolute differences and step directions */
    int dx  = abs(x1 - x0);       /* Horizontal distance */
    int dy  = abs(y1 - y0);       /* Vertical distance */
    int sx  = (x0 < x1) ? 1 : -1; /* Step direction for X */
    int sy  = (y0 < y1) ? 1 : -1; /* Step direction for Y */
    int err = dx - dy;            /* Initial error term */

    while (1) {
        /* Draw current pixel as a 1x1 bitmap */
        esp_lcd_panel_draw_bitmap(panel, x0, y0, x0 + 1, y0 + 1, &color);

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

static void draw_sine_waves(esp_lcd_panel_handle_t panel_handle) {
    /* Amplitude (max. height) of the wave */
    const int amplitude = 60;

    /* Wave frequency (cycles that fit in the screen) */
    const float frequency = 2.0;

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

    /* Center of the wave vertically */
    const int center_y = LCD_V_RES / 2;

    int prev_x;
    int prev_y_offset;

    /* Draw the normal and inverted-phase sine waves */
    for (int x = 0; x < LCD_H_RES; x++) {
        /*
         * The angle in radians is the current X percentage in the screen
         * divided by the value of the right-most pixel in radians.
         */
        const float angle = (float)x / LCD_V_RES * max_angular_range;

        /* Current Y coordinate relative to the middle */
        const int cur_y_offset = (int)(amplitude * sin(angle));

        if (x > 1) {
            draw_line(panel_handle,
                      prev_x,
                      center_y + prev_y_offset,
                      x,
                      center_y + cur_y_offset,
                      rgb888_to_rgb565(0xFFFFFF));

            /*
             * Also draw inverted angle, essentially shifting the phase 180
             * degrees (i.e. PI radians).
             */
            draw_line(panel_handle,
                      prev_x,
                      center_y - prev_y_offset,
                      x,
                      center_y - cur_y_offset,
                      rgb888_to_rgb565(0xFF0000));
        }

        prev_x        = x;
        prev_y_offset = cur_y_offset;
    }

    /* Draw the normal and inverted-phase cosine waves */
    for (int x = 0; x < LCD_H_RES; x++) {
        const float angle      = (float)x / LCD_V_RES * max_angular_range;
        const int cur_y_offset = (int)(amplitude * cos(angle));

        if (x > 1) {
            draw_line(panel_handle,
                      prev_x,
                      center_y + prev_y_offset,
                      x,
                      center_y + cur_y_offset,
                      rgb888_to_rgb565(0x00FF00));
            draw_line(panel_handle,
                      prev_x,
                      center_y - prev_y_offset,
                      x,
                      center_y - cur_y_offset,
                      rgb888_to_rgb565(0x0000FF));
        }

        prev_x        = x;
        prev_y_offset = cur_y_offset;
    }
}

/*----------------------------------------------------------------------------*/

void app_main(void) {
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
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
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
        .rgb_endian     = LCD_RGB_ENDIAN_BGR,
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

    /* Clear the screen to black by filling the entire display with zeros */
    uint16_t* buffer = malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t));
    memset(buffer, 0x00, LCD_H_RES * LCD_V_RES * sizeof(uint16_t));
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, buffer);
    free(buffer);

    /* Wait for the screen to clear before we start actually drawing. */
    vTaskDelay(pdMS_TO_TICKS(100));

    /*
     * Draw multiple sine and cosine waves with different phases to create an
     * example visualization.
     */
    draw_sine_waves(panel_handle);
}
