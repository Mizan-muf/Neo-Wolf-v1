#!/usr/bin/env python3
"""Deterministic generator for Neo-Wolf's application icon (Roadmap Phase 9).

Draws the icon from scratch (no third-party artwork, no licence to track): a rounded square
looking down a raycaster corridor (stone walls with perspective seams, ceiling and floor) at a
glowing far wall with a wolf's head in silhouette.

The build calls it (cmake/Packaging.cmake); nothing it writes is committed:

    --iconset DIR   macOS: DIR/icon_16x16.png ... icon_512x512@2x.png, the input of
                    `iconutil -c icns DIR -o NeoWolf.icns` (DIR must end in .iconset)
    --ico FILE      Windows: one .ico with PNG-compressed 16, 24, 32, 48, 64, 128 and 256 px
                    images (PNG-in-ICO, Windows Vista and later), used by packaging/windows/neo_wolf.rc
    --png FILE      one PNG of --size pixels (default 1024), e.g. for documentation

Standard library only. The pixels depend only on this file (pure integer and float arithmetic,
no randomness), so every run on every machine draws the same image; the DevKit test
`test_gen_icon` checks that. The PNG bytes also depend on the zlib in use, so compare pixels,
not files, across machines.

Sizes up to 256 px are drawn at 4x, larger ones at 2x, and box-filtered down, which
anti-aliases the edges.

Usage:
    python3 tools/devkit/gen_icon.py --iconset build/NeoWolf.iconset --ico build/neowolf.ico
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
import zlib
from pathlib import Path
from typing import Sequence, Tuple

Color = Tuple[int, int, int]
Point = Tuple[float, float]

# --- Geometry, in a unit square with y pointing down -----------------------------------------
# The rounded square follows Apple's icon grid: an 824 px body on a 1024 px canvas.
BODY_MIN = 100.0 / 1024.0
BODY_MAX = 924.0 / 1024.0
BODY_RADIUS = 185.0 / 1024.0
# The far wall (the end of the corridor); the corridor's edges run from the body's corners to it.
FAR_X0, FAR_Y0, FAR_X1, FAR_Y1 = 0.30, 0.27, 0.70, 0.63
# How much smaller the far wall is than the body (perspective scale at the far end).
FAR_SCALE = (FAR_X1 - FAR_X0) / (BODY_MAX - BODY_MIN)
WALL_SEGMENTS = 4  # stone blocks along each wall

# The wolf's head, in a unit box placed over the far wall (WOLF_BOX).
WOLF_BOX = (0.335, 0.300, 0.665, 0.615)
WOLF_HEAD: list[Point] = [
    (0.16, 0.02),
    (0.38, 0.27),
    (0.62, 0.27),
    (0.84, 0.02),  # ears
    (0.88, 0.40),
    (0.98, 0.57),
    (0.80, 0.62),
    (0.64, 0.90),  # right cheek and jaw
    (0.50, 1.00),  # muzzle
    (0.36, 0.90),
    (0.20, 0.62),
    (0.02, 0.57),
    (0.12, 0.40),  # left jaw and cheek
]
WOLF_EYES: list[list[Point]] = [
    [(0.25, 0.44), (0.43, 0.50), (0.41, 0.56), (0.29, 0.52)],
    [(0.75, 0.44), (0.71, 0.52), (0.59, 0.56), (0.57, 0.50)],
]

# --- Colours ---------------------------------------------------------------------------------
CEILING_NEAR: Color = (24, 31, 46)
CEILING_FAR: Color = (52, 64, 86)
FLOOR_FAR: Color = (62, 50, 40)
FLOOR_NEAR: Color = (30, 24, 20)
WALL_STONE: Color = (104, 118, 146)
GLOW_CENTRE: Color = (255, 168, 72)
GLOW_EDGE: Color = (178, 34, 14)
WOLF: Color = (20, 13, 13)
EYES: Color = (255, 226, 110)

# Sizes written by --iconset (file name -> pixels) and --ico.
ICONSET: dict[str, int] = {
    "icon_16x16.png": 16,
    "icon_16x16@2x.png": 32,
    "icon_32x32.png": 32,
    "icon_32x32@2x.png": 64,
    "icon_128x128.png": 128,
    "icon_128x128@2x.png": 256,
    "icon_256x256.png": 256,
    "icon_256x256@2x.png": 512,
    "icon_512x512.png": 512,
    "icon_512x512@2x.png": 1024,
}
ICO_SIZES: Sequence[int] = (16, 24, 32, 48, 64, 128, 256)


def _lerp(a: Color, b: Color, t: float) -> Color:
    t = min(1.0, max(0.0, t))
    return (round(a[0] + (b[0] - a[0]) * t), round(a[1] + (b[1] - a[1]) * t), round(a[2] + (b[2] - a[2]) * t))


def _scale(c: Color, f: float) -> Color:
    return (min(255, round(c[0] * f)), min(255, round(c[1] * f)), min(255, round(c[2] * f)))


def _rgba(c: Color) -> bytes:
    return bytes((c[0], c[1], c[2], 255))


def _pixel_range(x0: float, x1: float, n: int) -> tuple[int, int]:
    """Pixels whose centres lie in [x0, x1) on a row of n pixels (unit coordinates)."""
    a = math.ceil(x0 * n - 0.5)
    b = math.ceil(x1 * n - 0.5)
    return max(a, 0), min(b, n)


def _polygon_spans(points: Sequence[Point], y: float) -> list[tuple[float, float]]:
    """x intervals inside the polygon on the horizontal line y (even-odd rule)."""
    xs: list[float] = []
    count = len(points)
    for i in range(count):
        (x0, y0), (x1, y1) = points[i], points[(i + 1) % count]
        if (y0 <= y < y1) or (y1 <= y < y0):
            xs.append(x0 + (y - y0) * (x1 - x0) / (y1 - y0))
    xs.sort()
    return [(xs[i], xs[i + 1]) for i in range(0, len(xs) - 1, 2)]


def _in_box(points: Sequence[Point], box: tuple[float, float, float, float]) -> list[Point]:
    x0, y0, x1, y1 = box
    return [(x0 + px * (x1 - x0), y0 + py * (y1 - y0)) for px, py in points]


def _body_span(y: float) -> tuple[float, float] | None:
    """The rounded square's x interval on row y, or None outside it."""
    if y < BODY_MIN or y >= BODY_MAX:
        return None
    inset = 0.0
    corner_top = BODY_MIN + BODY_RADIUS
    corner_bottom = BODY_MAX - BODY_RADIUS
    dy = corner_top - y if y < corner_top else (y - corner_bottom if y > corner_bottom else 0.0)
    if dy > 0.0:
        inset = BODY_RADIUS - math.sqrt(max(0.0, BODY_RADIUS * BODY_RADIUS - dy * dy))
    return BODY_MIN + inset, BODY_MAX - inset


def _wall_row(n: int, left: bool) -> bytes:
    """One full-width row of wall colour. A wall's shade depends on x only (vertical seams
    spaced by perspective, darker towards the far end), so every row reuses the same pattern."""
    out = bytearray(n * 4)
    near, far = (BODY_MIN, FAR_X0) if left else (BODY_MAX, FAR_X1)
    depth_far = 1.0 / FAR_SCALE
    segment = (depth_far - 1.0) / WALL_SEGMENTS
    for x in range(n):
        xc = (x + 0.5) / n
        t = (xc - near) / (far - near)
        t = min(1.0, max(0.0, t))
        depth = 1.0 / (1.0 - t * (1.0 - FAR_SCALE))  # 1 at the near edge, 1/FAR_SCALE at the far wall
        shade = 1.0 - 0.6 * (depth - 1.0) / (depth_far - 1.0)
        if not left:
            shade *= 0.82  # the light comes from the left
        phase = ((depth - 1.0) / segment) % 1.0
        if phase < 0.06 or phase > 0.97:
            shade *= 0.55  # mortar seam
        out[x * 4 : x * 4 + 4] = _rgba(_scale(WALL_STONE, shade))
    return bytes(out)


def _fill_span(row: bytearray, body: tuple[int, int], x0: int, x1: int, colour: bytes) -> None:
    """Pixels x0..x1 of an RGBA row in one colour, clipped to the body's pixels."""
    x0, x1 = max(x0, body[0]), min(x1, body[1])
    if x1 > x0:
        row[x0 * 4 : x1 * 4] = colour * (x1 - x0)


def _copy_span(row: bytearray, body: tuple[int, int], x0: int, x1: int, pattern: bytes) -> None:
    """Pixels x0..x1 of an RGBA row copied from a full-width pattern row, clipped to the body."""
    x0, x1 = max(x0, body[0]), min(x1, body[1])
    if x1 > x0:
        row[x0 * 4 : x1 * 4] = pattern[x0 * 4 : x1 * 4]


def render(n: int) -> list[bytearray]:
    """Draws the icon at n x n pixels, no anti-aliasing. Returns RGBA rows."""
    left_wall = _wall_row(n, left=True)
    right_wall = _wall_row(n, left=False)
    wolf = _in_box(WOLF_HEAD, WOLF_BOX)
    eyes = [_in_box(eye, WOLF_BOX) for eye in WOLF_EYES]
    glow_cx, glow_cy, glow_r = 0.5 * (FAR_X0 + FAR_X1), 0.47, 0.30
    rows: list[bytearray] = []
    for py in range(n):
        y = (py + 0.5) / n
        row = bytearray(n * 4)  # transparent
        rows.append(row)
        span = _body_span(y)
        if span is None:
            continue
        body = _pixel_range(span[0], span[1], n)
        b0, b1 = body

        if FAR_Y0 <= y < FAR_Y1:
            f0, f1 = _pixel_range(FAR_X0, FAR_X1, n)
            _copy_span(row, body, b0, f0, left_wall)
            _copy_span(row, body, f1, b1, right_wall)
            for x in range(f0, f1):  # radial glow
                d = math.hypot((x + 0.5) / n - glow_cx, y - glow_cy) / glow_r
                row[x * 4 : x * 4 + 4] = _rgba(_lerp(GLOW_CENTRE, GLOW_EDGE, d * d))
            for x0, x1 in _polygon_spans(wolf, y):
                _fill_span(row, body, *_pixel_range(x0, x1, n), _rgba(WOLF))
            for eye in eyes:
                for x0, x1 in _polygon_spans(eye, y):
                    _fill_span(row, body, *_pixel_range(x0, x1, n), _rgba(EYES))
            continue

        top = y < FAR_Y0
        # The corridor's edge lines run from the body's corners to the far wall's corners.
        edge_y, far_y = (BODY_MIN, FAR_Y0) if top else (BODY_MAX, FAR_Y1)
        t = (y - edge_y) / (far_y - edge_y)  # 0 at the body's edge, 1 at the far wall
        xl = BODY_MIN + t * (FAR_X0 - BODY_MIN)
        xr = BODY_MAX + t * (FAR_X1 - BODY_MAX)
        c0, c1 = _pixel_range(xl, xr, n)
        _copy_span(row, body, b0, c0, left_wall)
        _copy_span(row, body, c1, b1, right_wall)
        colour = _lerp(CEILING_NEAR, CEILING_FAR, t) if top else _lerp(FLOOR_NEAR, FLOOR_FAR, t)
        _fill_span(row, body, c0, c1, _rgba(colour))
    return rows


def downsample(rows: list[bytearray], factor: int) -> list[bytearray]:
    """Box filter by an integer factor; colours are weighted by alpha (no dark fringes)."""
    n = len(rows) // factor
    area = factor * factor
    out: list[bytearray] = []
    for oy in range(n):
        acc_r = [0] * n
        acc_g = [0] * n
        acc_b = [0] * n
        acc_a = [0] * n
        for src in rows[oy * factor : (oy + 1) * factor]:
            for ox in range(n):
                i = ox * factor * 4
                for _ in range(factor):
                    a = src[i + 3]
                    if a:
                        acc_r[ox] += src[i] * a
                        acc_g[ox] += src[i + 1] * a
                        acc_b[ox] += src[i + 2] * a
                        acc_a[ox] += a
                    i += 4
        row = bytearray(n * 4)
        for ox in range(n):
            a = acc_a[ox]
            if a:
                row[ox * 4] = (acc_r[ox] + a // 2) // a
                row[ox * 4 + 1] = (acc_g[ox] + a // 2) // a
                row[ox * 4 + 2] = (acc_b[ox] + a // 2) // a
                row[ox * 4 + 3] = (a + area // 2) // area
        out.append(row)
    return out


_CACHE: dict[int, list[bytearray]] = {}


def icon_pixels(size: int) -> list[bytearray]:
    """The icon at size x size, anti-aliased by supersampling (4x up to 256 px, else 2x)."""
    if size not in _CACHE:
        factor = 4 if size <= 256 else 2
        rows = render(size * factor)
        _CACHE[size] = downsample(rows, factor) if factor > 1 else rows
    return _CACHE[size]


def encode_png(rows: list[bytearray]) -> bytes:
    """RGBA, 8 bits per channel, filter type 0 on every row."""
    height = len(rows)
    width = len(rows[0]) // 4 if rows else 0

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + bytes(row) for row in rows)
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def encode_ico(images: Sequence[tuple[int, bytes]]) -> bytes:
    """An .ico whose entries are PNG files (allowed since Windows Vista)."""
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries = b""
    for size, png in images:
        dim = size % 256  # 0 means 256
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(png), offset)
        offset += len(png)
    return header + entries + b"".join(png for _, png in images)


def write_iconset(directory: Path) -> list[Path]:
    directory.mkdir(parents=True, exist_ok=True)
    written = []
    for name, size in ICONSET.items():
        path = directory / name
        path.write_bytes(encode_png(icon_pixels(size)))
        written.append(path)
    return written


def write_ico(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encode_ico([(size, encode_png(icon_pixels(size))) for size in ICO_SIZES]))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--iconset", type=Path, help="write a macOS .iconset folder (PNG files for iconutil)")
    parser.add_argument("--ico", type=Path, help="write a Windows .ico file")
    parser.add_argument("--png", type=Path, help="write one PNG file of --size pixels")
    parser.add_argument("--size", type=int, default=1024, help="pixels for --png (16 to 1024, default 1024)")
    args = parser.parse_args(argv)
    if not (args.iconset or args.ico or args.png):
        parser.error("nothing to do: give --iconset, --ico and/or --png")
    if not 16 <= args.size <= 1024:
        parser.error("--size must be from 16 to 1024")
    if args.iconset:
        if args.iconset.suffix != ".iconset":
            parser.error("--iconset: the folder name must end in .iconset (iconutil needs that)")
        write_iconset(args.iconset)
        print(f"gen_icon: wrote {len(ICONSET)} PNG files to {args.iconset}")
    if args.ico:
        write_ico(args.ico)
        print(f"gen_icon: wrote {args.ico} ({', '.join(str(s) for s in ICO_SIZES)} px)")
    if args.png:
        args.png.parent.mkdir(parents=True, exist_ok=True)
        args.png.write_bytes(encode_png(icon_pixels(args.size)))
        print(f"gen_icon: wrote {args.png} ({args.size} px)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
