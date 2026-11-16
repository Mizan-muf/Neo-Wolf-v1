"""Every file under game/assets is made by a repository generator, byte for byte (known issue
X5): an asset of unknown origin, or one edited by hand, fails this test."""

import tempfile
import unittest
from pathlib import Path

from tools.devkit import asset_provenance


class TestAssetProvenance(unittest.TestCase):
    def test_every_asset_is_generated(self) -> None:
        report = asset_provenance.check()
        self.assertEqual(report.unknown, [], "assets that no generator makes")
        self.assertEqual(report.stale, [], "assets that differ from their generator's output")
        scripts = set(report.matched.values())
        self.assertEqual(scripts, {g.script for g in asset_provenance.MANIFEST})
        self.assertEqual(asset_provenance.main([]), 0)

    def test_reports_unknown_and_edited_files(self) -> None:
        expected = asset_provenance.expected_files()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for rel, (data, _) in expected.items():
                (root / rel).parent.mkdir(parents=True, exist_ok=True)
                (root / rel).write_bytes(data)
            self.assertEqual(asset_provenance.main(["--assets", tmp]), 0)

            (root / "sprite_extra.ppm").write_text("P3\n1 1\n255\n0 0 0\n", encoding="utf-8")
            edited = "sprites/decoration.ppm"
            (root / edited).write_bytes(expected[edited][0] + b"\n")
            removed = "sfx_pickup.wav"
            (root / removed).unlink()
            (root / ".DS_Store").write_bytes(b"\0")  # ignored

            report = asset_provenance.check(root)
            self.assertEqual(report.unknown, ["sprite_extra.ppm"])
            self.assertEqual(sorted(report.stale), sorted([edited, removed]))
            self.assertEqual(asset_provenance.main(["--assets", tmp]), 1)


if __name__ == "__main__":
    unittest.main()
