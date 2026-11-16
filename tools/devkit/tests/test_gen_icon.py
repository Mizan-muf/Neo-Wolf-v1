"""Tests for the application icon generator (tools/devkit/gen_icon.py, Roadmap Phase 9)."""

import hashlib
import io
import struct
import sys
import tempfile
import unittest
import zlib
from contextlib import redirect_stdout
from pathlib import Path

DEVKIT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(DEVKIT_DIR))
import gen_icon  # noqa: E402  (a stand-alone script, imported by path like gen_textures)


def decode_png(data: bytes):
    """Width, height and RGBA rows of a PNG written by gen_icon (checks signature and CRCs)."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos, chunks = 8, {}
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos : pos + 8])
        body = data[pos + 8 : pos + 8 + length]
        crc = struct.unpack(">I", data[pos + 8 + length : pos + 12 + length])[0]
        assert crc == zlib.crc32(kind + body) & 0xFFFFFFFF, kind
        chunks[kind] = body
        pos += 12 + length
    width, height, depth, colour = struct.unpack(">IIBB", chunks[b"IHDR"][:10])
    assert (depth, colour) == (8, 6)
    raw = zlib.decompress(chunks[b"IDAT"])
    stride = width * 4 + 1
    rows = [raw[i * stride + 1 : (i + 1) * stride] for i in range(height)]
    assert all(raw[i * stride] == 0 for i in range(height))
    return width, height, rows


class TestGenIcon(unittest.TestCase):
    def test_pixels_are_deterministic(self) -> None:
        first = b"".join(gen_icon.render(64))
        second = b"".join(gen_icon.render(64))
        self.assertEqual(first, second)
        self.assertEqual(len(first), 64 * 64 * 4)
        gen_icon._CACHE.clear()
        once = hashlib.sha256(b"".join(gen_icon.icon_pixels(32))).hexdigest()
        gen_icon._CACHE.clear()
        self.assertEqual(hashlib.sha256(b"".join(gen_icon.icon_pixels(32))).hexdigest(), once)

    def test_shape(self) -> None:
        rows = gen_icon.icon_pixels(128)
        self.assertEqual(len(rows), 128)

        def alpha(x: int, y: int) -> int:
            return rows[y][x * 4 + 3]

        self.assertEqual(alpha(0, 0), 0)  # transparent corner (rounded square)
        self.assertEqual(alpha(127, 127), 0)
        self.assertEqual(alpha(64, 64), 255)  # opaque middle
        self.assertEqual(alpha(64, 6), 0)  # above the body
        r, _, b = rows[40][64 * 4 : 64 * 4 + 3]  # the glowing far wall above the wolf's head
        self.assertGreater(r, 150)
        self.assertGreater(r, b)

    def test_png_ico_and_iconset(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)
            with redirect_stdout(io.StringIO()):
                code = gen_icon.main(
                    [
                        "--iconset",
                        str(out / "NeoWolf.iconset"),
                        "--ico",
                        str(out / "a.ico"),
                        "--png",
                        str(out / "a.png"),
                        "--size",
                        "48",
                    ]
                )
            self.assertEqual(code, 0)
            names = sorted(p.name for p in (out / "NeoWolf.iconset").iterdir())
            self.assertEqual(names, sorted(gen_icon.ICONSET))
            for name, size in gen_icon.ICONSET.items():
                width, height, _ = decode_png((out / "NeoWolf.iconset" / name).read_bytes())
                self.assertEqual((width, height), (size, size), name)
            width, _, rows = decode_png((out / "a.png").read_bytes())
            self.assertEqual(width, 48)
            self.assertEqual(b"".join(rows), b"".join(gen_icon.icon_pixels(48)))

            ico = (out / "a.ico").read_bytes()
            reserved, kind, count = struct.unpack("<HHH", ico[:6])
            self.assertEqual((reserved, kind, count), (0, 1, len(gen_icon.ICO_SIZES)))
            for i, size in enumerate(gen_icon.ICO_SIZES):
                w, h, _, _, planes, bpp, length, offset = struct.unpack("<BBBBHHII", ico[6 + 16 * i : 22 + 16 * i])
                self.assertEqual((w or 256, h or 256, planes, bpp), (size, size, 1, 32))
                width, _, _ = decode_png(ico[offset : offset + length])
                self.assertEqual(width, size)

    def test_iconset_name_is_checked(self) -> None:
        with tempfile.TemporaryDirectory() as temp, self.assertRaises(SystemExit):
            gen_icon.main(["--iconset", str(Path(temp) / "icons")])


if __name__ == "__main__":
    unittest.main()
