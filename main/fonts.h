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

#ifndef FONTS_H_
#define FONTS_H_ 1

#include "draw_text.h"

/*
 * 8x16 pixel bitmap font, covering ASCII 0x20 (' ') through 0x5A ('Z').
 * Each glyph is 16 bytes, one per row. Bit 7 = leftmost pixel.
 */
extern const FontDef FONT_8X16;

#endif /* FONTS_H_ */
