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

#ifndef OBD2_H_
#define OBD2_H_ 1

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    OBD_PID_RPM,
    OBD_PID_SPEED,
    OBD_PID_THROTTLE,
    OBD_PID_INTAKE_PRESSURE,
    OBD_PID_ENGINE_LOAD,
    OBD_PID_INTAKE_TEMP,
    OBD_PID_COOLANT_TEMP,
} EObdPid;

/*
 * Write the OBD2 request bytes for the specified 'pid' into 'buf'. The
 * output is not null-terminated. Returns the number of bytes written,
 * or 0 on error.
 */
size_t obd_build_request(EObdPid pid, uint8_t* buf, size_t buf_sz);

/*
 * Decode the ELM327 response in 'buf' for the specified 'pid' into 'out'.
 * Returns true on success, false if the response is malformed or contains
 * an error string.
 */
bool obd_decode_response(EObdPid pid,
                         const uint8_t* buf,
                         size_t len,
                         float* out);

#endif /* OBD2_H_ */
