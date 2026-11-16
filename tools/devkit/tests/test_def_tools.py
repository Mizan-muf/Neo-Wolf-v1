"""Unit tests for DevKit definition tools (CSV/JSON sync)."""

import json
import tempfile
import unittest
from pathlib import Path

from tools.devkit.def_tools import (
    csv_to_json,
    entity_file_pair,
    json_to_csv,
    load_entity_csv,
    load_entity_json,
    save_entity_csv,
    save_entity_json,
    sync_entity_definitions,
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

        csv_content = "# type,x,y,mapX,mapY\nplayer,2.5,2.5\nenemy,6.5,2.8\ndoor,8.5,7.5,8,7\n"
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

    def test_json_to_csv_keeps_phase4_fields(self) -> None:
        entities = [
            {"type": "player", "x": 2.5, "y": 2.5, "angle": 90.0},
            {"type": "enemy", "x": 6.5, "y": 2.8, "variant": "guard"},
            {"type": "pickup", "x": 11.8, "y": 6.6, "kind": "ammo", "value": 30},
            {"type": "pickup", "x": 3.0, "y": 3.0, "kind": "health"},
            {"type": "door", "x": 8.5, "y": 7.5, "mapX": 8, "mapY": 7},
        ]
        json_file = self.dir_path / "lvl.entities.json"
        csv_file = self.dir_path / "lvl.entities.csv"
        save_entity_json("lvl", entities, json_file)
        json_to_csv(json_file, csv_file)

        lines = csv_file.read_text(encoding="utf-8").splitlines()
        self.assertEqual(lines[0], "# type,x,y,mapX,mapY,kind,value,angle,variant,key,lock")
        self.assertEqual(lines[3], "pickup,11.8,6.6,,,ammo,30")
        self.assertEqual(lines[4], "pickup,3.0,3.0,,,health")
        self.assertEqual(load_entity_csv(csv_file), entities)

    def test_sync_defaults_to_json_to_csv(self) -> None:
        defs = self.dir_path / "game" / "defs"
        defs.mkdir(parents=True)
        save_entity_json("level01", [{"type": "player", "x": 1.5, "y": 1.5}], defs / "level01.entities.json")
        (defs / "level01.entities.csv").write_text("# stale\nplayer,9.5,9.5\n", encoding="utf-8")

        synced = sync_entity_definitions(self.dir_path)
        self.assertEqual(synced, ["Synced level01.entities.json -> level01.entities.csv"])
        self.assertEqual(load_entity_csv(defs / "level01.entities.csv"), [{"type": "player", "x": 1.5, "y": 1.5}])

        synced = sync_entity_definitions(self.dir_path, from_csv=True)
        self.assertEqual(synced, ["Synced level01.entities.csv -> level01.entities.json"])

    def test_sync_one_pair(self) -> None:
        # entities_path (sync-entities --level): only that level's pair, either direction.
        defs = self.dir_path / "game" / "defs"
        defs.mkdir(parents=True)
        for level_id in ("level01", "level02"):
            save_entity_json(level_id, [{"type": "player", "x": 1.5, "y": 1.5}], defs / f"{level_id}.entities.json")
            (defs / f"{level_id}.entities.csv").write_text("# stale\nplayer,9.5,9.5\n", encoding="utf-8")
        synced = sync_entity_definitions(self.dir_path, entities_path=defs / "level02.entities.json")
        self.assertEqual(synced, ["Synced level02.entities.json -> level02.entities.csv"])
        self.assertEqual(load_entity_csv(defs / "level02.entities.csv"), [{"type": "player", "x": 1.5, "y": 1.5}])
        self.assertIn("# stale", (defs / "level01.entities.csv").read_text(encoding="utf-8"))
        # The pair is found from either file of it.
        synced = sync_entity_definitions(self.dir_path, from_csv=True, entities_path=defs / "level01.entities.csv")
        self.assertEqual(synced, ["Synced level01.entities.csv -> level01.entities.json"])
        self.assertEqual(load_entity_json(defs / "level01.entities.json")[1], [{"type": "player", "x": 9.5, "y": 9.5}])
        self.assertEqual(load_entity_json(defs / "level02.entities.json")[1], [{"type": "player", "x": 1.5, "y": 1.5}])
        # A missing source file or a name that is not an entity file: ValueError, nothing written.
        with self.assertRaises(ValueError):
            sync_entity_definitions(self.dir_path, entities_path=defs / "level03.entities.json")
        with self.assertRaises(ValueError):
            sync_entity_definitions(self.dir_path, entities_path=defs / "level01.map")
        self.assertEqual(
            entity_file_pair(defs / "x.entities.csv"), (defs / "x.entities.json", defs / "x.entities.csv", "x")
        )

    def _defs_with_lights(self) -> Path:
        defs = self.dir_path / "game" / "defs"
        defs.mkdir(parents=True)
        self.lights = [{"x": 1, "y": 1, "w": 3, "h": 2, "level": 0.35}, {"x": 2, "y": 1, "w": 1, "h": 1, "level": 0.2}]
        save_entity_json(
            "level05", [{"type": "player", "x": 1.5, "y": 1.5}], defs / "level05.entities.json", self.lights
        )
        return defs

    def test_json_to_csv_warns_when_lights_are_dropped(self) -> None:
        # Known issue C21: the CSV cannot hold light zones; the export says so, in the result
        # and in a comment line the loaders skip.
        defs = self._defs_with_lights()
        synced = sync_entity_definitions(self.dir_path)
        self.assertEqual(synced[0], "Synced level05.entities.json -> level05.entities.csv")
        self.assertTrue(synced[1].startswith("Warning: level05.entities.json has 2 light zone(s)"), synced)
        csv_text = (defs / "level05.entities.csv").read_text(encoding="utf-8")
        self.assertIn("# lights: 2 light zone(s) in the JSON are not exported", csv_text)
        self.assertEqual(load_entity_csv(defs / "level05.entities.csv"), [{"type": "player", "x": 1.5, "y": 1.5}])
        self.assertEqual(
            json.loads((defs / "level05.entities.json").read_text(encoding="utf-8"))["lights"], self.lights
        )

    def test_csv_to_json_keeps_the_lights(self) -> None:
        defs = self._defs_with_lights()
        (defs / "level05.entities.csv").write_text("player,2.5,2.5\nenemy,3.5,1.5\n", encoding="utf-8")
        synced = sync_entity_definitions(self.dir_path, from_csv=True)
        self.assertEqual(
            synced,
            ["Synced level05.entities.csv -> level05.entities.json (kept its 2 light zone(s); the CSV has none)"],
        )
        document = json.loads((defs / "level05.entities.json").read_text(encoding="utf-8"))
        self.assertEqual(document["lights"], self.lights)
        self.assertEqual([e["type"] for e in document["entities"]], ["player", "enemy"])
        # Without lights the JSON gets no "lights" key, as before.
        save_entity_json("level05", [{"type": "player", "x": 1.5, "y": 1.5}], defs / "level05.entities.json")
        sync_entity_definitions(self.dir_path, from_csv=True)
        self.assertNotIn("lights", json.loads((defs / "level05.entities.json").read_text(encoding="utf-8")))

    def test_csv_to_json_refuses_an_unreadable_json(self) -> None:
        # It could not keep that file's light zones: nothing is written, not even other levels.
        defs = self._defs_with_lights()
        (defs / "level05.entities.csv").write_text("player,2.5,2.5\n", encoding="utf-8")
        (defs / "level06.entities.csv").write_text("player,2.5,2.5\n", encoding="utf-8")
        (defs / "level06.entities.json").write_text("{ not json", encoding="utf-8")
        before = (defs / "level05.entities.json").read_text(encoding="utf-8")
        with self.assertRaises(ValueError) as caught:
            sync_entity_definitions(self.dir_path, from_csv=True)
        self.assertIn("level06.entities.json cannot be read", str(caught.exception))
        self.assertIn("light zones", str(caught.exception))
        self.assertEqual((defs / "level05.entities.json").read_text(encoding="utf-8"), before)
        self.assertEqual((defs / "level06.entities.json").read_text(encoding="utf-8"), "{ not json")


if __name__ == "__main__":
    unittest.main()
