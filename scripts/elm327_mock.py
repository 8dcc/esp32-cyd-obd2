#!/usr/bin/env python3
#
# Copyright 2026 8dcc. All Rights Reserved.
#
# This file is part of ESP32 CYD OBD2.
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <https://www.gnu.org/licenses/>.
#
# ------------------------------------------------------------------------------
#
# elm327_mock.py — Fake ELM327 adapter over Bluetooth RFCOMM for testing.
#
# Acts as an RFCOMM server: the board connects to this machine, sends AT init
# commands and OBD2 PID requests, and this script returns plausible static
# responses.

import random
import socket
import sys

RFCOMM_CHANNEL = int(sys.argv[1]) if len(sys.argv) > 1 else 1

# AT command responses. Keys are the exact command strings (without \r).
AT_RESPONSES = {
    "ATZ":   "\r\nELM327 v1.5\r\n",
    "ATE0":  "OK\r\n",
    "ATL0":  "OK\r\n",
    "ATSP0": "OK\r\n",
}

def get_rpm(lo, hi):
    raw = random.randint(lo, hi) * 4
    a, b = (raw >> 8) & 0xFF, raw & 0xFF
    return f"41 0C {a:02X} {b:02X}\r\n"

def get_byte_pid(mode_pid, lo, hi):
    return f"{mode_pid} {random.randint(lo, hi):02X}\r\n"

OBD_RESPONSE_GENERATORS = {
    "010C": lambda: get_rpm(800, 2500),                # 800-2500 RPM
    "010D": lambda: get_byte_pid("41 0D", 80,   120),  # 80-120 km/h
    "0111": lambda: get_byte_pid("41 11", 0,   255),   # 0-100% throttle
    "010B": lambda: get_byte_pid("41 0B", 50,  255),   # 50-255 kPa intake
    "0104": lambda: get_byte_pid("41 04", 0,   255),   # 0-100% engine load
    "010F": lambda: get_byte_pid("41 0F", 20,  100),   # -20 to 60°C intake temp
    "0105": lambda: get_byte_pid("41 05", 60,  140),   # 20-100°C coolant temp
}

# ------------------------------------------------------------------------------

def handle_command(cmd):
    """Return the ELM327 response string for the given command."""
    cmd = cmd.strip()
    if cmd in AT_RESPONSES:
        return AT_RESPONSES[cmd] + ">"
    if cmd in OBD_RESPONSE_GENERATORS:
        return OBD_RESPONSE_GENERATORS[cmd]() + ">"
    if cmd.startswith("AT"):
        return "?\r\n>"
    return "NO DATA\r\n>"


def serve(client, max_buf=512):
    """Read commands from the connected client and send responses."""
    buf = b""
    while True:
        try:
            chunk = client.recv(256)
        except OSError:
            break

        if not chunk:
            break

        buf += chunk
        if len(buf) > max_buf:
            print("[warn] receive buffer exceeded limit, dropping connection")
            break

        # Commands are terminated by \r
        while b"\r" in buf:
            cmd_bytes, buf = buf.split(b"\r", 1)
            cmd = cmd_bytes.decode("ascii", errors="replace")
            response = handle_command(cmd)
            print(f"  CMD: {cmd!r}  →  {response!r}")
            try:
                client.sendall(response.encode("ascii"))
            except OSError:
                return

# ------------------------------------------------------------------------------

def main():
    server = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("00:00:00:00:00:00", RFCOMM_CHANNEL))
    server.listen(1)
    print(f"ELM327 mock listening on RFCOMM channel {RFCOMM_CHANNEL}...")

    try:
        client, addr = server.accept()
    except KeyboardInterrupt:
        print("\n[aborted]")
        server.close()
        return

    print(f"Board connected: {addr}\n")
    try:
        serve(client)
    except KeyboardInterrupt:
        pass
    finally:
        client.close()
        server.close()

    print("\n[disconnected]")


if __name__ == "__main__":
    main()
