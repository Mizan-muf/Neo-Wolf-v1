"""Unit tests for the DevKit content and asset validator."""

import json
import tempfile
import unittest
from pathlib import Path

from tools.devkit.validator import (
    validate_cfg,
    validate_entity_csv,
    validate_entity_json,
    validate_map,
    validate_ppm,
    validate_project,
    validate_wav,
)


class TestValidator(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.dir_path = Path(self.temp_dir.name)

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def test_validate_valid_map(self) -> None:
        map_content = (
            "######\n"
            "#....#\n"
            "#.A..#\n"
            "#..D.#\n"
            "#...T#\n"
            "######\n"
        )
        map_file = self.dir_path / "valid.map"
        map_file.write_text(map_content, encoding="utf-8")

        report, rows = validate_map(map_file)
        self.assertTrue(report.is_valid)
        self.assertEqual(len(report.errors), 0)
        self.assertIsNotNone(rows)
        assert rows is not None
        self.assertEqual(len(rows), 6)
        self.assertEqual(len(rows[0]), 6)

    def test_validate_unclosed_map(self) -> None:
        map_content = (
            "######\n"
            "#....#\n"
            ".....#\n"  # West wall missing
            "######\n"
        )
        map_file = self.dir_path / "unclosed.map"
        map_file.write_text(map_content, encoding="utf-8")

        report, rows = validate_map(map_file)
        self.assertFalse(report.is_valid)
        self.assertTrue(any("West perimeter leak" in err for err in report.errors))

    def test_validate_invalid_character_map(self) -> None:
        map_content = (
            "####\n"
            "#X.#\n"  # 'X' is invalid
            "#..#\n"
            "####\n"
        )
        map_file = self.dir_path / "invalid_char.map"
        map_file.write_text(map_content, encoding="utf-8")

        report, _ = validate_map(map_file)
        self.assertFalse(report.is_valid)
        self.assertTrue(any("Invalid tile character 'X'" in err for err in report.errors))

    def test_validate_inconsistent_width_map(self) -> None:
        map_content = (
            "#####\n"
            "#...\n"  # Too short
            "#####\n"
        )
        map_file = self.dir_path / "ragged.map"
        map_file.write_text(map_content, encoding="utf-8")

        report, _ = validate_map(map_file)
        self.assertFalse(report.is_valid)
        self.assertTrue(any("Inconsistent row width" in err for err in report.errors))

    def test_validate_valid_ppm(self) -> None:
        ppm_content = "P3\n# test\n2 2\n255\n255 0 0  0 255 0\n0 0 255  255 255 255\n"
        ppm_file = self.dir_path / "test.ppm"
        ppm_file.write_text(ppm_content, encoding="latin-1")

        report = validate_ppm(ppm_file)
        self.assertTrue(report.is_valid)
        self.assertEqual(len(report.errors), 0)

    def test_validate_invalid_ppm_token_count(self) -> None:
        ppm_content = "P3\n2 2\n255\n255 0 0  0 255 0\n"  # Only 2 pixels instead of 4
        ppm_file = self.dir_path / "bad.ppm"
        ppm_file.write_text(ppm_content, encoding="latin-1")

        report = validate_ppm(ppm_file)
        self.assertFalse(report.is_valid)
        self.assertTrue(any("Token count mismatch" in err for err in report.errors))

    def test_validate_entity_csv(self) -> None:
        csv_content = (
            "# type,x,y,mapX,mapY\n"
            "player,2.5,2.5\n"
            "enemy,5.0,5.0\n"
            "door,3.5,4.5,3,4\n"
        )
        csv_file = self.dir_path / "test.entities.csv"
        csv_file.write_text(csv_content, encoding="utf-8")

        report, ents = validate_entity_csv(csv_file, map_dimensions=(10, 10))
        self.assertTrue(report.is_valid)
        self.assertEqual(len(ents), 3)

    def test_validate_entity_csv_out_of_bounds(self) -> None:
        csv_content = "player,15.5,2.5\n"
        csv_file = self.dir_path / "oob.entities.csv"
        csv_file.write_text(csv_content, encoding="utf-8")

        report, _ = validate_entity_csv(csv_file, map_dimensions=(10, 10))
        self.assertFalse(report.is_valid)
        self.assertTrue(any("outside map bounds" in err for err in report.errors))

    def test_validate_entity_json(self) -> None:
        json_data = {
            "level": "level01",
            "entities": [
                {"type": "player", "x": 2.5, "y": 2.5},
                {"type": "pickup", "x": 4.0, "y": 4.0, "kind": "health"},
            ],
        }
        json_file = self.dir_path / "test.entities.json"
        json_file.write_text(json.dumps(json_data), encoding="utf-8")

        report, ents = validate_entity_json(json_file, map_dimensions=(10, 10))
        self.assertTrue(report.is_valid)
        self.assertEqual(len(ents), 2)

    def test_validate_entity_json_phase4_fields(self) -> None:
        json_data = {
            "level": "level01",
            "entities": [
                {"type": "player", "x": 2.5, "y": 2.5, "angle": 90},
                {"type": "enemy", "x": 5.0, "y": 5.0, "variant": "guard"},
                {"type": "pickup", "x": 4.0, "y": 4.0, "kind": "ammo", "value": 30},
                {"type": "door", "x": 3.5, "y": 4.5, "mapX": 3, "mapY": 4},
                {"type": "decoration", "x": 6.0, "y": 6.0},
            ],
        }
        json_file = self.dir_path / "fields.entities.json"
        json_file.write_text(json.dumps(json_data), encoding="utf-8")

        report, ents = validate_entity_json(json_file, map_dimensions=(10, 10))
        self.assertTrue(report.is_valid, report.errors)
        self.assertEqual(report.warnings, [])
        self.assertEqual(len(ents), 5)

    def test_validate_entity_json_requires_pickup_kind(self) -> None:
        json_data = {
            "entities": [
                {"type": "player", "x": 2.5, "y": 2.5},
                {"type": "pickup", "x": 4.0, "y": 4.0},
                {"type": "pickup", "x": 5.0, "y": 4.0, "kind": "armor"},
                {"type": "door", "x": 3.5, "y": 4.5},
            ],
        }
        json_file = self.dir_path / "nokind.entities.json"
        json_file.write_text(json.dumps(json_data), encoding="utf-8")

        report, _ = validate_entity_json(json_file, map_dimensions=(10, 10))
        self.assertFalse(report.is_valid)
        self.assertTrue(any("entities[1]" in err and "kind is required" in err for err in report.errors))
        self.assertTrue(any("entities[2]" in err and "'armor'" in err for err in report.errors))
        self.assertTrue(any("entities[3]" in err and "mapX and mapY" in err for err in report.errors))

    def test_validate_entity_csv_kind_column(self) -> None:
        csv_file = self.dir_path / "kind.entities.csv"
        csv_file.write_text(
            "# type,x,y,mapX,mapY,kind,value,angle,variant\n"
            "player,2.5,2.5,,,,,45\n"
            "pickup,4.0,4.0,,,ammo,20\n"
            "pickup,5.0,4.0\n",
            encoding="utf-8",
        )
        report, ents = validate_entity_csv(csv_file, map_dimensions=(10, 10))
        self.assertFalse(report.is_valid)  # the last pickup has no kind
        self.assertEqual(len(ents), 2)
        self.assertEqual(ents[1]["kind"], "ammo")
        self.assertEqual(ents[1]["value"], 20)
        self.assertTrue(any(":4 (entity 2)" in err and "kind is required" in err for err in report.errors))


if __name__ == "__main__":
    unittest.main()
