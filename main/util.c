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

#include "util.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"

void debug_hexdump(const char* tag, const uint8_t* data, size_t len) {
    /*
     * Line format (16 bytes per row):
     *   "XX XX ... XX    ................"
     */
    enum { COLS = 16 };

    for (size_t row = 0; row < len; row += COLS) {
        char line[80];
        int pos = 0;

        /* Hex bytes, pad incomplete rows with spaces */
        for (size_t col = 0; col < COLS; col++) {
            if (row + col < len)
                pos += snprintf(line + pos,
                                sizeof(line) - pos,
                                "%02X ",
                                data[row + col]);
            else
                pos += snprintf(line + pos, sizeof(line) - pos, "   ");
        }

        /* Separator and ASCII */
        pos += snprintf(line + pos, sizeof(line) - pos, "   ");
        for (size_t col = 0; col < COLS && row + col < len; col++) {
            const uint8_t b = data[row + col];
            pos += snprintf(line + pos,
                            sizeof(line) - pos,
                            "%c",
                            (b >= 0x20 && b < 0x7F) ? (char)b : '.');
        }

        ESP_LOGD(tag, "%s", line);
    }
}
