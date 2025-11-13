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

#include "serial_uart.h"
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

#include "driver/uart.h"

#include "util.h"

/*
 * Serial communication configuration.
 *
 * UART_NUM_0 is connected to the USB port via the CH340 USB-to-UART bridge
 * chip on the ESP32-CYD board.
 */
#define SERIAL_UART_NUM           UART_NUM_0
#define SERIAL_UART_BAUD_RATE     115200
#define SERIAL_UART_BUF_SIZE      1024
#define SERIAL_UART_RX_BUF_SIZE   (SERIAL_UART_BUF_SIZE * 2)
#define SERIAL_UART_RX_TIMEOUT_MS 20

/*----------------------------------------------------------------------------*/

void serial_uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate  = SERIAL_UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_param_config(SERIAL_UART_NUM, &uart_config);
    uart_set_pin(SERIAL_UART_NUM,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
    uart_driver_install(SERIAL_UART_NUM,
                        SERIAL_UART_RX_BUF_SIZE,
                        0,
                        0,
                        NULL,
                        0);
}

bool serial_uart_read_value(float* dst) {
    /* Buffer used to store digits of the input string */
    static char digit_buffer[64];
    size_t digit_buffer_pos = 0;

    const int uart_timeout_ticks =
      SERIAL_UART_RX_TIMEOUT_MS / portTICK_PERIOD_MS;

    for (;;) {
        /*
         * Read data from UART, one byte at a time, with a maximum timeout.
         *
         * TODO: Is the timeout even needed, if we just 'continue' on failure?
         */
        uint8_t byte;
        const int len =
          uart_read_bytes(SERIAL_UART_NUM, &byte, 1, uart_timeout_ticks);
        if (len <= 0)
            continue;

        /*
         * Check if we reached a value delimiter (space, newline, etc.). If they
         * appear before the value, ignore them and keep reading. If they appear
         * after the value, we are done.
         */
        if (isspace(byte)) {
            if (digit_buffer_pos == 0)
                continue;
            else
                break;
        }

        /*
         * If we are still trying to read a value, but there is no space left on
         * the buffer, abort.
         */
        if (digit_buffer_pos >= LENGTH(digit_buffer) - 1)
            return false;

        /* Store the current digit in the buffer */
        digit_buffer[digit_buffer_pos++] = byte;
    }

    /* Null-terminate the read number */
    digit_buffer[digit_buffer_pos] = '\0';

    /* Convert the string to a double */
    errno = 0;
    char* endptr;
    const float result = strtod(digit_buffer, &endptr);

    /* Check if the type conversion failed */
    if (endptr == digit_buffer || errno == ERANGE)
        return false;

    *dst = result;
    return true;
}
