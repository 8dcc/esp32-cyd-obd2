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

#ifndef UTIL_H_
#define UTIL_H_ 1

#include <stddef.h>
#include <stdint.h>

/*
 * Return the compile-time length of an array.
 */
#define LENGTH(ARR) (sizeof(ARR) / sizeof((ARR)[0]))

/*
 * Explicitly mark a symbol as unused, to avoid compilator warnings.
 */
#define UNUSED(SYM) ((void)SYM)

/*
 * Wrappers for ESP-IDF's logging macros, passing the current filename as the
 * TAG argument.
 */
#if !defined(LOG_DISABLE)
#include "esp_log.h"
#if !defined(__FILE_NAME__)
#define __FILE_NAME__ __FILE__
#endif /* !defined(__FILE_NAME__) */
#define LOGD(...)          ESP_LOGD(__FILE_NAME__, __VA_ARGS__);
#define LOGI(...)          ESP_LOGI(__FILE_NAME__, __VA_ARGS__);
#define LOGW(...)          ESP_LOGW(__FILE_NAME__, __VA_ARGS__);
#define LOGE(...)          ESP_LOGE(__FILE_NAME__, __VA_ARGS__);
#define HEXDUMP(data, len) hexdump(__FILE_NAME__, data, len);
#endif /* !defined(LOG_DISABLE) */

/*
 * Log a hex dump of 'len' bytes from 'data' at DEBUG level, tagged with 'tag'.
 * Output is formatted as offset, hex bytes, and ASCII representation.
 */
void hexdump(const char* tag, const uint8_t* data, size_t len);

/*----------------------------------------------------------------------------*/

/*
 * Convert an RGB888 (24-bit) color to the RGB565 (16-bit) format expected
 * by the ILI9341 chip, byte-swapped for the SPI DMA transfer.
 */
static inline uint16_t rgb888_to_rgb565(uint32_t rgb888) {
    const uint8_t r = (rgb888 >> 16) & 0xFF;
    const uint8_t g = (rgb888 >> 8) & 0xFF;
    const uint8_t b = rgb888 & 0xFF;

    const uint8_t r5 = (r * 0x1F / 0xFF) & 0x1F;
    const uint8_t g6 = (g * 0x3F / 0xFF) & 0x3F;
    const uint8_t b5 = (b * 0x1F / 0xFF) & 0x1F;

    const uint16_t little_endian = (b5 << 11) | (g6 << 5) | r5;
    return (little_endian >> 8) | (little_endian << 8);
}

#endif /* UTIL_H_ */
