"""Unit tests for DevKit definition tools (CSV/JSON sync)."""

import json
import tempfile
import unittest
from pathlib import Path

from tools.devkit.def_tools import (
    csv_to_json,
    json_to_csv,
    load_entity_csv,
    load_entity_json,
    save_entity_csv,
    save_entity_json,
)


class TestDefTools(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.dir_path = Path(self.temp_dir.name)

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def test_csv_json_roundtrip(self) -> None:
        csv_file = self.dir_path / "test.entities.csv"
        json_file = self.dir_path / "test.entities.json"

        csv_content = (
            "# type,x,y,mapX,mapY\n"
            "player,2.5,2.5\n"
            "enemy,6.5,2.8\n"
            "door,8.5,7.5,8,7\n"
        )
        csv_file.write_text(csv_content, encoding="utf-8")

        csv_to_json(csv_file, json_file, level_id="test")
        self.assertTrue(json_file.is_file())

        lvl, ents = load_entity_json(json_file)
        self.assertEqual(lvl, "test")
        self.assertEqual(len(ents), 3)
        self.assertEqual(ents[0]["type"], "player")
        self.assertEqual(ents[2]["mapX"], 8)

        # Convert back
        csv_back = self.dir_path / "back.entities.csv"
        json_to_csv(json_file, csv_back)
        ents_back = load_entity_csv(csv_back)
        self.assertEqual(len(ents_back), 3)
        self.assertEqual(ents_back[0]["type"], "player")
        self.assertEqual(ents_back[2]["mapX"], 8)


if __name__ == "__main__":
    unittest.main()
