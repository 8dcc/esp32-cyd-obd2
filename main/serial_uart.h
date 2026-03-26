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
#include <stdint.h>
#include <stddef.h>

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

/*
 * Write 'len' bytes to the previously-initialized UART.
 * Returns the number of bytes written.
 */
size_t serial_uart_write(const uint8_t* data, size_t len);

/*
 * Read up to 'len' bytes from the previously-initialized UART without
 * blocking. Returns the number of bytes read (may be 0 if none available).
 */
size_t serial_uart_read(uint8_t* buf, size_t len);

#endif /* SERIAL_UART_H_ */
