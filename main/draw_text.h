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

#ifndef DRAW_TEXT_H_
#define DRAW_TEXT_H_ 1

#include <stdint.h>

#include "framebuffer.h"

/*
 * Structure representing a bitmap font definition.
 */
typedef struct FontDef {
    /* Width and height of each glyph (character) in the font */
    short glyph_width;
    short glyph_height;

    /*
     * First character included in the 'data' array, and the number of
     * characters included after it.
     */
    char first_char;
    short num_chars;

    /*
     * Array stores glyphs in row-major order, MSB-first. The number of bytes
     * taken by a glyph is:
     *
     *    ceil(glyph_width / 8) * glyph_height
     */
    const uint8_t* data;
} FontDef;

/*----------------------------------------------------------------------------*/

/*
 * Draw 'str' centered horizontally and vertically around (cx, cy) in 'fb',
 * using the specified 'font' and RGB888 'color'.
 *
 * Characters outside the font's supported range are skipped (their space is
 * still advanced).
 */
void draw_text_centered(const Framebuffer* fb,
                        const FontDef* font,
                        int cx,
                        int cy,
                        uint32_t color,
                        const char* str);

#endif /* DRAW_TEXT_H_ */
