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
typedef size_t (*elm327_read_fn)(uint8_t* buf, size_t len, uint32_t timeout_ms);

/*
 * Context structure for an ELM327 session. The caller declares this and passes
 * it to 'elm327_init', which fills in the transport function pointers. All
 * other 'elm327_*' functions require a pointer to an initialized context.
 */
typedef struct {
    elm327_write_fn write;
    elm327_read_fn read;
} Elm327Ctx;

/*
 * Initialize the 'ctx' structure with the provided 'write' and 'read'
 * transport functions, so it can be passed to subsequent elm327_* calls.
 */
void elm327_init(Elm327Ctx* ctx,
                 elm327_write_fn write,
                 elm327_read_fn read);

/*
 * Send the standard AT setup sequence through the transport functions stored
 * in 'ctx'. Returns true if all commands in the sequence were acknowledged
 * successfully.
 */
bool elm327_setup(Elm327Ctx* ctx);

/*
 * Write 'len' bytes using the transport function stored in 'ctx'. Returns the
 * number of bytes written.
 */
size_t elm327_write(const Elm327Ctx* ctx, const uint8_t* data, size_t len);

/*
 * Read a full ELM327 response (up to and including the '>' prompt) into 'buf',
 * blocking until the prompt is received or 'timeout_ms' elapses. Null bytes
 * (which the ELM327 may occasionally insert) are silently discarded. Returns
 * the number of bytes stored in 'buf'.
 */
size_t elm327_read_response(const Elm327Ctx* ctx,
                            uint8_t* buf,
                            size_t len,
                            uint32_t timeout_ms);

#endif /* ELM327_H_ */
