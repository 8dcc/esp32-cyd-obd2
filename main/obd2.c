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

#include "obd2.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

/*----------------------------------------------------------------------------*/

typedef float (*obd_decode_fn)(uint8_t a, uint8_t b);

/* Forward-declarations for 'PID_TABLE' */
static float decode_rpm(uint8_t a, uint8_t b);
static float decode_speed(uint8_t a, uint8_t b);
static float decode_percent(uint8_t a, uint8_t b);
static float decode_kpa(uint8_t a, uint8_t b);
static float decode_temp(uint8_t a, uint8_t b);

/*----------------------------------------------------------------------------*/

static const struct {
    const char* name;
    const char* request;
    uint8_t num_bytes;
    obd_decode_fn decode;
} PID_TABLE[] = {
    [OBD_PID_RPM]             = { "RPM", "010C\r", 2, decode_rpm },
    [OBD_PID_SPEED]           = { "Speed", "010D\r", 1, decode_speed },
    [OBD_PID_THROTTLE]        = { "Throttle", "0111\r", 1, decode_percent },
    [OBD_PID_INTAKE_PRESSURE] = { "Intake pressure", "010B\r", 1, decode_kpa },
    [OBD_PID_ENGINE_LOAD]     = { "Engine load", "0104\r", 1, decode_percent },
    [OBD_PID_INTAKE_TEMP]     = { "Intake temp", "010F\r", 1, decode_temp },
    [OBD_PID_COOLANT_TEMP]    = { "Coolant temp", "0105\r", 1, decode_temp },
};

static const char* ERROR_STRINGS[] = {
    "NO DATA",
    "ERROR",
    "UNABLE TO CONNECT",
    "?\r",
};

/*----------------------------------------------------------------------------*/

static float decode_rpm(uint8_t a, uint8_t b) {
    return ((a * 256.f) + b) / 4.f; /* RPM */
}

static float decode_speed(uint8_t a, uint8_t b) {
    UNUSED(b);
    return (float)a; /* KM/H */
}

static float decode_percent(uint8_t a, uint8_t b) {
    UNUSED(b);
    return (a * 100.f) / 255.f; /* Percentage */
}

static float decode_kpa(uint8_t a, uint8_t b) {
    UNUSED(b);
    return (float)a; /* kPa */
}

static float decode_temp(uint8_t a, uint8_t b) {
    UNUSED(b);
    return (float)a - 40.f; /* Celsius */
}

/*
 * Parse a two-character hex string (e.g. "1A") into a uint8_t. Returns true
 * on success. Both characters must be valid hex digits.
 */
static bool parse_hex_byte(const char* str, uint8_t* out) {
    if (!isxdigit((unsigned char)str[0]) || !isxdigit((unsigned char)str[1]))
        return false;

    char* end;
    const unsigned long val = strtoul(str, &end, 16);
    if (end != str + 2 || val > 0xFF)
        return false;

    *out = (uint8_t)val;
    return true;
}

/*----------------------------------------------------------------------------*/

const char* obd_pid_name(EObdPid pid) {
    if (pid >= OBD_NUM_PIDS)
        return NULL;
    return PID_TABLE[pid].name;
}

size_t obd_build_request(EObdPid pid, uint8_t* buf, size_t buf_sz) {
    if (pid >= LENGTH(PID_TABLE))
        return 0;

    const char* request      = PID_TABLE[pid].request;
    const size_t request_len = strlen(request);
    if (request_len >= buf_sz)
        return 0;

    memcpy(buf, request, request_len);
    return request_len;
}

size_t obd_build_multi_request(const EObdPid* pids,
                               size_t num_pids,
                               uint8_t* buf,
                               size_t buf_sz) {
    if (pids == NULL || num_pids == 0)
        return 0;

    /* "01" + num_pids * " XX" + "\r" */
    const size_t needed = 2 + num_pids * 3 + 1;
    if (needed >= buf_sz)
        return 0;

    size_t pos = 0;
    buf[pos++] = '0';
    buf[pos++] = '1';

    for (size_t i = 0; i < num_pids; i++) {
        const EObdPid pid = pids[i];
        if (pid >= OBD_NUM_PIDS)
            return 0;

        const char* req = PID_TABLE[pid].request;

        buf[pos++] = ' ';
        buf[pos++] = (uint8_t)req[2];
        buf[pos++] = (uint8_t)req[3];
    }

    buf[pos++] = '\r';
    return pos;
}

bool obd_decode_response(EObdPid pid,
                         const uint8_t* buf,
                         size_t len,
                         float* out) {
    if (pid >= LENGTH(PID_TABLE) || len == 0)
        return false;

    /* Null-terminate a local copy for string operations */
    char str[64];
    if (len >= sizeof(str))
        return false;
    memcpy(str, buf, len);
    str[len] = '\0';

    /* Reject known ELM327 error strings */
    for (size_t i = 0; i < LENGTH(ERROR_STRINGS); i++)
        if (strstr(str, ERROR_STRINGS[i]) != NULL)
            return false;

    /*
     * The expected response format is:
     *
     *     "41 XX [A [B]]\r\n>"
     *
     * Where XX is the PID byte, and A and B are data bytes.
     *
     * First, skip past the "41 " header.
     */
    const char* p = str;
    if (strncmp(p, "41 ", 3) != 0)
        return false;
    p += 3;

    /* Extract the PID byte from the response */
    uint8_t response_pid;
    if (!parse_hex_byte(p, &response_pid))
        return false;

    /* Verify that the PID byte from the response matches the expected one */
    const char* request = PID_TABLE[pid].request;
    uint8_t expected_pid;
    if (!parse_hex_byte(request + 2, &expected_pid))
        return false;
    if (response_pid != expected_pid)
        return false;

    /* Skip PID byte and the adjacent space, pointing to the first data byte */
    p += 2;
    if (*p != ' ')
        return false;
    p++;

    /* Parse data bytes */
    const uint8_t num_bytes = PID_TABLE[pid].num_bytes;
    uint8_t a = 0, b = 0;

    if (!parse_hex_byte(p, &a))
        return false;

    if (num_bytes >= 2) {
        p += 2;
        if (*p != ' ')
            return false;
        p++;
        if (!parse_hex_byte(p, &b))
            return false;
    }

    *out = PID_TABLE[pid].decode(a, b);
    return true;
}

size_t obd_decode_multi_response(const EObdPid* pids,
                                 size_t num_pids,
                                 const uint8_t* buf,
                                 size_t len,
                                 float* out) {
    if (pids == NULL || num_pids == 0 || buf == NULL || len == 0)
        return 0;

    size_t decoded = 0;
    size_t pos     = 0;

    while (pos < len && buf[pos] != '>') {
        const size_t line_start = pos;

        /* Advance to end of line */
        while (pos < len && buf[pos] != '\r' && buf[pos] != '\n' &&
               buf[pos] != '>')
            pos++;
        const size_t line_len = pos - line_start;

        /* Skip line terminators */
        while (pos < len && (buf[pos] == '\r' || buf[pos] == '\n'))
            pos++;

        /* Minimum "41 XX YY" is 8 bytes; skip short or non-response lines */
        if (line_len < 8)
            continue;
        const uint8_t* line = buf + line_start;
        if (line[0] != '4' || line[1] != '1' || line[2] != ' ')
            continue;

        uint8_t response_pid;
        if (!parse_hex_byte((const char*)line + 3, &response_pid))
            continue;

        for (size_t i = 0; i < num_pids; i++) {
            if (pids[i] >= OBD_NUM_PIDS)
                continue;

            uint8_t expected_pid;
            if (!parse_hex_byte(PID_TABLE[pids[i]].request + 2, &expected_pid))
                continue;
            if (expected_pid != response_pid)
                continue;
            if (obd_decode_response(pids[i], line, line_len, &out[i]))
                decoded++;

            break;
        }
    }

    return decoded;
}
