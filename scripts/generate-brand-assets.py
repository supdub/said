#!/usr/bin/env python3
"""Generate the checked-in VoiceKey PNG/ICO assets from the vector geometry."""

from __future__ import annotations

import binascii
import math
import pathlib
import struct
import zlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
SUPERSAMPLE = 4
VERMILION = (226, 75, 50, 255)
PAPER = (252, 250, 245, 255)


def inside_round_rect(x: float, y: float, left: float, top: float,
                      right: float, bottom: float, radius: float) -> bool:
    nearest_x = min(max(x, left + radius), right - radius)
    nearest_y = min(max(y, top + radius), bottom - radius)
    dx = x - nearest_x
    dy = y - nearest_y
    return dx * dx + dy * dy <= radius * radius


def inside_capsule(x: float, y: float, x1: float, y1: float,
                   x2: float, y2: float, width: float) -> bool:
    vx, vy = x2 - x1, y2 - y1
    wx, wy = x - x1, y - y1
    length_sq = vx * vx + vy * vy
    t = 0.0 if length_sq == 0 else min(1.0, max(0.0, (wx * vx + wy * vy) / length_sq))
    dx, dy = x - (x1 + t * vx), y - (y1 + t * vy)
    return dx * dx + dy * dy <= (width / 2.0) ** 2


def render(size: int) -> bytes:
    scale = size / 512.0
    ss = SUPERSAMPLE
    pixels = bytearray(size * size * 4)
    samples = ss * ss

    for py in range(size):
        for px in range(size):
            counts = [0, 0, 0, 0]
            for sy in range(ss):
                for sx in range(ss):
                    x = (px + (sx + 0.5) / ss) / scale
                    y = (py + (sy + 0.5) / ss) / scale
                    color = (0, 0, 0, 0)
                    if inside_round_rect(x, y, 32, 32, 480, 480, 116):
                        color = VERMILION
                    if (inside_capsule(x, y, 132, 218, 204, 218, 44)
                            or inside_capsule(x, y, 132, 294, 248, 294, 44)
                            or inside_capsule(x, y, 322, 146, 322, 366, 48)):
                        color = PAPER
                    for channel in range(4):
                        counts[channel] += color[channel]
            offset = (py * size + px) * 4
            pixels[offset:offset + 4] = bytes(round(value / samples) for value in counts)
    return bytes(pixels)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)


def encode_png(size: int, rgba: bytes) -> bytes:
    rows = bytearray()
    stride = size * 4
    for y in range(size):
        rows.append(0)
        rows.extend(rgba[y * stride:(y + 1) * stride])
    signature = b"\x89PNG\r\n\x1a\n"
    header = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)
    return signature + png_chunk(b"IHDR", header) + png_chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + png_chunk(b"IEND", b"")


def encode_bmp(width: int, height: int, rgb: bytes) -> bytes:
    row_size = (width * 3 + 3) & ~3
    image = bytearray()
    for y in range(height - 1, -1, -1):
        for x in range(width):
            offset = (y * width + x) * 3
            red, green, blue = rgb[offset:offset + 3]
            image.extend((blue, green, red))
        image.extend(b"\0" * (row_size - width * 3))
    data_offset = 14 + 40
    file_header = b"BM" + struct.pack("<IHHI", data_offset + len(image), 0, 0, data_offset)
    info_header = struct.pack("<IIIHHIIIIII", 40, width, height, 1, 24, 0, len(image), 2835, 2835, 0, 0)
    return file_header + info_header + bytes(image)


def installer_bitmaps() -> None:
    def canvas(width: int, height: int, color: tuple[int, int, int]) -> bytearray:
        return bytearray(color * (width * height))

    def paste_rgb(target: bytearray, width: int, height: int, source: bytes,
                  source_size: int, left: int, top: int) -> None:
        for y in range(source_size):
            if not 0 <= top + y < height:
                continue
            for x in range(source_size):
                if not 0 <= left + x < width:
                    continue
                source_offset = (y * source_size + x) * 4
                target_offset = ((top + y) * width + left + x) * 3
                alpha = source[source_offset + 3] / 255.0
                for channel in range(3):
                    target[target_offset + channel] = round(
                        source[source_offset + channel] * alpha
                        + target[target_offset + channel] * (1.0 - alpha)
                    )

    welcome = canvas(164, 314, (22, 23, 22))
    paste_rgb(welcome, 164, 314, render(104), 104, 30, 38)
    # A quiet oversized voice-cursor echo at the foot of the panel.
    for y in range(224, 288):
        for x in range(106, 114):
            if (x - 110) ** 2 + min(0, y - 228) ** 2 <= 16 and min(0, 284 - y) ** 2 + (x - 110) ** 2 <= 16:
                offset = (y * 164 + x) * 3
                welcome[offset:offset + 3] = bytes((240, 100, 73))
    for left, right, y in ((40, 72, 246), (40, 86, 268)):
        for py in range(y - 4, y + 4):
            for px in range(left, right):
                offset = (py * 164 + px) * 3
                welcome[offset:offset + 3] = bytes((240, 100, 73))
    (ASSETS / "installer-welcome.bmp").write_bytes(encode_bmp(164, 314, bytes(welcome)))

    header = canvas(150, 57, (252, 250, 245))
    paste_rgb(header, 150, 57, render(40), 40, 101, 8)
    for y in range(53, 57):
        for x in range(0, 88):
            offset = (y * 150 + x) * 3
            header[offset:offset + 3] = bytes((226, 75, 50))
    (ASSETS / "installer-header.bmp").write_bytes(encode_bmp(150, 57, bytes(header)))


def main() -> None:
    ASSETS.mkdir(parents=True, exist_ok=True)
    images = []
    for size in SIZES:
        png = encode_png(size, render(size))
        images.append((size, png))
        if size in (64, 256):
            (ASSETS / f"voicekey-{size}.png").write_bytes(png)

    header_size = 6 + len(images) * 16
    cursor = header_size
    entries = []
    payload = bytearray()
    for size, png in images:
        encoded_size = 0 if size == 256 else size
        entries.append(struct.pack("<BBBBHHII", encoded_size, encoded_size, 0, 0, 1, 32, len(png), cursor))
        payload.extend(png)
        cursor += len(png)

    ico = struct.pack("<HHH", 0, 1, len(images)) + b"".join(entries) + bytes(payload)
    (ASSETS / "voicekey.ico").write_bytes(ico)
    installer_bitmaps()
    print(f"Generated {ASSETS / 'voicekey.ico'} ({len(ico):,} bytes)")


if __name__ == "__main__":
    main()
