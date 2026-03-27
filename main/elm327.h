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

#ifndef ELM327_H_
#define ELM327_H_ 1

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* Function pointer types for the transport layer */
typedef size_t (*elm327_write_fn)(const uint8_t* data, size_t len);
typedef size_t (*elm327_read_fn)(uint8_t* buf, size_t len);

/*
 * Initialize the ELM327 adapter by sending the standard AT init sequence
 * through the provided 'write' and 'read' transport functions. Returns true
 * if all commands in the sequence were acknowledged successfully.
 */
bool elm327_init(elm327_write_fn write, elm327_read_fn read);

#endif /* ELM327_H_ */
