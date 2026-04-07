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
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SERIAL_BLUETOOTH_H_
#define SERIAL_BLUETOOTH_H_ 1

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/*
 * Initialize the Bluetooth stack and register SPP callbacks.
 * Must be called once before any other serial_bt_* function.
 * Returns true on success, false on failure.
 */
bool serial_bt_init(void);

/*
 * Connect to a remote Bluetooth device by MAC address.
 * Blocks until the SPP connection is established or a timeout occurs.
 * Returns true on success, false on timeout or error.
 */
bool serial_bt_connect(const uint8_t mac[6]);

/*
 * Returns true if the SPP connection is currently alive.
 */
bool serial_bt_is_connected(void);

/*
 * Write 'len' bytes over the SPP connection.
 * Returns the number of bytes written.
 */
size_t serial_bt_write(const uint8_t* data, size_t len);

/*
 * Read up to 'len' bytes from the internal receive buffer (non-blocking).
 * Returns the number of bytes read (may be 0 if none available).
 */
size_t serial_bt_read(uint8_t* buf, size_t len);

/*
 * Read up to 'len' bytes, blocking until at least one byte arrives or
 * 'timeout_ms' milliseconds elapse. Returns the number of bytes read.
 */
size_t serial_bt_read_blocking(uint8_t* buf, size_t len, uint32_t timeout_ms);

/*
 * Disconnect from the remote device and release Bluetooth resources.
 */
void serial_bt_deinit(void);

#endif /* SERIAL_BLUETOOTH_H_ */
