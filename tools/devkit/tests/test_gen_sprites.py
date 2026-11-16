"""The sprite generator is deterministic, follows the engine's sprite rules, and the committed
sprites match it."""

import unittest

from tools.devkit import gen_sprites

MAGENTA = (255, 0, 255)


def pixels(text: str):
    lines = text.splitlines()
    width, height = (int(v) for v in lines[2].split())
    values = [int(v) for line in lines[4:] for v in line.split()]
    rgb = [tuple(values[i : i + 3]) for i in range(0, len(values), 3)]
    return width, height, [rgb[y * width : (y + 1) * width] for y in range(height)]


class TestGenSprites(unittest.TestCase):
    def test_deterministic_and_complete(self) -> None:
        first = gen_sprites.generate()
        self.assertEqual(first, gen_sprites.generate())
        expected = {
            "guard",
            "brute",
            "runner",
            "guard_dead",
            "brute_dead",
            "runner_dead",
            "health",
            "ammo",
            "key_red",
            "key_blue",
            "key_gold",
            "decoration",
        }
        self.assertEqual({name[:-4] for name in first}, expected)
        for name, text in first.items():
            width, height, rows = pixels(text)
            self.assertEqual((width, height), (32, 32), name)
            self.assertEqual(text.splitlines()[0], "P3", name)
            self.assertEqual(rows[0][0], MAGENTA, name)  # transparent background (colour key)

    def test_silhouettes_differ(self) -> None:
        """The enemies differ in size and shape, not only colour; corpses lie on the floor."""
        files = gen_sprites.generate()

        def box(name: str):
            _, _, rows = pixels(files[f"{name}.ppm"])
            cells = [(x, y) for y, row in enumerate(rows) for x, c in enumerate(row) if c != MAGENTA]
            xs = [x for x, _ in cells]
            ys = [y for _, y in cells]
            return min(xs), min(ys), max(xs), max(ys), len(cells)

        guard, brute, runner = box("guard"), box("brute"), box("runner")
        for sprite in (guard, brute, runner):
            self.assertEqual(sprite[3], 31)  # feet on the floor (bottom row)
        self.assertLess(brute[1], guard[1])  # the brute is taller ...
        self.assertGreater(brute[4], guard[4] * 1.3)  # ... and much bulkier
        self.assertGreater(runner[1], guard[1] + 4)  # the runner is small
        self.assertLess(runner[4], guard[4])
        for corpse in ("guard_dead", "brute_dead", "runner_dead"):
            self.assertGreaterEqual(box(corpse)[1], 16, corpse)  # lies in the lower half of the frame
        vase = box("decoration")
        self.assertEqual(vase[3], 31)  # stands on the floor
        self.assertGreater(vase[3] - vase[1], 20)  # tall enough to read as a landmark

    def test_committed_sprites_are_up_to_date(self) -> None:
        self.assertEqual(gen_sprites.main(["--check"]), 0)


if __name__ == "__main__":
    unittest.main()
