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

import serial
import time
import math
import argparse
import sys
import random

BAUD_RATE = 115200

class DataGenerator:
    def __init__(self, num_channels):
        self.num_channels = num_channels

    # Generate sine wave test data with different phases for each channel,
    # receiving the current sample index.
    def generate_sine_waves(self, sample_index, amplitude=50, offset=50):
        values = []
        for ch in range(self.num_channels):
            # Different frequency and phase for each channel
            frequency = 0.01 + (ch * 0.005)
            phase = (ch * math.pi / self.num_channels)
            value = offset + amplitude * math.sin(2 * math.pi * frequency * sample_index + phase)
            values.append(value)
        return values

    # Generate random walk data, gradually drifting values up and down. Receives
    # an array of previous values for each channel.
    def generate_random_walk(self, previous_values, step_size=5, min_val=0, max_val=100):
        if previous_values is None:
            # Initialize at midpoint
            return [50.0] * self.num_channels

        values = []
        for prev in previous_values:
            # Random step up or down
            step = random.uniform(-step_size, step_size)
            new_val = prev + step
            # Clamp to range
            #new_val = max(min_val, min(max_val, new_val))
            values.append(new_val)
        return values

    # Generate sawtooth wave test data.
    def generate_sawtooth(self, sample_index, period=100, amplitude=100):
        values = []
        for ch in range(self.num_channels):
            # Offset each channel's phase
            offset_index = sample_index + (ch * period // self.num_channels)
            value = (offset_index % period) / period * amplitude
            values.append(value)
        return values


def main():
    parser = argparse.ArgumentParser(
        description='Test the ESP32 CYD serial data plotter with whitespace-separated data'
    )
    parser.add_argument(
        '-p', '--port',
        default='/dev/ttyUSB0',
        help='Serial port (default: /dev/ttyUSB0)'
    )
    parser.add_argument(
        '-c', '--channels',
        type=int,
        default=4,
        help='Number of data channels (default: 4)'
    )
    parser.add_argument(
        '-r', '--rate',
        type=float,
        default=10.0,
        help='Update rate in Hz (default: 10.0)'
    )
    parser.add_argument(
        '-m', '--mode',
        choices=['sine', 'random', 'sawtooth'],
        default='sine',
        help='Data pattern mode (default: sine)'
    )
    args = parser.parse_args()

    # Calculate delay between samples
    delay = 1.0 / args.rate

    print(f"ESP32 CYD Serial Data Plotter Test")
    print(f"=" * 50)
    print(f"Port:     {args.port}")
    print(f"Baud:     {BAUD_RATE}")
    print(f"Channels: {args.channels}")
    print(f"Rate:     {args.rate} Hz")
    print(f"Mode:     {args.mode}")
    print(f"=" * 50)
    print()

    try:
        # Open serial port
        print(f"Opening {args.port}...", end='', flush=True)
        ser = serial.Serial(args.port, BAUD_RATE, timeout=1)
        print(" OK")

        # Give ESP32 time to initialize
        print("Waiting 2 seconds for ESP32 to initialize...", end='', flush=True)
        time.sleep(2)
        print(" OK")

        print(f"\nSending whitespace-separated data (Ctrl+C to stop)...")
        print()

        sample_index = 0
        previous_values = None

        # FIXME: Last added. Simplify above. Simplify generation functions.
        data_generator = DataGenerator(args.channels)

        while True:
            # Generate data based on mode
            if args.mode == 'sine':
                values = data_generator.generate_sine_waves(sample_index)
            elif args.mode == 'random':
                values = data_generator.generate_random_walk(previous_values)
                previous_values = values
            elif args.mode == 'sawtooth':
                values = data_generator.generate_sawtooth(sample_index)

            # Format as whitespace-separated values
            data_line = ' '.join([f'{v:.2f}' for v in values]) + '\n'

            # Send to serial
            ser.write(data_line.encode('ascii'))

            # Print to console every 10 samples
            if sample_index % 10 == 0:
                print(f"Sample {sample_index:5d}: {data_line.strip()}")

            sample_index += 1

            # Delay for specified rate
            time.sleep(delay)

    except serial.SerialException as e:
        print(f"\nSerial error: {e}", file=sys.stderr)
        return 1

    except KeyboardInterrupt:
        print("\n\nStopped by user (Ctrl+C)")

    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Serial port closed.")

    print(f"\nTotal samples sent: {sample_index}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
