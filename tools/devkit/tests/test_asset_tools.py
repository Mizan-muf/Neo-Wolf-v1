"""Unit tests for DevKit asset tools (inspection, generation)."""

import tempfile
import unittest
from pathlib import Path

from tools.devkit.asset_tools import (
    create_brick_ppm,
    create_checkerboard_ppm,
    create_solid_ppm,
    generate_beep_wav,
    inspect_ppm,
    inspect_wav,
)


class TestAssetTools(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.dir_path = Path(self.temp_dir.name)

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def test_checkerboard_and_inspect_ppm(self) -> None:
        ppm_file = self.dir_path / "checker.ppm"
        create_checkerboard_ppm(ppm_file, width=16, height=16)
        self.assertTrue(ppm_file.is_file())

        meta = inspect_ppm(ppm_file)
        self.assertEqual(meta["width"], 16)
        self.assertEqual(meta["height"], 16)
        self.assertEqual(meta["format"], "P3")
        self.assertTrue(meta["is_power_of_two"])
        self.assertEqual(meta["total_pixels"], 256)

    def test_solid_and_brick_ppm(self) -> None:
        solid_file = self.dir_path / "solid.ppm"
        create_solid_ppm(solid_file, width=8, height=8, color=(100, 150, 200))
        meta_s = inspect_ppm(solid_file)
        self.assertEqual(meta_s["average_color_rgb"], (100.0, 150.0, 200.0))

        brick_file = self.dir_path / "brick.ppm"
        create_brick_ppm(brick_file, width=16, height=16)
        self.assertTrue(brick_file.is_file())

    def test_generate_and_inspect_wav(self) -> None:
        wav_file = self.dir_path / "beep.wav"
        generate_beep_wav(wav_file, duration_sec=0.1, frequency=440.0, sample_rate=22050)
        self.assertTrue(wav_file.is_file())

        meta = inspect_wav(wav_file)
        self.assertEqual(meta["channels"], 1)
        self.assertEqual(meta["sample_rate"], 22050)
        self.assertEqual(meta["bits_per_sample"], 16)
        self.assertAlmostEqual(meta["duration_seconds"], 0.1, delta=0.02)


if __name__ == "__main__":
    unittest.main()
