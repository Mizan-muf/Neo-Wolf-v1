"""Engine/DevKit parity: the DevKit's readers must accept and reject exactly the files the engine
does (CRLF, whitespace-only lines, comments, byte order marks, number syntax, strict JSON).

tests/fixtures/parity/expected.json records the expected result of every fixture file; the
native test `content_parity_fixtures` checks the engine's loaders against the same file, so a
difference between the two sides fails one of the two tests."""

import json
import unittest
from pathlib import Path

from tools.devkit.engine_formats import (
    check_light_bounds,
    light_grid,
    parse_c_double,
    parse_c_integer,
    parse_entities,
    parse_map,
)
from tools.devkit.map_tools import load_map
from tools.devkit.validator import validate_entity_csv, validate_entity_json, validate_map

PARITY_DIR = Path(__file__).resolve().parents[3] / "tests" / "fixtures" / "parity"


class TestParity(unittest.TestCase):
    def setUp(self) -> None:
        self.expected = json.loads((PARITY_DIR / "expected.json").read_text(encoding="utf-8"))

    def test_fixtures_are_byte_exact(self) -> None:
        # .gitattributes keeps these files as they are; a checkout that converted line endings
        # would make the CRLF fixtures test nothing.
        self.assertIn(b"\r\n", (PARITY_DIR / "crlf.map").read_bytes())
        self.assertNotIn(b"\r", (PARITY_DIR / "lf.map").read_bytes())
        self.assertIn(b"\r", (PARITY_DIR / "cr_only.map").read_bytes())

    def test_maps_match_the_engine(self) -> None:
        self.assertGreaterEqual(len(self.expected["maps"]), 10)
        for name, want in self.expected["maps"].items():
            with self.subTest(map=name):
                parsed = parse_map(PARITY_DIR / name)
                self.assertEqual(parsed.ok, want["ok"], parsed.errors)
                if want["ok"]:
                    self.assertEqual((parsed.width, parsed.height), (want["width"], want["height"]))
                    # The map tools and the validator read the same rows.
                    self.assertEqual(load_map(PARITY_DIR / name).width, want["width"])
                    report, _ = validate_map(PARITY_DIR / name)
                    self.assertTrue(report.is_valid, report.errors)
                else:
                    self.assertIsNone(validate_map(PARITY_DIR / name)[1])

    def test_entity_files_match_the_engine(self) -> None:
        self.assertGreaterEqual(len(self.expected["entities"]), 12)
        for name, want in self.expected["entities"].items():
            with self.subTest(entities=name):
                parsed = parse_entities(PARITY_DIR / name)
                self.assertEqual(parsed.ok, want["ok"], parsed.errors)
                if want["ok"]:
                    self.assertEqual([e["type"] for e in parsed.entities], want["types"])
                    self.assertEqual(
                        [[z["x"], z["y"], z["w"], z["h"], z["level"]] for z in parsed.lights], want.get("lights", [])
                    )
                    validate = validate_entity_json if name.endswith(".json") else validate_entity_csv
                    report, entities = validate(PARITY_DIR / name)
                    self.assertTrue(report.is_valid, report.errors)
                    self.assertEqual(len(entities), len(want["types"]))
                else:
                    validate = validate_entity_json if name.endswith(".json") else validate_entity_csv
                    self.assertFalse(validate(PARITY_DIR / name)[0].is_valid)

    def test_levels_match_the_engine(self) -> None:
        # A map with an entity file: the level builds (light zones inside the map) and the light
        # level of every cell afterwards, like BuildLevelFromPlacements.
        self.assertGreaterEqual(len(self.expected["levels"]), 4)
        for name, want in self.expected["levels"].items():
            with self.subTest(level=name):
                parsed_map = parse_map(PARITY_DIR / want["map"])
                parsed = parse_entities(PARITY_DIR / want["entities"])
                self.assertTrue(parsed_map.ok and parsed.ok)
                errors = check_light_bounds(parsed.lights, parsed_map.width, parsed_map.height, want["entities"])
                self.assertEqual(not errors, want["ok"], errors)
                report, _ = validate_entity_json(PARITY_DIR / want["entities"], (parsed_map.width, parsed_map.height))
                self.assertEqual(report.is_valid, want["ok"], report.errors)
                if want["ok"]:
                    grid = light_grid(parsed_map.width, parsed_map.height, parsed.lights)
                    self.assertEqual(len(grid), len(want["light"]))
                    for row, wanted in zip(grid, want["light"]):
                        self.assertEqual(len(row), len(wanted))
                        for value, expected in zip(row, wanted):
                            self.assertAlmostEqual(value, expected, places=6)

    def test_c_number_syntax(self) -> None:
        # strtod/strtoll accept these ...
        for text, value in (("1.5", 1.5), ("-2", -2.0), ("+.5", 0.5), ("1e3", 1000.0), ("15E-1", 1.5), ("0x10", 16.0)):
            self.assertEqual(parse_c_double(text), value, text)
        self.assertEqual(parse_c_integer("+7"), 7)
        self.assertEqual(parse_c_integer("-12"), -12)
        # ... and reject what Python's float()/int() would take or what the engine refuses.
        for text in ("1_5", "1.5 ", " 1.5", "inf", "nan", "1e999", "", "1.5.2", "١"):
            self.assertIsNone(parse_c_double(text), repr(text))
        for text in ("1.0", "1_0", "0x10", "", "1e3"):
            self.assertIsNone(parse_c_integer(text), repr(text))


if __name__ == "__main__":
    unittest.main()
