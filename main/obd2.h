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

    OBD_NUM_PIDS,
} EObdPid;

/*
 * Return the human-readable name for the specified 'pid'.
 */
const char* obd_pid_name(EObdPid pid);

/*
 * Write the OBD2 request bytes for the specified 'pid' into 'buf'. The
 * output is not null-terminated. Returns the number of bytes written,
 * or 0 on error.
 */
size_t obd_build_request(EObdPid pid, uint8_t* buf, size_t buf_sz);

/*
 * Write a multi-PID OBD2 request for the 'num_pids' entries in 'pids' into
 * 'buf'. The output is not null-terminated. Returns the number of bytes
 * written, or 0 on error.
 *
 * The generated format is: "01 XX XX ...\r", where each XX is the two-digit
 * hex PID code. Requires a CAN-based protocol (ISO 15765-4).
 */
size_t obd_build_multi_request(const EObdPid* pids,
                               size_t num_pids,
                               uint8_t* buf,
                               size_t buf_sz);

/*
 * Decode the ELM327 response in 'buf' for the specified 'pid' into 'out'.
 * Returns true on success, false if the response is malformed or contains
 * an error string.
 */
bool obd_decode_response(EObdPid pid,
                         const uint8_t* buf,
                         size_t len,
                         float* out);

/*
 * Parse a multi-PID ELM327 response in 'buf' for the 'num_pids' entries in
 * 'pids', writing decoded floats into the corresponding slots of 'out'. Lines
 * are matched by PID byte, so response order need not match request order.
 * Returns the number of PIDs successfully decoded.
 */
size_t obd_decode_multi_response(const EObdPid* pids,
                                 size_t num_pids,
                                 const uint8_t* buf,
                                 size_t len,
                                 float* out);

#endif /* OBD2_H_ */
