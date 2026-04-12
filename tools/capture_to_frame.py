#!/usr/bin/env python3
"""Capture a frame from pico-rgb2hdmi over serial and store it as a binary frame.

Default command payload matches the request from issue: "capture 0\r\n".
The output frame format is a compact custom container:

- 4 bytes magic: b'RGBF'
- u8  version: 1
- u8  pixel format: 1=RGB332, 2=RGB565
- u16 reserved: 0
- u16 width
- u16 height
- u32 payload size in bytes
- u32 crc32 of payload (zlib crc32)
- payload: row-major pixels, little-endian for 16-bit samples
"""

import argparse
import re
import struct
import sys
import time
import zlib
from pathlib import Path

import serial
from serial.tools import list_ports

MAGIC = b"RGBF"
VERSION = 1
PIXFMT_RGB332 = 1
PIXFMT_RGB565 = 2
HEADER_STRUCT = struct.Struct("<4sBBHHHII")

HEADER_RE = re.compile(r"Capture screen:\s*(\d+)x(\d+)@(\d+)bppx", re.IGNORECASE)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Capture a frame over serial and save it as .rgbf")
    parser.add_argument("--port", help="Serial port (example: /dev/ttyACM0 or COM3). If omitted, auto-detect.")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=1.0, help="Serial read timeout in seconds")
    parser.add_argument("--settle-ms", type=int, default=200, help="Delay before sending command")
    parser.add_argument("--command", default="capture 0\r\n", help="Command payload (default: capture 0\\r\\n)")
    parser.add_argument("--output", "-o", default="capture.rgbf", help="Output file path")
    return parser.parse_args()


def autodetect_port() -> str:
    candidates = []
    for p in list_ports.comports():
        text = f"{p.device} {p.description} {p.hwid}".lower()
        if "bluetooth" in text:
            continue
        if any(tag in text for tag in ["usb", "acm", "cp210", "ch340", "tty"]):
            candidates.append(p.device)

    if not candidates:
        raise RuntimeError("No serial candidates found. Provide --port explicitly.")
    return candidates[0]


def read_capture(ser: serial.Serial):
    # Keep reading until the capture header appears.
    header_line = None
    deadline = time.monotonic() + 8.0
    while time.monotonic() < deadline:
        line = ser.readline()
        if not line:
            continue
        text = line.decode("ascii", errors="replace").strip()
        if "Capture screen:" in text:
            header_line = text
            break

    if header_line is None:
        raise RuntimeError("Capture header not found in serial response.")

    match = HEADER_RE.search(header_line)
    if not match:
        raise RuntimeError(f"Could not parse capture header: {header_line!r}")

    width = int(match.group(1))
    height = int(match.group(2))
    bpp = int(match.group(3))

    if bpp == 8:
        pixfmt = PIXFMT_RGB332
    elif bpp == 16:
        pixfmt = PIXFMT_RGB565
    else:
        raise RuntimeError(f"Unsupported bits-per-pixel from device: {bpp}")

    rows = []
    for row_idx in range(height):
        row = ser.readline()
        if not row:
            raise RuntimeError(f"Timed out while reading row {row_idx + 1}/{height}")
        rows.append(row.decode("ascii", errors="replace").strip())

    return width, height, pixfmt, rows


def rows_to_payload(rows, width, pixfmt):
    payload = bytearray()
    for row_idx, row in enumerate(rows):
        if not row:
            raise RuntimeError(f"Empty capture row at index {row_idx}")
        samples = [cell.strip() for cell in row.split(",") if cell.strip() != ""]
        if len(samples) != width:
            raise RuntimeError(
                f"Row {row_idx} has {len(samples)} pixels, expected {width}."
            )
        for sample in samples:
            value = int(sample, 16)
            if pixfmt == PIXFMT_RGB332:
                payload.append(value & 0xFF)
            else:
                payload.extend(struct.pack("<H", value & 0xFFFF))
    return bytes(payload)


def save_frame(output_path: Path, width: int, height: int, pixfmt: int, payload: bytes):
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    header = HEADER_STRUCT.pack(
        MAGIC,
        VERSION,
        pixfmt,
        0,
        width,
        height,
        len(payload),
        crc,
    )
    output_path.write_bytes(header + payload)


def main() -> int:
    args = parse_args()
    port = args.port or autodetect_port()
    out = Path(args.output)

    print(f"Using port: {port}")
    with serial.Serial(port=port, baudrate=args.baud, timeout=args.timeout) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        time.sleep(args.settle_ms / 1000.0)
        ser.write(args.command.encode("ascii"))
        width, height, pixfmt, rows = read_capture(ser)

    payload = rows_to_payload(rows, width, pixfmt)
    save_frame(out, width, height, pixfmt, payload)

    pixfmt_name = "RGB332" if pixfmt == PIXFMT_RGB332 else "RGB565"
    print(f"Captured {width}x{height} {pixfmt_name} -> {out} ({len(payload)} bytes payload)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
