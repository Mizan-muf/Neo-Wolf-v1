"""The theme texture generator is deterministic and the committed textures match it."""

import unittest

from tools.devkit import gen_textures


class TestGenTextures(unittest.TestCase):
    def test_deterministic(self) -> None:
        first = gen_textures.generate()
        second = gen_textures.generate()
        self.assertEqual(first, second)
        # 3 full themes (7 textures each) + the classic theme's exit switch.
        self.assertEqual(len(first), 22)
        for name, text in first.items():
            lines = text.splitlines()
            self.assertEqual(lines[0], "P3", name)
            self.assertEqual(lines[2], "32 32", name)

    def test_committed_textures_are_up_to_date(self) -> None:
        # Fails when someone edits the generator without regenerating (or edits a file by hand).
        self.assertEqual(gen_textures.main(["--check"]), 0)


if __name__ == "__main__":
    unittest.main()
