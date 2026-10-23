"""USB controls for DeskDisplay. Uses pyserial; no network connection required."""
import argparse
import re
import struct
import sys
import time
import zlib
from pathlib import Path

import serial
from serial.tools import list_ports

sys.stdout.reconfigure(encoding="utf-8", errors="replace")


def connect(port):
    if not port:
        matches = [p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]
        if len(matches) != 1:
            raise SystemExit(f"Specify --port; found {matches}")
        port = matches[0]
    link = serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=2)
    link.dtr = False
    link.rts = False
    link.port = port
    link.open()
    time.sleep(0.3)
    link.reset_input_buffer()
    return link


def request(link, command, marker=None, timeout=4):
    link.write((command + "\n").encode("ascii"))
    link.flush()
    deadline = time.monotonic() + timeout
    result = ""
    while time.monotonic() < deadline:
        chunk = link.read(max(1, link.in_waiting)).decode("ascii", errors="replace")
        result += chunk
        if marker and marker in result and result.endswith("\n"):
            return result
        if not marker and result and not chunk:
            return result
    if marker:
        raise RuntimeError(f"Timed out waiting for {marker!r}: {result[-1000:]}")
    return result


def save_png(path, rows, scale=8):
    image = bytearray()
    for row in rows:
        rgb = bytearray()
        for x in range(0, len(row), 4):
            value = int(row[x:x + 4], 16)
            r, g, b = (value >> 11) & 31, (value >> 5) & 63, value & 31
            rgb.extend(bytes((r * 255 // 31, g * 255 // 63, b * 255 // 31)) * scale)
        image.extend((b"\x00" + rgb) * scale)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", 64 * scale, 64 * scale, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(image)) + chunk(b"IEND", b"")
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)
    print(f"Saved {path.resolve()}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Auto-detects the ESP32 USB port by default")
    parser.add_argument("command", choices=["status", "sync", "mode", "brightness", "sound", "test", "capture", "verify"])
    parser.add_argument("value", nargs="?")
    args = parser.parse_args()
    with connect(args.port) as link:
        if args.command == "sync":
            print(request(link, f"time {int(time.time())}", "TIME "), end="")
        elif args.command == "capture":
            result = request(link, "capture", "FRAME END", timeout=12)
            rows = re.findall(r"(?m)^([0-9a-f]{256})\r?$", result)
            if len(rows) != 64:
                raise RuntimeError(f"Expected 64 framebuffer rows, got {len(rows)}")
            save_png(args.value or "artifacts/clock.png", rows)
        elif args.command == "verify":
            first = request(link, "status", "STATUS ")
            print(first, end="")
            assert "panel=64x64" in first and "audio=ESP_OK" in first, first
            for mode in ["orbit", "rain", "clock"]:
                assert "MODE OK" in request(link, f"mode {mode}", "MODE ")
                time.sleep(0.5)
                result = request(link, "status", "STATUS ")
                print(result, end="")
                assert f"mode={mode}" in result, result
            assert "ERROR" in request(link, "brightness 101", "ERROR")
            assert "ERROR" in request(link, "mode invalid", "ERROR")
            assert "ERROR" in request(link, "time 0", "ERROR")
            assert "ERROR" in request(link, "sound 86", "ERROR")
            assert "ERROR" in request(link, "x" * 140, "ERROR")
            time.sleep(2)
            final = request(link, "status", "STATUS ")
            print(final, end="")
            frame = lambda text: int(re.search(r"frames=(\d+)", text)[1])
            assert frame(final) > frame(first) + 30, "Display stopped rendering"
            assert "audio=ESP_OK" in final, final
            print("PASS: modes, command bounds, framebuffer progress, audio initialization")
        else:
            command = args.command
            if args.command in ["mode", "brightness"]:
                if args.value is None:
                    parser.error(f"{args.command} requires a value")
                command += " " + args.value
            elif args.command == "sound" and args.value is not None:
                command += " " + args.value
            marker = {"status": "STATUS ", "sound": "CHIME ", "test": "TEST ",
                      "mode": "MODE ", "brightness": "BRIGHTNESS "}[args.command]
            print(request(link, command, marker), end="")


if __name__ == "__main__":
    main()
