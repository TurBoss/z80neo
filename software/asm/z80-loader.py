#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""Send an Intel HEX file to the z80neo over serial for UART loading."""
# Usage: z80-loader.py <file> [serial_port]

import sys
import time

import serial

BAUD = 115200
BYTES_PER_LINE = 16  # 16 data bytes per Intel HEX record
LINE_DELAY = 0.005  # 5 ms between lines (give Pico time to parse)

# --- Try common Pico serial ports
COMMON_PORTS = [
    "/dev/ttyACM0",  # USB CDC
    "/dev/ttyACM1",
    "/dev/ttyUSB0",  # USB-to-serial adapter / debug probe
    "/dev/ttyUSB1",
    "/dev/ttyAMA0",  # built-in UART
    "/dev/serial0",
]


def intel_hex_line(addr, data):
    """Return one Intel HEX data record as a bytes object."""
    count = len(data)
    record = bytes([count, (addr >> 8) & 0xFF, addr & 0xFF, 0x00]) + data
    checksum = (-sum(record)) & 0xFF
    line = f":{record.hex().upper()}{checksum:02X}\n"
    return line.encode("ascii")


def find_port():
    """Try common ports, return the first one that opens."""
    import os

    for port in COMMON_PORTS:
        if os.path.exists(port):
            return port
    return None


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <file> [serial_port]")
        print(f"Common ports: {', '.join(COMMON_PORTS)}")
        sys.exit(1)

    filename = sys.argv[1]

    # Determine serial port
    if len(sys.argv) >= 3:
        port = sys.argv[2]
    else:
        port = find_port()
        if port is None:
            print("No serial port found. Specify one as second argument.")
            print(f"Tried: {', '.join(COMMON_PORTS)}")
            sys.exit(1)

    print(f"Loading: {filename}")
    print(f"Serial port: {port}")

    with open(filename, "rb") as f:
        raw = f.read()

    # Auto-detect: look for ':' as first non-whitespace character
    stripped = raw.lstrip()
    is_hex = stripped[:1] == b":"

    if is_hex:
        print(f"Intel HEX file, {len(raw)} bytes raw")
    else:
        print(f"Binary file, {len(raw)} bytes")
        if len(raw) > 0x4000:
            print("ERROR: Program exceeds 16 KB RAM!")
            sys.exit(1)

    with serial.Serial(port, BAUD, timeout=1) as sp:
        print(f"Connected to {sp.portstr}")
        time.sleep(0.5)

        if is_hex:
            lines = raw.decode("ascii").splitlines()
            for line in lines:
                line = line.strip()
                if not line:
                    continue
                sp.write((line + "\n").encode("ascii"))
                sp.flush()
                time.sleep(LINE_DELAY)
            print(f"Sent {len(lines)} lines.")
        else:
            for addr in range(0, len(raw), BYTES_PER_LINE):
                chunk = raw[addr : addr + BYTES_PER_LINE]
                sp.write(intel_hex_line(addr, chunk))
                sp.flush()
                time.sleep(LINE_DELAY)
            sp.write(b":00000001FF\n")
            sp.flush()
            print(f"Sent {len(raw)} bytes in {len(raw) // BYTES_PER_LINE + 1} lines.")

        print("Done!")


if __name__ == "__main__":
    main()
