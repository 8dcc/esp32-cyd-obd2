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

#include "elm327.h"

#include <assert.h>
#include <string.h>

#include "util.h"

static const struct {
    const char* cmd;
    const char* expected;
    uint32_t timeout_ms;
} INIT_SEQUENCE[] = {
    { "ATZ\r",   "ELM327", 2000 },
    { "ATE0\r",  "OK",     1000 },
    { "ATL0\r",  "OK",     1000 },
    { "ATSP0\r", "OK",     1000 },
};

void elm327_init(Elm327Ctx* ctx, elm327_write_fn write, elm327_read_fn read) {
    assert(ctx != NULL && write != NULL && read != NULL);

    ctx->write = write;
    ctx->read  = read;
}

bool elm327_setup(Elm327Ctx* ctx) {
    assert(ctx != NULL);

    if (ctx->write == NULL || ctx->read == NULL)
        return false;

    uint8_t buf[64];

    for (size_t i = 0; i < LENGTH(INIT_SEQUENCE); i++) {
        const char* cmd        = INIT_SEQUENCE[i].cmd;
        const char* expected   = INIT_SEQUENCE[i].expected;
        const uint32_t timeout = INIT_SEQUENCE[i].timeout_ms;

        const size_t cmd_len = strlen(cmd);
        if (ctx->write((const uint8_t*)cmd, cmd_len) != cmd_len)
            return false;

        const size_t num_read = ctx->read(buf, sizeof(buf) - 1, timeout);
        assert(num_read < sizeof(buf) - 1);
        buf[num_read] = '\0';

        if (strstr((const char*)buf, expected) == NULL) {
            LOGW("Expected response '%s' to ELM327 command '%s', received:",
                 expected,
                 cmd);
            HEXDUMP(buf, num_read);
            return false;
        }
    }

    return true;
}
