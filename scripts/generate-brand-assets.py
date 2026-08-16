#!/usr/bin/env python3
"""Generate SAID's checked-in monochrome Windows brand assets."""

from __future__ import annotations

import binascii
import pathlib
import struct
import zlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ARTIFACTS = ROOT / "artifacts"
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
SUPERSAMPLE = 4

INK = (21, 22, 19, 255)
RAISED_INK = (36, 37, 34, 255)
BONE = (245, 242, 233, 255)
LIGHT_HAIRLINE = (209, 206, 197, 255)


def inside_round_rect(
    x: float,
    y: float,
    left: float,
    top: float,
    right: float,
    bottom: float,
    radius: float,
) -> bool:
    nearest_x = min(max(x, left + radius), right - radius)
    nearest_y = min(max(y, top + radius), bottom - radius)
    dx = x - nearest_x
    dy = y - nearest_y
    return dx * dx + dy * dy <= radius * radius


def inside_capsule(
    x: float,
    y: float,
    x1: float,
    y1: float,
    x2: float,
    y2: float,
    width: float,
) -> bool:
    vx, vy = x2 - x1, y2 - y1
    wx, wy = x - x1, y - y1
    length_sq = vx * vx + vy * vy
    t = 0.0 if length_sq == 0 else min(1.0, max(0.0, (wx * vx + wy * vy) / length_sq))
    dx, dy = x - (x1 + t * vx), y - (y1 + t * vy)
    return dx * dx + dy * dy <= (width / 2.0) ** 2


def inside_mark(x: float, y: float) -> bool:
    dots = ((126.0, 256.0), (209.0, 256.0), (282.0, 256.0))
    if any((x - cx) ** 2 + (y - cy) ** 2 <= 28.0**2 for cx, cy in dots):
        return True
    return inside_capsule(x, y, 344.0, 140.0, 344.0, 372.0, 34.0)


def render(size: int, *, tray: bool = False) -> bytes:
    """Render the bone dots-to-caret mark on an Ink tile."""
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
                    inside_tile = inside_round_rect(x, y, 28, 28, 484, 484, 96)
                    if inside_tile:
                        color = INK
                    if tray and inside_tile and not inside_round_rect(x, y, 40, 40, 472, 472, 84):
                        color = BONE
                    if inside_mark(x, y):
                        color = BONE
                    for channel in range(4):
                        counts[channel] += color[channel]
            offset = (py * size + px) * 4
            pixels[offset : offset + 4] = bytes(round(value / samples) for value in counts)
    return bytes(pixels)


def render_bare_mark(width: int, height: int, *, inverse: bool) -> bytes:
    """Render a centered mark without a tile for installer artwork."""
    pixels = bytearray(width * height * 4)
    scale = min(width / 512.0, height / 512.0)
    x_offset = (width - 512.0 * scale) / 2.0
    y_offset = (height - 512.0 * scale) / 2.0
    foreground = INK if inverse else BONE
    for py in range(height):
        for px in range(width):
            x = (px - x_offset + 0.5) / scale
            y = (py - y_offset + 0.5) / scale
            if inside_mark(x, y):
                offset = (py * width + px) * 4
                pixels[offset : offset + 4] = bytes(foreground)
    return bytes(pixels)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    checksum = binascii.crc32(kind + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", checksum)


def encode_png(width: int, height: int, rgba: bytes) -> bytes:
    rows = bytearray()
    stride = width * 4
    for y in range(height):
        rows.append(0)
        rows.extend(rgba[y * stride : (y + 1) * stride])
    signature = b"\x89PNG\r\n\x1a\n"
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (
        signature
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(bytes(rows), 9))
        + png_chunk(b"IEND", b"")
    )


def encode_bmp(width: int, height: int, rgb: bytes) -> bytes:
    row_size = (width * 3 + 3) & ~3
    image = bytearray()
    for y in range(height - 1, -1, -1):
        for x in range(width):
            offset = (y * width + x) * 3
            red, green, blue = rgb[offset : offset + 3]
            image.extend((blue, green, red))
        image.extend(b"\0" * (row_size - width * 3))
    data_offset = 14 + 40
    file_header = b"BM" + struct.pack("<IHHI", data_offset + len(image), 0, 0, data_offset)
    info_header = struct.pack(
        "<IIIHHIIIIII", 40, width, height, 1, 24, 0, len(image), 2835, 2835, 0, 0
    )
    return file_header + info_header + bytes(image)


def rgba_over_rgb(target: bytearray, width: int, height: int, source: bytes, source_width: int,
                  source_height: int, left: int, top: int) -> None:
    for y in range(source_height):
        if not 0 <= top + y < height:
            continue
        for x in range(source_width):
            if not 0 <= left + x < width:
                continue
            source_offset = (y * source_width + x) * 4
            target_offset = ((top + y) * width + left + x) * 3
            alpha = source[source_offset + 3] / 255.0
            for channel in range(3):
                target[target_offset + channel] = round(
                    source[source_offset + channel] * alpha
                    + target[target_offset + channel] * (1.0 - alpha)
                )


def installer_bitmaps() -> None:
    def canvas(width: int, height: int, color: tuple[int, int, int]) -> bytearray:
        return bytearray(color * (width * height))

    welcome = canvas(164, 314, INK[:3])
    welcome_mark = render_bare_mark(124, 124, inverse=False)
    rgba_over_rgb(welcome, 164, 314, welcome_mark, 124, 124, 20, 88)
    (ASSETS / "installer-welcome.bmp").write_bytes(encode_bmp(164, 314, bytes(welcome)))

    header = canvas(150, 57, BONE[:3])
    header_mark = render_bare_mark(42, 42, inverse=True)
    rgba_over_rgb(header, 150, 57, header_mark, 42, 42, 100, 7)
    (ASSETS / "installer-header.bmp").write_bytes(encode_bmp(150, 57, bytes(header)))


def encode_ico(images: list[tuple[int, bytes]]) -> bytes:
    header_size = 6 + len(images) * 16
    cursor = header_size
    entries = []
    payload = bytearray()
    for size, png in images:
        encoded_size = 0 if size == 256 else size
        entries.append(
            struct.pack("<BBBBHHII", encoded_size, encoded_size, 0, 0, 1, 32, len(png), cursor)
        )
        payload.extend(png)
        cursor += len(png)
    return struct.pack("<HHH", 0, 1, len(images)) + b"".join(entries) + bytes(payload)


def icon_size_check() -> None:
    """Create the review sheet required for light/dark Windows tray checks."""
    width, height = 420, 148
    pixels = bytearray(BONE[:3] + (255,)) * (width * height)
    for y in range(height // 2, height):
        for x in range(width):
            offset = (y * width + x) * 4
            pixels[offset : offset + 4] = bytes((*RAISED_INK[:3], 255))

    for row, background_y in enumerate((0, height // 2)):
        for column, size in enumerate((16, 20, 24, 32)):
            icon = render(size, tray=True)
            left = 44 + column * 94 + (32 - size) // 2
            top = background_y + (height // 2 - size) // 2
            for y in range(size):
                for x in range(size):
                    source_offset = (y * size + x) * 4
                    target_offset = ((top + y) * width + left + x) * 4
                    alpha = icon[source_offset + 3] / 255.0
                    for channel in range(3):
                        pixels[target_offset + channel] = round(
                            icon[source_offset + channel] * alpha
                            + pixels[target_offset + channel] * (1.0 - alpha)
                        )
                    pixels[target_offset + 3] = 255
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    (ARTIFACTS / "said-tray-size-check.png").write_bytes(encode_png(width, height, bytes(pixels)))


def main() -> None:
    ASSETS.mkdir(parents=True, exist_ok=True)
    app_images: list[tuple[int, bytes]] = []
    tray_images: list[tuple[int, bytes]] = []
    for size in SIZES:
        app_png = encode_png(size, size, render(size))
        tray_png = encode_png(size, size, render(size, tray=True))
        app_images.append((size, app_png))
        tray_images.append((size, tray_png))
        if size in (64, 256):
            (ASSETS / f"said-{size}.png").write_bytes(app_png)

    app_ico = encode_ico(app_images)
    tray_ico = encode_ico(tray_images)
    (ASSETS / "said.ico").write_bytes(app_ico)
    (ASSETS / "said-tray.ico").write_bytes(tray_ico)
    installer_bitmaps()
    icon_size_check()
    print(f"Generated {ASSETS / 'said.ico'} ({len(app_ico):,} bytes)")
    print(f"Generated {ASSETS / 'said-tray.ico'} ({len(tray_ico):,} bytes)")


if __name__ == "__main__":
    main()
