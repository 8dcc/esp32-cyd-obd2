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

#ifndef SERIAL_UART_H_
#define SERIAL_UART_H_ 1

#include <stdbool.h>

/*
 * Initialize UART zero of the ESP for data communication.
 *
 * In the ESP32-CYD, UART zero is connected to the USB port via the CH340
 * USB-to-UART bridge chip.
 */
void serial_uart_init(void);

/*
 * Read a whitespace-separated float value from the previously-initialized UART,
 * and write it to 'dst'. This function returns true on success, or false on
 * failure.
 *
 * Any whitespace is considered a value separator, as matched by the 'isspace'
 * function from the 'ctype.h' header.
 */
bool serial_uart_read_value(float* dst);

#endif /* SERIAL_UART_H_ */
