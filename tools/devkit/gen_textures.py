#!/usr/bin/env python3
"""Deterministic generator for Neo-Wolf's per-level texture themes (Roadmap Phase 5).

Writes 32x32 plain-text PPM (P3) textures to game/assets/themes/<theme>/:

    wall.ppm  wall_alt_a.ppm  wall_alt_b.ppm  door.ppm  floor.ppm  ceiling.ppm  exit.ppm
    door_red.ppm  door_blue.ppm  door_gold.ppm   (locked doors, Roadmap Phase 6)

for the themes "bunker", "stone" and "tech", plus game/assets/themes/classic/exit.ppm and
door_<colour>.ppm (the "classic" theme otherwise uses the original textures).

A locked door is the theme's door with a band in the lock's colour across the middle, and on
it the colour's initial (R, B, G) and a keyhole, so it differs from a plain door, and the
three locks from each other, by shape as well as colour.

Billboard sprites (enemies, corpses, pickups, keys) come from the sibling generator
tools/devkit/gen_sprites.py.

Standard library only. Every texture uses its own random.Random seeded from SEED and the
texture's name (zlib.crc32, not Python's salted hash()), so the output is byte-for-byte
reproducible on every machine and Python version. The images are drawn from scratch by this
script: there is no third-party artwork and no licence to track (docs/ASSET_GUIDE.md).

Usage:
    python3 tools/devkit/gen_textures.py                 # (re)write the files under game/
    python3 tools/devkit/gen_textures.py --out DIR       # write to DIR/<theme>/ instead
    python3 tools/devkit/gen_textures.py --check         # exit 1 if a committed file differs
"""
from __future__ import annotations

import argparse
import random
import sys
import zlib
from pathlib import Path
from typing import Callable, Dict, List, Tuple

SEED = 20261116
SIZE = 32
REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT = REPO_ROOT / "game" / "assets" / "themes"

Color = Tuple[int, int, int]


class Image:
    def __init__(self, fill: Color = (0, 0, 0)) -> None:
        self.px: List[List[Color]] = [[fill for _ in range(SIZE)] for _ in range(SIZE)]

    def set(self, x: int, y: int, c: Color) -> None:
        if 0 <= x < SIZE and 0 <= y < SIZE:
            self.px[y][x] = clamp(c)

    def get(self, x: int, y: int) -> Color:
        return self.px[y % SIZE][x % SIZE]

    def rect(self, x0: int, y0: int, w: int, h: int, c: Color) -> None:
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                self.set(x, y, c)

    def frame(self, x0: int, y0: int, w: int, h: int, c: Color) -> None:
        for x in range(x0, x0 + w):
            self.set(x, y0, c)
            self.set(x, y0 + h - 1, c)
        for y in range(y0, y0 + h):
            self.set(x0, y, c)
            self.set(x0 + w - 1, y, c)

    def ppm(self, comment: str) -> str:
        lines = ["P3", f"# {comment}", f"{SIZE} {SIZE}", "255"]
        for row in self.px:
            lines.append(" ".join(f"{r} {g} {b}" for r, g, b in row))
        return "\n".join(lines) + "\n"


def clamp(c: Color) -> Color:
    return (max(0, min(255, int(c[0]))), max(0, min(255, int(c[1]))), max(0, min(255, int(c[2]))))


def shade(c: Color, f: float) -> Color:
    return clamp((c[0] * f, c[1] * f, c[2] * f))


def noisy_fill(img: Image, rng: random.Random, base: Color, spread: int) -> None:
    for y in range(SIZE):
        for x in range(SIZE):
            d = rng.randint(-spread, spread)
            img.set(x, y, (base[0] + d, base[1] + d, base[2] + d))


# 3x5 pixel letters for the exit switch label.
LETTERS = {
    "E": ["111", "100", "110", "100", "111"],
    "X": ["101", "101", "010", "101", "101"],
    "I": ["111", "010", "010", "010", "111"],
    "T": ["111", "010", "010", "010", "010"],
    "R": ["110", "101", "110", "101", "101"],
    "B": ["110", "101", "110", "101", "110"],
    "G": ["011", "100", "101", "101", "011"],
}

# Lock colours (Phase 6), shared with gen_sprites.py and the engine's HUD (KeyCardColor).
LOCK_COLORS: Dict[str, Color] = {"red": (220, 45, 40), "blue": (45, 105, 240), "gold": (240, 190, 30)}


def draw_word(img: Image, word: str, x0: int, y0: int, c: Color) -> None:
    for i, ch in enumerate(word):
        for row, bits in enumerate(LETTERS[ch]):
            for col, bit in enumerate(bits):
                if bit == "1":
                    img.set(x0 + i * 4 + col, y0 + row, c)


def bricks(img: Image, rng: random.Random, base: Color, mortar: Color, brick_h: int, brick_w: int,
           jitter: int) -> None:
    for y in range(SIZE):
        row = y // brick_h
        offset = (brick_w // 2) if row % 2 else 0
        for x in range(SIZE):
            if y % brick_h == 0 or (x + offset) % brick_w == 0:
                img.set(x, y, mortar)
            else:
                brick = (row * 7 + (x + offset) // brick_w * 13) % 5
                f = 0.85 + brick * 0.06
                d = rng.randint(-jitter, jitter)
                c = shade(base, f)
                img.set(x, y, (c[0] + d, c[1] + d, c[2] + d))


def exit_switch(img: Image, rng: random.Random, plate: Color, trim: Color) -> None:
    """A wall plate with an 'EXIT' label and a big lever switch with a green lamp: the same
    layout in every theme, so the switch is recognisable by shape, not only by colour."""
    noisy_fill(img, rng, plate, 6)
    img.frame(0, 0, SIZE, SIZE, shade(plate, 0.6))
    # Label strip.
    img.rect(6, 2, 20, 9, (12, 16, 12))
    draw_word(img, "EXIT", 9, 4, (90, 255, 120))
    # Switch housing.
    img.rect(9, 13, 14, 16, shade(trim, 0.55))
    img.frame(9, 13, 14, 16, trim)
    # Lever: a thick bar up and to the right, with a knob.
    for i in range(8):
        img.rect(15 + i // 2, 24 - i, 2, 2, (200, 200, 200))
    img.rect(18, 15, 4, 3, (230, 60, 50))
    # Green lamp and screws.
    img.rect(11, 15, 3, 3, (60, 255, 90))
    for sx, sy in ((3, 3), (28, 3), (3, 28), (28, 28)):
        img.set(sx, sy, shade(trim, 1.2))


def locked_door(base: Image, color: Color, letter: str) -> Image:
    """The theme's door with a coloured band (rows 11-20), the lock's initial and a keyhole."""
    img = Image()
    for y in range(SIZE):
        for x in range(SIZE):
            img.set(x, y, base.get(x, y))
    img.rect(0, 11, SIZE, 10, color)
    img.rect(0, 11, SIZE, 1, shade(color, 0.45))
    img.rect(0, 20, SIZE, 1, shade(color, 0.45))
    # A light plate with the letter, and a dark keyhole beside it.
    img.rect(9, 12, 7, 8, (235, 235, 230))
    draw_word(img, letter, 11, 13, (20, 20, 24))
    img.rect(19, 13, 3, 3, (16, 16, 18))
    img.rect(20, 16, 1, 3, (16, 16, 18))
    return img


# --- Themes --------------------------------------------------------------------------------

def bunker(name: str, rng: random.Random) -> Image:
    img = Image()
    concrete = (118, 116, 108)
    if name == "wall":
        noisy_fill(img, rng, concrete, 10)
        for x in range(SIZE):  # horizontal form-work seams
            img.set(x, 15, shade(concrete, 0.55))
            img.set(x, 31, shade(concrete, 0.55))
        for y in range(SIZE):
            img.set(0, y, shade(concrete, 0.6))
        for _ in range(6):  # pock marks
            img.set(rng.randrange(SIZE), rng.randrange(SIZE), shade(concrete, 0.5))
    elif name == "wall_alt_a":  # olive painted steel with rivets
        olive = (92, 104, 62)
        noisy_fill(img, rng, olive, 6)
        for y in (0, 16):
            img.rect(0, y, SIZE, 1, shade(olive, 0.5))
        for y in (3, 19):
            for x in range(3, SIZE, 7):
                img.set(x, y, (170, 170, 150))
    elif name == "wall_alt_b":  # hazard stripes over concrete
        noisy_fill(img, rng, concrete, 8)
        for y in range(10, 22):
            for x in range(SIZE):
                img.set(x, y, (230, 190, 30) if ((x + y) // 4) % 2 == 0 else (30, 30, 30))
    elif name == "door":  # steel blast door with vertical slats
        steel = (96, 104, 112)
        noisy_fill(img, rng, steel, 5)
        for x in range(2, SIZE, 6):
            img.rect(x, 2, 1, SIZE - 4, shade(steel, 0.6))
        img.frame(0, 0, SIZE, SIZE, (40, 44, 48))
        img.rect(13, 14, 6, 4, (210, 170, 40))
    elif name == "floor":
        noisy_fill(img, rng, (70, 68, 64), 7)
        x, y = rng.randrange(SIZE), rng.randrange(SIZE)
        for _ in range(20):  # a crack
            img.set(x, y, (40, 38, 36))
            x += rng.choice((-1, 0, 1))
            y += 1
    elif name == "ceiling":
        noisy_fill(img, rng, (58, 58, 60), 5)
        img.rect(0, 0, SIZE, 1, (36, 36, 38))
        img.rect(0, 0, 1, SIZE, (36, 36, 38))
        img.rect(10, 12, 12, 8, (200, 196, 160))  # light fixture
        img.frame(10, 12, 12, 8, (90, 90, 80))
    elif name == "exit":
        exit_switch(img, rng, (110, 112, 104), (150, 150, 140))
    return img


def stone(name: str, rng: random.Random) -> Image:
    img = Image()
    stone_c = (128, 118, 102)
    if name == "wall":
        bricks(img, rng, stone_c, (70, 64, 56), 8, 16, 8)
    elif name == "wall_alt_a":  # mossy stone
        bricks(img, rng, stone_c, (66, 70, 50), 8, 16, 8)
        for _ in range(90):
            x, y = rng.randrange(SIZE), rng.randrange(SIZE)
            if y > 12:
                img.set(x, y, (60 + rng.randint(0, 30), 110 + rng.randint(0, 40), 50))
    elif name == "wall_alt_b":  # dark granite blocks
        bricks(img, rng, (78, 78, 86), (40, 40, 46), 16, 16, 10)
    elif name == "door":  # wooden planks with iron bands
        wood = (120, 78, 42)
        for y in range(SIZE):
            for x in range(SIZE):
                plank = x // 8
                d = rng.randint(-8, 8) + (plank % 2) * 10
                img.set(x, y, (wood[0] + d, wood[1] + d, wood[2] + d // 2))
        for x in range(0, SIZE, 8):
            img.rect(x, 0, 1, SIZE, (60, 38, 20))
        for y in (5, 26):
            img.rect(0, y, SIZE, 2, (52, 52, 56))
        img.rect(24, 15, 3, 3, (40, 40, 44))
    elif name == "floor":  # cobblestones
        bricks(img, rng, (96, 92, 84), (50, 48, 44), 8, 8, 10)
    elif name == "ceiling":  # wooden beams
        for y in range(SIZE):
            for x in range(SIZE):
                d = rng.randint(-6, 6)
                if x % 16 < 5:
                    img.set(x, y, (92 + d, 60 + d, 34 + d))
                else:
                    img.set(x, y, (46 + d, 42 + d, 38 + d))
    elif name == "exit":
        exit_switch(img, rng, (120, 112, 98), (170, 150, 110))
    return img


def tech(name: str, rng: random.Random) -> Image:
    img = Image()
    panel = (44, 56, 84)
    cyan = (60, 210, 230)
    if name == "wall":
        noisy_fill(img, rng, panel, 4)
        img.frame(0, 0, SIZE, SIZE, (24, 30, 46))
        img.frame(2, 2, 28, 28, shade(panel, 1.3))
        img.rect(4, 26, 24, 1, cyan)
    elif name == "wall_alt_a":  # circuit traces
        noisy_fill(img, rng, (28, 50, 40), 4)
        for _ in range(7):
            x, y = rng.randrange(SIZE), rng.randrange(SIZE)
            horizontal = rng.random() < 0.5
            for _ in range(rng.randint(6, 18)):
                img.set(x, y, (90, 200, 120))
                if horizontal:
                    x = (x + 1) % SIZE
                else:
                    y = (y + 1) % SIZE
            img.rect(x - 1, y - 1, 3, 3, (200, 180, 60))
    elif name == "wall_alt_b":  # server rack with status lights
        noisy_fill(img, rng, (30, 32, 38), 3)
        for y in range(2, SIZE, 6):
            img.rect(2, y, 28, 4, (52, 56, 64))
            for x in range(4, 28, 4):
                if rng.random() < 0.5:
                    img.set(x, y + 1, (60, 230, 90) if rng.random() < 0.7 else (240, 70, 60))
    elif name == "door":  # sliding door with yellow edge stripes
        noisy_fill(img, rng, (70, 80, 96), 4)
        img.rect(15, 0, 2, SIZE, (24, 28, 36))
        for y in range(SIZE):
            if (y // 3) % 2 == 0:
                img.set(0, y, (230, 190, 40))
                img.set(1, y, (230, 190, 40))
                img.set(30, y, (230, 190, 40))
                img.set(31, y, (230, 190, 40))
        img.rect(6, 14, 4, 4, cyan)
        img.rect(22, 14, 4, 4, cyan)
    elif name == "floor":  # grid tiles
        noisy_fill(img, rng, (50, 54, 62), 3)
        for i in range(0, SIZE, 8):
            img.rect(i, 0, 1, SIZE, (30, 34, 40))
            img.rect(0, i, SIZE, 1, (30, 34, 40))
    elif name == "ceiling":  # light panels
        noisy_fill(img, rng, (36, 40, 50), 3)
        img.rect(4, 4, 24, 10, (170, 200, 220))
        img.frame(4, 4, 24, 10, (60, 70, 90))
        img.rect(4, 20, 24, 2, (60, 70, 90))
    elif name == "exit":
        exit_switch(img, rng, (52, 64, 92), (120, 200, 220))
    return img


def classic(name: str, rng: random.Random) -> Image:
    # The classic theme reuses the original textures; only its exit switch and locked doors
    # are generated (the locked doors on a plain grey door).
    if name.startswith("door_"):
        base = Image()
        noisy_fill(base, rng, (112, 112, 120), 5)
        base.frame(0, 0, SIZE, SIZE, (60, 60, 66))
        return base
    return exit_switch_image(rng, (110, 110, 120), (180, 180, 190))


def exit_switch_image(rng: random.Random, plate: Color, trim: Color) -> Image:
    img = Image()
    exit_switch(img, rng, plate, trim)
    return img


LOCKED_DOOR_SLOTS = [f"door_{color}" for color in LOCK_COLORS]
SLOTS = ["wall", "wall_alt_a", "wall_alt_b", "door", "floor", "ceiling", "exit"] + LOCKED_DOOR_SLOTS
THEMES: Dict[str, Tuple[Callable[[str, random.Random], Image], List[str]]] = {
    "bunker": (bunker, SLOTS),
    "stone": (stone, SLOTS),
    "tech": (tech, SLOTS),
    "classic": (classic, ["exit"] + LOCKED_DOOR_SLOTS),
}


def generate() -> Dict[str, str]:
    """Returns {"<theme>/<slot>.ppm": PPM text} for every generated texture."""
    files: Dict[str, str] = {}
    for theme, (draw, slots) in THEMES.items():
        for slot in slots:
            key = f"{theme}/{slot}"
            if slot in LOCKED_DOOR_SLOTS:
                # The theme's own door (same seed, so the same pixels) plus the lock band.
                door_key = f"{theme}/door"
                base = draw("door" if theme != "classic" else slot,
                            random.Random(SEED ^ zlib.crc32(door_key.encode("utf-8"))))
                color = slot[len("door_"):]
                image = locked_door(base, LOCK_COLORS[color], color[0].upper())
            else:
                rng = random.Random(SEED ^ zlib.crc32(key.encode("utf-8")))
                image = draw(slot, rng)
            files[f"{key}.ppm"] = image.ppm(f"Neo-Wolf theme '{theme}', {slot}: generated by "
                                            f"tools/devkit/gen_textures.py (seed {SEED})")
    return files


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="output folder (default: game/assets/themes)")
    parser.add_argument("--check", action="store_true", help="compare with the files on disk instead of writing")
    args = parser.parse_args(argv)

    files = generate()
    if args.check:
        stale = []
        for rel, text in files.items():
            path = args.out / rel
            if not path.is_file() or path.read_text(encoding="utf-8") != text:
                stale.append(rel)
        if stale:
            print("Out of date (run tools/devkit/gen_textures.py): " + ", ".join(stale))
            return 1
        print(f"All {len(files)} generated textures are up to date.")
        return 0

    for rel, text in files.items():
        path = args.out / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        # newline="\n": identical bytes on Windows.
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
    print(f"Wrote {len(files)} textures to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
