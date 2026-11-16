#!/usr/bin/env python3
"""Deterministic generator for Neo-Wolf's billboard sprites (Roadmap Phase 6).

Writes 32x32 plain-text PPM (P3) sprites to game/assets/sprites/:

    guard.ppm  brute.ppm  runner.ppm                    the three enemy types
    guard_dead.ppm  brute_dead.ppm  runner_dead.ppm     their corpses (drawn on the floor)
    health.ppm  ammo.ppm                                pickups
    key_red.ppm  key_blue.ppm  key_gold.ppm             key cards
    decoration.ppm                                      the blue vase (level landmarks)

game/defs/sprites.cfg maps the engine's sprite slots to these files (docs/ASSET_GUIDE.md).

Transparency follows the engine's rule (SpriteRenderer): pure magenta RGB(255, 0, 255) is not
drawn. A sprite is drawn one map cell tall, from floor to ceiling, so a figure's feet are on
the bottom row and objects lying on the floor use only the lower rows.

The enemies are told apart at 320x240 by silhouette as well as colour: the guard is an
upright olive soldier with a helmet and rifle, the brute fills the whole frame (wide armoured
shoulders, dark red, yellow visor), and the runner is small, slim and mid-stride (cyan, orange
visor). Every figure gets a dark 1-pixel outline. Key cards carry their initial (R, B, G).
The decoration (Phase 10) is a blue-and-white floor vase; it replaced the hand-made 16x16
sprite_decor.ppm (known issue X5).

Standard library only. Each sprite uses its own random.Random seeded from SEED and the
sprite's name (zlib.crc32), so the output is byte-for-byte reproducible. The images are drawn
from scratch by this script: no third-party artwork (docs/ASSET_GUIDE.md).

Usage:
    python3 tools/devkit/gen_sprites.py                 # (re)write the files under game/
    python3 tools/devkit/gen_sprites.py --out DIR       # write to DIR instead
    python3 tools/devkit/gen_sprites.py --check         # exit 1 if a committed file differs
"""

from __future__ import annotations

import argparse
import math
import random
import sys
import zlib
from pathlib import Path
from typing import Callable

try:  # run as a module (tests) or as a script
    from tools.devkit.gen_textures import LETTERS, LOCK_COLORS, SEED, SIZE, Color, clamp, shade
except ImportError:  # pragma: no cover
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from tools.devkit.gen_textures import LETTERS, LOCK_COLORS, SEED, SIZE, Color, clamp, shade

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT = REPO_ROOT / "game" / "assets" / "sprites"

KEY: Color = (255, 0, 255)  # transparent
OUTLINE: Color = (18, 18, 22)


class Sprite:
    def __init__(self, rng: random.Random) -> None:
        self.rng = rng
        self.px: list[list[Color]] = [[KEY for _ in range(SIZE)] for _ in range(SIZE)]

    def set(self, x: int, y: int, c: Color) -> None:
        if 0 <= x < SIZE and 0 <= y < SIZE:
            self.px[y][x] = clamp(c)

    def opaque(self, x: int, y: int) -> bool:
        return 0 <= x < SIZE and 0 <= y < SIZE and self.px[y][x] != KEY

    def rect(self, x0: int, y0: int, w: int, h: int, c: Color, noise: int = 0) -> None:
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                d = self.rng.randint(-noise, noise) if noise else 0
                self.set(x, y, (c[0] + d, c[1] + d, c[2] + d))

    def ellipse(self, cx: float, cy: float, rx: float, ry: float, c: Color, noise: int = 0) -> None:
        for y in range(SIZE):
            for x in range(SIZE):
                if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0:
                    d = self.rng.randint(-noise, noise) if noise else 0
                    self.set(x, y, (c[0] + d, c[1] + d, c[2] + d))

    def line(self, x0: int, y0: int, x1: int, y1: int, c: Color, width: int = 1) -> None:
        steps = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(steps + 1):
            x = round(x0 + (x1 - x0) * i / steps)
            y = round(y0 + (y1 - y0) * i / steps)
            self.rect(x, y, width, width, c)

    def letter(self, ch: str, x0: int, y0: int, c: Color, scale: int = 1) -> None:
        for row, bits in enumerate(LETTERS[ch]):
            for col, bit in enumerate(bits):
                if bit == "1":
                    self.rect(x0 + col * scale, y0 + row * scale, scale, scale, c)

    def outline(self) -> None:
        """A dark 1-pixel border around every opaque shape (crisp silhouettes when scaled)."""
        edge = [
            (x, y)
            for y in range(SIZE)
            for x in range(SIZE)
            if not self.opaque(x, y)
            and any(self.opaque(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        ]
        for x, y in edge:
            self.px[y][x] = OUTLINE

    def ppm(self, comment: str) -> str:
        lines = ["P3", f"# {comment}", f"{SIZE} {SIZE}", "255"]
        for row in self.px:
            lines.append(" ".join(f"{r} {g} {b}" for r, g, b in row))
        return "\n".join(lines) + "\n"


# --- Enemies ------------------------------------------------------------------------------

GUARD_UNIFORM: Color = (118, 128, 58)
GUARD_HELMET: Color = (58, 78, 40)
BRUTE_ARMOUR: Color = (158, 40, 34)
BRUTE_METAL: Color = (150, 152, 162)
RUNNER_SUIT: Color = (40, 190, 205)
SKIN: Color = (222, 172, 132)
BOOT: Color = (52, 40, 30)
GUN: Color = (48, 48, 54)


def guard(s: Sprite) -> None:
    """Upright soldier, 29 rows tall: helmet, face, olive uniform, rifle across the chest."""
    s.rect(10, 29, 5, 3, BOOT)
    s.rect(17, 29, 5, 3, BOOT)
    s.rect(11, 21, 4, 8, shade(GUARD_UNIFORM, 0.8), 6)
    s.rect(17, 21, 4, 8, shade(GUARD_UNIFORM, 0.8), 6)
    s.rect(9, 11, 14, 10, GUARD_UNIFORM, 6)
    s.rect(9, 19, 14, 2, (70, 52, 30))  # belt
    s.rect(15, 19, 2, 2, (200, 170, 60))  # buckle
    s.rect(6, 11, 3, 8, GUARD_UNIFORM, 6)
    s.rect(23, 11, 3, 8, GUARD_UNIFORM, 6)
    s.rect(6, 18, 3, 2, SKIN)
    s.rect(23, 18, 3, 2, SKIN)
    s.line(5, 18, 26, 12, GUN, 2)  # rifle
    s.rect(24, 10, 2, 3, GUN)
    s.rect(12, 5, 8, 6, SKIN)
    s.rect(13, 7, 2, 1, (30, 30, 30))
    s.rect(17, 7, 2, 1, (30, 30, 30))
    s.rect(14, 9, 4, 1, shade(SKIN, 0.75))
    s.rect(11, 2, 10, 4, GUARD_HELMET, 4)
    s.rect(10, 5, 12, 1, shade(GUARD_HELMET, 0.7))
    s.outline()


def brute(s: Sprite) -> None:
    """A wall of armour: the full 32x32 frame, wide metal shoulders, small head, yellow visor."""
    s.rect(5, 28, 7, 4, (60, 60, 66))
    s.rect(20, 28, 7, 4, (60, 60, 66))
    s.rect(7, 20, 6, 8, shade(BRUTE_ARMOUR, 0.7), 6)
    s.rect(19, 20, 6, 8, shade(BRUTE_ARMOUR, 0.7), 6)
    s.rect(4, 8, 24, 13, BRUTE_ARMOUR, 6)
    s.rect(9, 10, 14, 7, shade(BRUTE_ARMOUR, 1.25), 5)  # chest plate
    s.rect(15, 10, 2, 7, shade(BRUTE_ARMOUR, 0.8))
    s.rect(4, 19, 24, 2, (50, 30, 26))  # belt
    s.rect(1, 6, 7, 6, BRUTE_METAL, 5)  # shoulder pads
    s.rect(24, 6, 7, 6, BRUTE_METAL, 5)
    s.rect(1, 12, 5, 9, BRUTE_ARMOUR, 6)  # arms
    s.rect(26, 12, 5, 9, BRUTE_ARMOUR, 6)
    s.rect(0, 20, 6, 5, BRUTE_METAL, 4)  # fists
    s.rect(26, 20, 6, 5, BRUTE_METAL, 4)
    s.rect(11, 0, 10, 8, (72, 72, 82), 4)  # helmet
    s.rect(12, 3, 8, 2, (255, 212, 40))  # visor
    s.rect(14, 6, 4, 1, (40, 40, 46))
    s.outline()


def runner(s: Sprite) -> None:
    """Small and slim, 23 rows tall, mid-stride and leaning forward; cyan suit, orange visor."""
    s.line(14, 23, 9, 29, (22, 92, 104), 3)  # back leg
    s.line(18, 23, 23, 29, (22, 92, 104), 3)  # front leg
    s.rect(7, 29, 4, 2, (235, 235, 235))  # shoes
    s.rect(22, 29, 5, 2, (235, 235, 235))
    s.rect(12, 15, 8, 9, RUNNER_SUIT, 6)  # torso
    s.line(12, 16, 19, 23, (235, 235, 235), 1)  # stripe
    s.line(13, 16, 7, 20, shade(RUNNER_SUIT, 0.75), 2)  # back arm
    s.line(19, 16, 25, 18, shade(RUNNER_SUIT, 0.75), 2)  # front arm
    s.line(26, 17, 29, 15, (200, 200, 210), 1)  # blade
    s.rect(14, 9, 6, 6, RUNNER_SUIT, 4)  # hood
    s.rect(15, 11, 5, 2, (255, 140, 30))  # visor
    s.outline()


def corpse(s: Sprite, body: Color, legs: Color, head: Color, face: Color, length: int, thickness: int) -> None:
    """A body lying on its back in a pool of blood (rows 21-31 only): boots and legs on the
    left, the torso with an arm flung up, the head on the right."""
    s.ellipse(16, 29.5, 14, 2.4, (104, 12, 12), 6)
    x0 = (SIZE - length) // 2 + 1
    y0 = 29 - thickness
    leg_h = max(1, thickness // 2)
    s.rect(x0 - 7, y0 + thickness - 2 * leg_h, 7, leg_h, legs, 4)  # legs, apart
    s.rect(x0 - 7, y0 + thickness - leg_h + 1, 7, leg_h, legs, 4)
    s.rect(x0 - 9, y0 + thickness - 2 * leg_h, 2, 2 * leg_h + 1, BOOT)
    s.rect(x0, y0, length, thickness, body, 6)  # torso
    s.line(x0 + length // 2, y0, x0 + length // 2 + 4, y0 - 4, body, 2)  # arm
    s.rect(x0 + length // 2 + 4, y0 - 5, 2, 2, face)
    s.rect(x0 + length, y0 - 1, 5, thickness + 1, face, 3)  # head
    s.rect(x0 + length + 2, y0 - 1, 3, thickness + 1, head, 3)
    s.outline()


def guard_dead(s: Sprite) -> None:
    corpse(s, GUARD_UNIFORM, shade(GUARD_UNIFORM, 0.8), GUARD_HELMET, SKIN, 12, 4)


def brute_dead(s: Sprite) -> None:
    corpse(s, BRUTE_ARMOUR, shade(BRUTE_ARMOUR, 0.7), (72, 72, 82), (255, 212, 40), 14, 6)


def runner_dead(s: Sprite) -> None:
    corpse(s, RUNNER_SUIT, (22, 92, 104), RUNNER_SUIT, (255, 140, 30), 10, 3)


# --- Pickups ------------------------------------------------------------------------------


def health(s: Sprite) -> None:
    """A white medkit with a red cross, on the floor."""
    s.rect(13, 17, 6, 2, (120, 120, 126))  # handle
    s.rect(7, 19, 18, 12, (236, 236, 232), 4)
    s.rect(7, 29, 18, 2, (190, 190, 186))
    s.rect(14, 21, 4, 8, (220, 30, 30))
    s.rect(11, 23, 10, 4, (220, 30, 30))
    s.outline()


def ammo(s: Sprite) -> None:
    """An olive ammo box with a row of brass cartridges standing on it."""
    for x in range(8, 24, 3):
        s.rect(x, 16, 2, 6, (212, 168, 60))
        s.rect(x, 15, 2, 1, (238, 206, 130))
    s.rect(6, 22, 20, 9, (88, 96, 48), 5)
    s.rect(6, 22, 20, 1, (120, 130, 70))
    s.rect(10, 25, 12, 3, (40, 40, 30))
    s.rect(11, 26, 10, 1, (212, 168, 60))
    s.outline()


def key_card(color: Color, letter: str) -> Callable[[Sprite], None]:
    def draw(s: Sprite) -> None:
        """A key card floating above a shadow, in the key's colour, with its initial."""
        s.ellipse(16, 30, 6, 1.3, (40, 40, 44))
        s.rect(8, 12, 16, 14, color, 4)
        s.rect(8, 12, 16, 2, shade(color, 0.6))  # magnetic stripe
        s.rect(10, 15, 3, 3, (16, 16, 18))  # punch hole
        s.letter(letter, 14, 15, (250, 250, 250), 2)
        s.rect(8, 25, 16, 1, shade(color, 0.6))
        s.outline()

    return draw


# --- Decoration ---------------------------------------------------------------------------

VASE_GLAZE: Color = (150, 192, 240)
VASE_PATTERN: Color = (34, 70, 160)
# Half-width of the vase (pixels) at the given rows; rows in between are interpolated.
VASE_PROFILE: list[tuple[int, float]] = [
    (7, 5.5),
    (8, 5.5),
    (9, 3.5),
    (12, 3.5),
    (14, 5.5),
    (16, 8.5),
    (19, 10.0),
    (22, 10.0),
    (25, 8.0),
    (28, 5.5),
    (29, 6.5),
    (31, 6.5),
]


def vase_half_width(y: int) -> float:
    for (y0, r0), (y1, r1) in zip(VASE_PROFILE, VASE_PROFILE[1:]):
        if y0 <= y <= y1:
            return r0 + (r1 - r0) * (y - y0) / (y1 - y0)
    return 0.0


def decoration(s: Sprite) -> None:
    """The landmark decoration (docs/levels/README.md rule 13): a blue-and-white porcelain
    floor vase, 25 rows tall, standing on the floor. A light glaze shaded as a cylinder lit
    from the left, with a highlight, dark blue bands at the rim, shoulder, base and foot, and
    a wave round the belly. Pale and bright, so it reads in the dim light zones and against
    every theme's walls."""
    cx = SIZE / 2

    def u_at(x: int, y: int) -> float:  # -1 at the vase's left edge, +1 at its right edge
        return (x + 0.5 - cx) / vase_half_width(y)

    for y in range(7, SIZE):
        for x in range(SIZE):
            u = u_at(x, y)
            if abs(u) > 1.0:
                continue
            light = 0.62 + 0.38 * math.sqrt(1.0 - u * u) - 0.16 * u
            if -0.62 <= u <= -0.38 and 15 <= y <= 27:
                light += 0.22  # highlight
            d = s.rng.randint(-3, 3)
            s.set(x, y, shade((VASE_GLAZE[0] + d, VASE_GLAZE[1] + d, VASE_GLAZE[2] + d), light))

    def paint(x: int, y: int) -> None:
        u = u_at(x, y)
        if abs(u) <= 1.0:
            s.set(x, y, shade(VASE_PATTERN, 0.85 + 0.25 * math.sqrt(1.0 - u * u)))

    for y in (7, 15, 17, 26, 29, 30):  # rim, shoulder, base and foot bands
        for x in range(SIZE):
            paint(x, y)
    s.rect(14, 8, 4, 1, (28, 34, 60))  # the opening
    for x in range(SIZE):  # the wave round the belly, rows 19-23
        if abs(u_at(x, 21)) <= 0.92:
            paint(x, 21 + round(1.6 * math.sin(x * 0.9)))
    s.outline()


SPRITES: dict[str, Callable[[Sprite], None]] = {
    "guard": guard,
    "brute": brute,
    "runner": runner,
    "guard_dead": guard_dead,
    "brute_dead": brute_dead,
    "runner_dead": runner_dead,
    "health": health,
    "ammo": ammo,
    "key_red": key_card(LOCK_COLORS["red"], "R"),
    "key_blue": key_card(LOCK_COLORS["blue"], "B"),
    "key_gold": key_card(LOCK_COLORS["gold"], "G"),
    "decoration": decoration,
}


def generate() -> dict[str, str]:
    """Returns {"<name>.ppm": PPM text} for every sprite."""
    files: dict[str, str] = {}
    for name, draw in SPRITES.items():
        sprite = Sprite(random.Random(SEED ^ zlib.crc32(f"sprite/{name}".encode())))
        draw(sprite)
        files[f"{name}.ppm"] = sprite.ppm(
            f"Neo-Wolf sprite '{name}': generated by tools/devkit/gen_sprites.py (seed {SEED}); magenta = transparent"
        )
    return files


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="output folder (default: game/assets/sprites)")
    parser.add_argument("--check", action="store_true", help="compare with the files on disk instead of writing")
    args = parser.parse_args(argv if argv is not None else sys.argv[1:])

    files = generate()
    if args.check:
        stale = [
            rel
            for rel, text in files.items()
            if not (args.out / rel).is_file() or (args.out / rel).read_text(encoding="utf-8") != text
        ]
        if stale:
            print("Out of date (run tools/devkit/gen_sprites.py): " + ", ".join(stale))
            return 1
        print(f"All {len(files)} generated sprites are up to date.")
        return 0

    args.out.mkdir(parents=True, exist_ok=True)
    for rel, text in files.items():
        with open(args.out / rel, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
    print(f"Wrote {len(files)} sprites to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
