"""Every level page (docs/levels/levelNN.md) shows its shipped map byte for byte and an
annotated grid that matches its entity file (tools/devkit/level_docs.py, Phase 8)."""

import shutil
import tempfile
import unittest
from pathlib import Path

from tools.devkit import level_docs


class TestLevelDocs(unittest.TestCase):
    def test_shipped_level_pages_match_their_levels(self) -> None:
        problems = level_docs.check(level_docs.REPO_ROOT)
        self.assertEqual(problems, [], "\n".join(problems))

    def test_annotated_grid_symbols_and_rulers(self) -> None:
        map_text = "; l - test\n#######\n#..D..#\n#.....#\n#######\n"
        entities = {
            "entities": [
                {"type": "player", "x": 1.5, "y": 1.5, "angle": 0},
                {"type": "door", "x": 3.5, "y": 1.5, "mapX": 3, "mapY": 1, "lock": "gold"},
                {"type": "enemy", "x": 4.5, "y": 2.5, "variant": "runner"},
                {"type": "decoration", "x": 4.4, "y": 2.6},  # same cell: the enemy wins
                {"type": "pickup", "x": 5.5, "y": 1.5, "kind": "key", "key": "blue"},
                {"type": "pickup", "x": 2.5, "y": 2.5, "kind": "ammo"},
            ]
        }
        self.assertEqual(
            level_docs.annotated_grid(map_text, entities),
            "   0\n   0123456\n 0 #######\n 1 #p.3.u#\n 2 #.a.n.#\n 3 #######\n",
        )

    def test_mismatches_are_reported_and_grids_rewritten(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for rel in (
                "game/defs/campaign.json",
                "game/maps/level01.map",
                "game/defs/level01.entities.json",
                "docs/levels/level01.md",
            ):
                (root / rel).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(level_docs.REPO_ROOT / rel, root / rel)
            # Only level 1 in this scratch campaign.
            campaign = (root / "game/defs/campaign.json").read_text(encoding="utf-8")
            start = campaign.index('"levels"')
            first_level_end = campaign.index("}", start) + 1
            (root / "game/defs/campaign.json").write_text(campaign[:first_level_end] + "\n  ]\n}\n", encoding="utf-8")
            self.assertEqual(level_docs.check(root), [])

            page = root / "docs/levels/level01.md"
            text = page.read_text(encoding="utf-8")
            # A stale annotated grid (the player drawn as '.') is reported, then fixed by --write.
            page.write_text(text.replace(" 2 #p", " 2 #.", 1), encoding="utf-8")
            self.assertIn("annotated grid is out of date", "\n".join(level_docs.check(root)))
            self.assertEqual(level_docs.check(root, write=True), [])
            self.assertEqual(level_docs.check(root), [])

            # A map edit without the page edit is reported and never rewritten.
            map_path = root / "game/maps/level01.map"
            map_path.write_text(map_path.read_text(encoding="utf-8").replace("X", "B", 1), encoding="utf-8")
            problems = level_docs.check(root, write=True)
            self.assertTrue(any("differs from game/maps/level01.map" in p for p in problems), problems)


if __name__ == "__main__":
    unittest.main()
