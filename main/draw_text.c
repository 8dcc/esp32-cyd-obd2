/*
 * Copyright 2025 8dcc
 *
 * This file is part of ESP32 CYD OBD2.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "draw_text.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "framebuffer.h"

/*
 * Draw 'str' into 'fb' with its top-left corner at (x, y). Characters outside
 * the font range are skipped but still advance the X position.
 */
static void draw_text_string(const Framebuffer* fb,
                             const FontDef* font,
                             int x,
                             int y,
                             uint32_t color,
                             const char* str) {
    const int bytes_per_row = (font->glyph_width + 7) / 8;

    for (int i = 0; str[i] != '\0'; i++) {
        const int glyph_idx = str[i] - font->first_char;

        if (glyph_idx < 0 || glyph_idx >= font->num_chars) {
            x += font->glyph_width;
            continue;
        }

        const uint8_t* glyph =
          &font->data[glyph_idx * bytes_per_row * font->glyph_height];

        for (int row = 0; row < font->glyph_height; row++) {
            for (int col = 0; col < font->glyph_width; col++) {
                const int byte_idx = col / 8;
                const int bit_idx  = 7 - (col % 8);

                if ((glyph[row * bytes_per_row + byte_idx] >> bit_idx) & 1)
                    framebuffer_set_pixel(fb, x + col, y + row, color);
            }
        }

        x += font->glyph_width + font->char_spacing;
    }
}

void draw_text_centered(const Framebuffer* fb,
                        const FontDef* font,
                        int cx,
                        int cy,
                        uint32_t color,
                        const char* str) {
    const size_t len = strlen(str);
    if (len <= 0)
        return;

    /* Effective width of each glyph/character in pixels */
    const short char_advance = font->glyph_width + font->char_spacing;

    /* Total width taken by the string in pixels */
    const int total_width = len * char_advance - font->char_spacing;

    /* Calculate top left coordinate of the string after centering */
    const int start_x = cx - total_width / 2;
    const int start_y = cy - font->glyph_height / 2;

    draw_text_string(fb, font, start_x, start_y, color, str);
}
