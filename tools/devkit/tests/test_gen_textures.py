"""The theme texture generator is deterministic and the committed textures match it."""

import unittest

from tools.devkit import gen_textures


class TestGenTextures(unittest.TestCase):
    def test_deterministic(self) -> None:
        first = gen_textures.generate()
        second = gen_textures.generate()
        self.assertEqual(first, second)
        # 4 full themes (bunker, stone, tech, classic), 10 textures each including the 3 locked
        # doors.
        self.assertEqual(len(first), 40)
        for name, text in first.items():
            lines = text.splitlines()
            self.assertEqual(lines[0], "P3", name)
            self.assertEqual(lines[2], "32 32", name)

    def test_locked_doors_are_the_theme_door_plus_a_band(self) -> None:
        files = gen_textures.generate()
        for theme in ("bunker", "stone", "tech", "classic"):
            door = files[f"{theme}/door.ppm"].splitlines()[4:]
            red = files[f"{theme}/door_red.ppm"].splitlines()[4:]
            blue = files[f"{theme}/door_blue.ppm"].splitlines()[4:]
            self.assertEqual(door[0], red[0], theme)  # the top row is the plain door
            self.assertNotEqual(door[15], red[15], theme)  # the band across the middle
            self.assertNotEqual(red[15], blue[15], theme)  # differs per colour

    def test_classic_door_differs_from_every_wall(self) -> None:
        # Phase 8: the old classic theme used its B wall as the door, so doors did not read as
        # doors. Every theme's door must differ from its three wall textures.
        files = gen_textures.generate()
        for theme in ("bunker", "stone", "tech", "classic"):
            door = files[f"{theme}/door.ppm"].splitlines()[4:]
            for wall in ("wall", "wall_alt_a", "wall_alt_b"):
                self.assertNotEqual(door, files[f"{theme}/{wall}.ppm"].splitlines()[4:], f"{theme}/{wall}")

    def test_committed_textures_are_up_to_date(self) -> None:
        # Fails when someone edits the generator without regenerating (or edits a file by hand).
        self.assertEqual(gen_textures.main(["--check"]), 0)


if __name__ == "__main__":
    unittest.main()
