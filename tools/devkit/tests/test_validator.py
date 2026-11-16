"""Unit tests for the DevKit content and asset validator."""

import json
import tempfile
import unittest
from pathlib import Path

from tools.devkit.validator import (
    analyse_level_progress,
    find_reachable_exits,
    validate_campaign,
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
        map_content = "######\n#....#\n#.A..#\n#..D.#\n#...T#\n######\n"
        map_file = self.dir_path / "valid.map"
        map_file.write_text(map_content, encoding="utf-8")

        report, rows = validate_map(map_file)
        self.assertTrue(report.is_valid)
        self.assertEqual(len(report.errors), 0)
        self.assertIsNotNone(rows)
        assert rows is not None
        self.assertEqual(len(rows), 6)
        self.assertEqual(len(rows[0]), 6)

    def test_exit_switch_is_a_valid_solid_tile(self) -> None:
        # 'X' is accepted and closes the perimeter like a wall.
        map_file = self.dir_path / "exit.map"
        map_file.write_text("#####\n#..DX\n#####\n", encoding="utf-8")
        report, rows = validate_map(map_file)
        self.assertTrue(report.is_valid, report.errors)
        self.assertIsNotNone(rows)

    def test_exit_reachability(self) -> None:
        rows = ["#######", "#..#..X", "#..D..#", "#######"]
        # Through the door the exit is usable.
        self.assertEqual(find_reachable_exits(rows, 1.5, 1.5), (1, 1))
        # Walled off: present but not reachable.
        walled = ["#######", "#..#..X", "#..#..#", "#######"]
        self.assertEqual(find_reachable_exits(walled, 1.5, 1.5), (1, 0))
        self.assertEqual(find_reachable_exits(["####", "#..#", "####"], 1.5, 1.5), (0, 0))

    def test_key_before_lock_reachability(self) -> None:
        # Spawn | blue door | room with the red key | red door | exit.
        rows = ["##########", "#..D..D..X", "##########"]
        player = {"type": "player", "x": 1.5, "y": 1.5}
        blue_door = {"type": "door", "x": 3.5, "y": 1.5, "mapX": 3, "mapY": 1, "lock": "blue"}
        red_door = {"type": "door", "x": 6.5, "y": 1.5, "mapX": 6, "mapY": 1, "lock": "red"}
        blue_key = {"type": "pickup", "x": 2.5, "y": 1.5, "kind": "key", "key": "blue"}
        red_key = {"type": "pickup", "x": 5.5, "y": 1.5, "kind": "key", "key": "red"}

        good = analyse_level_progress(rows, [player, blue_door, red_door, blue_key, red_key], 1.5, 1.5)
        self.assertEqual(good.problems, [])
        self.assertEqual(good.key_order, ["blue", "red"])
        self.assertEqual((good.total_exits, good.usable_exits), (1, 1))

        # The red key behind its own door: never reachable, and the exit is not either.
        behind = dict(red_key, x=7.5)
        bad = analyse_level_progress(rows, [player, blue_door, red_door, blue_key, behind], 1.5, 1.5)
        self.assertEqual(bad.usable_exits, 0)
        self.assertTrue(any("red key cannot be reached" in p for p in bad.problems), bad.problems)

        # A lock colour with no key at all.
        missing = analyse_level_progress(rows, [player, blue_door, red_door, blue_key], 1.5, 1.5)
        self.assertTrue(any("needs the red key, but the level has no red key" in p for p in missing.problems))

        # Without locks every door is passable (the pre-Phase 6 rule).
        unlocked = analyse_level_progress(
            rows,
            [player, dict(blue_door, lock=None), {"type": "door", "x": 6.5, "y": 1.5, "mapX": 6, "mapY": 1}],
            1.5,
            1.5,
        )
        self.assertEqual((unlocked.usable_exits, unlocked.problems), (1, []))

    def test_campaign_reports_lock_problems(self) -> None:
        root = self._write_campaign("#######\n#..D..X\n#######\n")
        (root / "game" / "defs" / "l1.entities.json").write_text(
            json.dumps(
                {
                    "entities": [
                        {"type": "player", "x": 1.5, "y": 1.5},
                        {"type": "door", "x": 3.5, "y": 1.5, "mapX": 3, "mapY": 1, "lock": "gold"},
                    ]
                }
            ),
            encoding="utf-8",
        )
        report = validate_campaign(root)
        self.assertTrue(any("needs the gold key" in e for e in report.errors), report.errors)
        self.assertTrue(any("with the keys that can be collected first" in e for e in report.errors), report.errors)

    def test_entity_variants_keys_and_locks(self) -> None:
        json_file = self.dir_path / "v.entities.json"
        json_file.write_text(
            json.dumps(
                {
                    "entities": [
                        {"type": "player", "x": 1.5, "y": 1.5},
                        {"type": "enemy", "x": 2.0, "y": 2.0, "variant": "brute"},
                        {"type": "enemy", "x": 2.0, "y": 2.0, "variant": "ninja"},
                        {"type": "pickup", "x": 2.0, "y": 2.0, "kind": "key", "key": "blue"},
                        {"type": "pickup", "x": 2.0, "y": 2.0, "kind": "key", "key": "green"},
                        {"type": "pickup", "x": 2.0, "y": 2.0, "kind": "key"},
                        {"type": "door", "x": 3.5, "y": 1.5, "mapX": 3, "mapY": 1, "lock": "blue"},
                        {"type": "door", "x": 3.5, "y": 1.5, "mapX": 3, "mapY": 1, "lock": "purple"},
                        {"type": "pickup", "x": 2.0, "y": 2.0, "kind": "ammo", "key": "red"},
                    ]
                }
            ),
            encoding="utf-8",
        )
        report, entities = validate_entity_json(json_file, (8, 8))
        self.assertEqual(len(report.errors), 4, report.errors)
        self.assertTrue(
            any("entities[2]" in e and "variant must be 'guard', 'brute' or 'runner'" in e for e in report.errors)
        )
        self.assertTrue(any("entities[4]" in e and "'green'" in e for e in report.errors))
        self.assertTrue(any("entities[5]" in e and "needs 'key'" in e for e in report.errors))
        self.assertTrue(any("entities[7]" in e and "lock must be" in e for e in report.errors))
        self.assertTrue(
            any("entities[8]" in w and "only applies to pickups of kind 'key'" in w for w in report.warnings)
        )
        self.assertEqual(len(entities), 5)

    def test_sprites_cfg_renamed_keys(self) -> None:
        root = Path(__file__).resolve().parents[3]
        report = validate_project(root)
        self.assertTrue(report.is_valid, report.errors)
        self.assertFalse(any("sprites.cfg" in w for w in report.warnings), report.warnings)

    def _write_campaign(self, map_text: str) -> Path:
        root = self.dir_path / "content"
        (root / "game" / "maps").mkdir(parents=True)
        (root / "game" / "defs" / "themes").mkdir(parents=True)
        (root / "game" / "defs" / "themes" / "t.cfg").write_text("wall=x\n", encoding="utf-8")
        (root / "game" / "maps" / "l1.map").write_text(map_text, encoding="utf-8")
        (root / "game" / "defs" / "l1.entities.json").write_text(
            json.dumps({"entities": [{"type": "player", "x": 1.5, "y": 1.5}]}), encoding="utf-8"
        )
        (root / "game" / "defs" / "campaign.json").write_text(
            json.dumps(
                {
                    "title": "T",
                    "levels": [
                        {
                            "id": "l1",
                            "name": "One",
                            "map": "game/maps/l1.map",
                            "entities": "game/defs/l1.entities.json",
                            "theme": "t",
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        return root

    def test_campaign_requires_reachable_exit(self) -> None:
        good = validate_campaign(self._write_campaign("#####\n#..X#\n#####\n"))
        self.assertTrue(good.is_valid, good.errors)

    def test_campaign_without_exit_fails(self) -> None:
        report = validate_campaign(self._write_campaign("#####\n#...#\n#####\n"))
        self.assertTrue(any("no exit switch" in e for e in report.errors), report.errors)

    def test_campaign_unreachable_exit_fails(self) -> None:
        report = validate_campaign(self._write_campaign("######\n#..#.X\n######\n"))
        self.assertTrue(any("can be reached" in e for e in report.errors), report.errors)

    def _light_file(self, lights, name: str = "lights.entities.json") -> Path:
        path = self.dir_path / name
        path.write_text(
            json.dumps({"level": "l", "entities": [{"type": "player", "x": 1.5, "y": 1.5}], "lights": lights}),
            encoding="utf-8",
        )
        return path

    def test_light_zones_valid(self) -> None:
        # Known issue C21: "lights" is a known top-level field (no warning); the range is
        # inclusive at both ends; a zone may cover the whole map.
        path = self._light_file(
            [{"x": 0, "y": 0, "w": 10, "h": 10, "level": 0.2}, {"x": 2, "y": 3, "w": 1, "h": 1, "level": 1}]
        )
        report, _ = validate_entity_json(path, map_dimensions=(10, 10))
        self.assertEqual((report.errors, report.warnings), ([], []))
        self.assertTrue(any("2 light zone(s)" in line for line in report.info), report.info)

    def test_light_zones_invalid(self) -> None:
        cases = [
            (
                {"x": 1, "y": 1, "w": 1, "h": 1, "level": 0.19},
                "lights[0]: field 'level' must be a number from 0.2 to 1.0, got 0.19",
            ),
            ({"x": 1, "y": 1, "w": 1, "h": 1, "level": 1.01}, "must be a number from 0.2 to 1.0, got 1.01"),
            ({"x": 1, "y": 1, "w": 1, "h": 1, "level": True}, "must be a number from 0.2 to 1.0, got True"),
            ({"x": 1, "y": 1, "w": 1, "h": 1, "level": "dim"}, "got 'dim'"),
            ({"x": 1, "y": 1, "w": 1, "h": 1}, "field 'level' is required"),
            ({"x": 1, "y": 1, "w": 0, "h": 1, "level": 0.5}, "field 'w' must be an integer from 1 to 100000, got 0"),
            ({"x": -1, "y": 1, "w": 1, "h": 1, "level": 0.5}, "field 'x' must be an integer from 0 to 100000, got -1"),
            (
                {"x": 1, "y": 1.0, "w": 1, "h": 1, "level": 0.5},
                "field 'y' must be an integer from 0 to 100000, got 1.0",
            ),
            ({"x": 1, "y": 1, "w": 1, "level": 0.5}, "field 'h' is required"),
            (7, "lights[0]: each light zone must be an object, got 7"),
            (
                {"x": 8, "y": 1, "w": 3, "h": 1, "level": 0.5},
                "lights[0]: cells (8, 1) to (10, 1) are not all inside the 10x10 map",
            ),
            (
                {"x": 1, "y": 9, "w": 1, "h": 2, "level": 0.5},
                "cells (1, 9) to (1, 10) are not all inside the 10x10 map",
            ),
        ]
        for zone, message in cases:
            with self.subTest(zone=zone):
                report, _ = validate_entity_json(self._light_file([zone]), map_dimensions=(10, 10))
                self.assertFalse(report.is_valid)
                self.assertTrue(any(message in error for error in report.errors), report.errors)
        report, _ = validate_entity_json(self._light_file({"x": 1}), map_dimensions=(10, 10))
        self.assertTrue(any("field 'lights' must be an array" in error for error in report.errors), report.errors)
        report, _ = validate_entity_json(self._light_file([{"x": 1, "y": 1, "w": 1, "h": 1, "level": 0.5, "r": 1}]))
        self.assertTrue(report.is_valid, report.errors)
        self.assertTrue(any("lights[0]: unknown field 'r' ignored" in w for w in report.warnings), report.warnings)

    def test_campaign_checks_light_zones(self) -> None:
        root = self._write_campaign("#####\n#..X#\n#####\n")
        entities = root / "game" / "defs" / "l1.entities.json"
        document = {
            "entities": [{"type": "player", "x": 1.5, "y": 1.5}],
            "lights": [{"x": 1, "y": 1, "w": 2, "h": 1, "level": 0.35}],
        }
        entities.write_text(json.dumps(document), encoding="utf-8")
        report = validate_campaign(root)
        self.assertEqual((report.errors, report.warnings), ([], []))
        self.assertTrue(any("1 light zone(s)" in line for line in report.info), report.info)
        document["lights"].append({"x": 3, "y": 1, "w": 3, "h": 1, "level": 0.35})
        entities.write_text(json.dumps(document), encoding="utf-8")
        report = validate_campaign(root)
        self.assertTrue(
            any("lights[1]: cells (3, 1) to (5, 1) are not all inside the 5x3 map" in e for e in report.errors),
            report.errors,
        )

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
            "#Z.#\n"  # 'Z' is invalid ('X' is the exit switch since Phase 5)
            "#..#\n"
            "####\n"
        )
        map_file = self.dir_path / "invalid_char.map"
        map_file.write_text(map_content, encoding="utf-8")

        report, _ = validate_map(map_file)
        self.assertFalse(report.is_valid)
        self.assertTrue(any("Invalid tile character 'Z'" in err for err in report.errors))

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
        csv_content = "# type,x,y,mapX,mapY\nplayer,2.5,2.5\nenemy,5.0,5.0\ndoor,3.5,4.5,3,4\n"
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
