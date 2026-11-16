"""Validator campaign rules (Roadmap Phase 7) on multi-level fixture campaigns: a good campaign
passes `validate --strict`; each test breaks one rule and expects its error (or warning)."""

import copy
import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from typing import List

from fixture_builder import GOOD_LEVELS, write_campaign, write_content_root, write_entities
from tools.devkit import cli
from tools.devkit.validator import validate_gameplay, validate_project

REPO_ROOT = Path(__file__).resolve().parents[3]


def contains(messages: List[str], *parts: str) -> bool:
    return any(all(part in message for part in parts) for message in messages)


class CampaignRuleTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = write_content_root(Path(self.temp.name) / "content")

    def tearDown(self) -> None:
        self.temp.cleanup()

    def set_level(self, level_id: str, grid: str = None, entities: List[dict] = None) -> None:
        if grid is not None:
            (self.root / "game" / "maps" / f"{level_id}.map").write_text(grid, encoding="utf-8")
        if entities is not None:
            write_entities(self.root / "game" / "defs" / f"{level_id}.entities.json", level_id, entities)

    def entities(self, level_id: str) -> List[dict]:
        return copy.deepcopy(GOOD_LEVELS[level_id][1])

    def validate(self, require_levels: int = None):
        return validate_project(self.root, require_levels=require_levels)

    def strict_cli(self, *extra: str) -> int:
        with redirect_stdout(io.StringIO()):
            return cli.main(["--no-color", "--root", str(self.root), "validate", "--strict", *extra])


class TestGoodCampaign(CampaignRuleTest):
    def test_three_level_campaign_passes_strict(self) -> None:
        report = self.validate()
        self.assertEqual(report.errors, [])
        self.assertEqual(report.warnings, [])
        self.assertIn("Campaign: 3 level(s)", report.info)
        self.assertTrue(contains(report.info, "(beta)", "keys in order: blue, red"))
        self.assertEqual(self.strict_cli(), 0)

    def test_level_count_defaults_to_the_manifest_and_can_be_required(self) -> None:
        self.assertTrue(self.validate(require_levels=3).is_valid)
        report = self.validate(require_levels=10)
        self.assertTrue(contains(report.errors, "has 3 level(s); 10 are required"))
        self.assertEqual(self.strict_cli("--require-levels", "10"), 1)
        self.assertEqual(self.strict_cli("--require-levels", "3"), 0)


class TestCampaignManifest(CampaignRuleTest):
    def write_manifest(self, document) -> None:
        text = document if isinstance(document, str) else json.dumps(document)
        (self.root / "game" / "defs" / "campaign.json").write_text(text, encoding="utf-8")

    def test_duplicate_and_malformed_ids(self) -> None:
        write_campaign(self.root, ["alpha", "beta", "alpha"])
        self.assertTrue(contains(self.validate().errors, "levels[2] (alpha)", "id is used by an earlier level"))
        write_campaign(self.root, ["alpha"], extra={"alpha": {"id": "Alpha One"}})
        self.assertTrue(contains(self.validate().errors, "id must use only a-z"))

    def test_missing_files_and_theme(self) -> None:
        write_campaign(self.root, ["alpha", "beta", "gamma"], extra={"beta": {"map": "game/maps/nope.map"},
                                                                     "gamma": {"theme": "missing"}})
        errors = self.validate().errors
        self.assertTrue(contains(errors, "(beta)", "map file not found: game/maps/nope.map"))
        self.assertTrue(contains(errors, "(gamma)", "theme 'missing' not found"))

    def test_manifest_shape(self) -> None:
        self.write_manifest({"levels": []})
        errors = self.validate().errors
        self.assertTrue(contains(errors, "'title' must be a non-empty string"))
        self.assertTrue(contains(errors, "'levels' must be a non-empty array"))
        self.write_manifest('{ "title": "T", "title": "U", "levels": [] }')
        self.assertTrue(contains(self.validate().errors, 'duplicate key "title"'))
        self.write_manifest("{ not json")
        self.assertTrue(contains(self.validate().errors, "invalid JSON"))

    def test_unknown_keys_and_long_names_are_warnings(self) -> None:
        write_campaign(self.root, ["alpha", "beta", "gamma"],
                       extra={"alpha": {"music": "x.ogg"}, "beta": {"name": "A level name that is far too long to fit"}})
        report = self.validate()
        self.assertTrue(report.is_valid, report.errors)
        self.assertTrue(contains(report.warnings, '(alpha): unknown key "music" ignored'))
        self.assertTrue(contains(report.warnings, "(beta): name is longer than 32 characters"))
        self.assertEqual(self.strict_cli(), 1)

    def test_byte_order_mark_and_crlf_are_accepted(self) -> None:
        path = self.root / "game" / "defs" / "campaign.json"
        path.write_bytes(b"\xef\xbb\xbf" + path.read_bytes().replace(b"\n", b"\r\n"))
        self.assertEqual(self.validate().errors, [])

    def test_orphan_files_are_warnings_and_fail_strict(self) -> None:
        (self.root / "game" / "maps" / "unused.map").write_text("#####\n#..X#\n#####\n", encoding="utf-8")
        write_entities(self.root / "game" / "defs" / "unused.entities.json", "unused",
                       [{"type": "player", "x": 1.5, "y": 1.5}])
        (self.root / "game" / "defs" / "stale.entities.csv").write_text("player,1.5,1.5\n", encoding="utf-8")
        (self.root / "game" / "defs" / "alpha.entities.csv").write_text("player,1.5,1.5\n", encoding="utf-8")
        report = self.validate()
        self.assertTrue(report.is_valid, report.errors)
        self.assertTrue(contains(report.warnings, "orphan map: game/maps/unused.map"))
        self.assertTrue(contains(report.warnings, "orphan entity file: game/defs/unused.entities.json"))
        self.assertTrue(contains(report.warnings, "orphan entity file: game/defs/stale.entities.csv"))
        self.assertFalse(contains(report.warnings, "alpha.entities.csv"))  # the export of a used JSON
        self.assertEqual(self.strict_cli(), 1)


class TestPlayersAndPlacements(CampaignRuleTest):
    def test_exactly_one_player(self) -> None:
        self.set_level("alpha", entities=[e for e in self.entities("alpha") if e["type"] != "player"])
        self.assertTrue(contains(self.validate().errors, "(alpha)", "0 player entities"))
        self.set_level("alpha", entities=self.entities("alpha") + [{"type": "player", "x": 2.5, "y": 1.5}])
        self.assertTrue(contains(self.validate().errors, "(alpha)", "2 player entities"))

    def test_player_must_stand_on_a_floor_cell_clear_of_walls(self) -> None:
        ents = self.entities("alpha")
        ents[0].update(x=0.5, y=0.5)
        self.set_level("alpha", entities=ents)
        self.assertTrue(contains(self.validate().errors, "(alpha): player 0", "inside a solid cell '#'"))
        ents[0].update(x=1.1, y=1.5)  # in a floor cell, but the body (0.22) reaches into the wall
        self.set_level("alpha", entities=ents)
        self.assertTrue(contains(self.validate().errors, "the player's body", "cannot move"))

    def test_entities_inside_solid_cells_or_doors(self) -> None:
        ents = self.entities("beta") + [
            {"type": "pickup", "x": 3.5, "y": 0.5, "kind": "health"},
            {"type": "enemy", "x": 3.5, "y": 1.5},
            {"type": "decoration", "x": 9.5, "y": 1.5},
        ]
        self.set_level("beta", entities=ents)
        errors = self.validate().errors
        self.assertTrue(contains(errors, "(beta): pickup 5", "inside a solid cell '#'"))
        self.assertTrue(contains(errors, "(beta): enemy 6", "inside a door cell"))
        self.assertTrue(contains(errors, "(beta): decoration 7", "inside a solid cell 'X'"))

    def test_enemy_wedged_against_a_wall_is_a_warning(self) -> None:
        ents = self.entities("alpha")
        ents[3].update(y=1.15)
        self.set_level("alpha", entities=ents)
        report = self.validate()
        self.assertTrue(report.is_valid, report.errors)
        self.assertTrue(contains(report.warnings, "(alpha): enemy 3", "starts wedged"))

    def test_doors_sit_on_d_cells_one_each(self) -> None:
        ents = self.entities("beta")
        ents[1].update(mapX=2, x=2.5)  # the blue door moved onto a floor cell
        self.set_level("beta", entities=ents)
        errors = self.validate().errors
        self.assertTrue(contains(errors, "(beta): door 1", "is '.' on the map, not 'D'"))
        self.assertTrue(contains(errors, "(beta): 'D' at (3, 1) has no door entity"))

        ents = self.entities("beta") + [{"type": "door", "x": 6.5, "y": 1.5, "mapX": 6, "mapY": 1}]
        self.set_level("beta", entities=ents)
        self.assertTrue(contains(self.validate().errors, "'D' at (6, 1) has 2 door entities"))

        ents = self.entities("beta")
        ents[2].update(x=5.5)  # the red door's position is in the cell before it
        self.set_level("beta", entities=ents)
        self.assertTrue(contains(self.validate().errors, "(beta): door 2", "not in its own cell (6, 1)"))

    def test_triggers_sit_on_t_cells(self) -> None:
        ents = self.entities("alpha")
        ents[1].update(mapX=2, x=2.5)
        self.set_level("alpha", entities=ents)
        report = self.validate()
        self.assertTrue(contains(report.errors, "(alpha): trigger 1", "not 'T'"))
        self.assertTrue(contains(report.warnings, "'T' at (3, 2) has no trigger entity"))


class TestProgress(CampaignRuleTest):
    def test_exit_switch_required_and_reachable(self) -> None:
        self.set_level("alpha", grid="#######\n#.....#\n#..T..#\n#######\n")
        self.assertTrue(contains(self.validate().errors, "(alpha): the map has no exit switch"))
        self.set_level("alpha", grid="#######\n#..#..#\n#..T#.X\n#######\n")
        ents = self.entities("alpha")
        ents[3].update(x=2.5)  # keep the guard reachable
        self.set_level("alpha", entities=ents)
        errors = self.validate().errors
        self.assertTrue(contains(errors, "(alpha): no exit switch 'X' can be reached from the player spawn"))

    def test_key_before_lock(self) -> None:
        ents = self.entities("beta")
        ents[4].update(x=8.5)  # the red key behind its own door
        self.set_level("beta", entities=ents)
        errors = self.validate().errors
        self.assertTrue(contains(errors, "(beta): the red key cannot be reached from the spawn before the door it opens"))
        self.assertTrue(contains(errors, "with the keys that can be collected first"))
        self.set_level("beta", entities=self.entities("beta")[:4])  # no red key at all
        self.assertTrue(contains(self.validate().errors, "needs the red key, but the level has no red key"))

    def test_unreachable_pickups_and_enemies_are_warnings(self) -> None:
        grid = "########\n#......#\n#.####.#\n#......X\n#.#.####\n#.#.#..#\n########\n"
        ents = self.entities("gamma") + [
            {"type": "pickup", "x": 5.5, "y": 5.5, "kind": "health"},
            {"type": "enemy", "x": 6.5, "y": 5.5, "variant": "guard"},
        ]
        self.set_level("gamma", grid=grid, entities=ents)
        report = self.validate()
        self.assertTrue(report.is_valid, report.errors)
        self.assertTrue(contains(report.warnings, "(gamma): the health pickup (entity 5)", "cannot be reached"))
        self.assertTrue(contains(report.warnings, "(gamma): the guard enemy (entity 6)", "cannot be reached"))

    def test_committed_unwinnable_fixture(self) -> None:
        # The same level makes the autoplay bot fail (CTest autoplay.fixture_unwinnable): the
        # validator must report exactly that problem and nothing else.
        report = validate_project(REPO_ROOT / "tests" / "fixtures" / "unwinnable")
        self.assertEqual(len(report.errors), 1, report.errors)
        self.assertTrue(contains(report.errors, "(unwinnable): no exit switch 'X' can be reached"))
        self.assertEqual(report.warnings, [])


class TestGameplaySchema(CampaignRuleTest):
    def check(self, text: str):
        path = self.root / "game" / "defs" / "gameplay.json"
        path.write_text(text, encoding="utf-8")
        return validate_gameplay(path)

    def test_shipped_gameplay_is_valid(self) -> None:
        report, values = validate_gameplay(REPO_ROOT / "game" / "defs" / "gameplay.json")
        self.assertEqual((report.errors, report.warnings), ([], []))
        self.assertEqual(values["weapon"]["damage"], 35)
        self.assertEqual(values["enemies"]["brute"]["health"], 140)

    def test_types_ranges_and_cross_fields(self) -> None:
        report, _ = self.check('{ "weapon": { "damage": "35", "fireRate": -1, "maxAmmo": 3.0 }, '
                               '"player": { "collisionRadius": 0.5, "turnSpeed": 0 } }')
        for part in ('weapon.damage must be an integer from 1 to 10000, got "35"',
                     "weapon.fireRate must be a number >= 0 and <= 60, got -1",
                     "weapon.maxAmmo must be an integer from 0 to 10000, got 3.0",
                     "player.collisionRadius must be a number > 0 and < 0.5",
                     "player.turnSpeed must be a number > 0 and <= 20"):
            self.assertTrue(contains(report.errors, part), (part, report.errors))
        report, _ = self.check('{ "player": { "initialHealth": 120, "maxHealth": 100 } }')
        self.assertTrue(contains(report.errors, "player.initialHealth (120) must not exceed player.maxHealth (100)"))
        report, _ = self.check('{ "weapon": { "damage": NaN } }')
        self.assertTrue(contains(report.errors, "invalid JSON"))

    def test_sections_duplicates_and_unknown_keys(self) -> None:
        report, _ = self.check('{ "weapon": { "damage": 5, "damage": 6 } }')
        self.assertTrue(contains(report.errors, 'duplicate key "damage"'))
        report, _ = self.check('{ "player": [] }')
        self.assertTrue(contains(report.errors, "player must be an object, got an array"))
        report, _ = self.check('{ "enemies": { "guard": { "health": 0 }, "dragon": {} }, "extra": 1 }')
        self.assertTrue(contains(report.errors, "enemies.guard.health must be an integer from 1 to 10000, got 0"))
        self.assertTrue(contains(report.warnings, "unknown key enemies.dragon ignored"))
        self.assertTrue(contains(report.warnings, "unknown key extra ignored"))
        report, _ = self.check('{ "pickups": { "health": { "value": 10, "maxPlayerHealth": 200 } } }')
        self.assertTrue(contains(report.warnings, "pickups.health.maxPlayerHealth ignored (moved to player.maxHealth)"))

    def test_enemies_section_and_the_old_enemy_section(self) -> None:
        report, values = self.check('{ "enemy": { "health": 70 } }')
        self.assertTrue(report.is_valid, report.errors)
        self.assertTrue(contains(report.warnings, 'section "enemy" is deprecated'))
        self.assertEqual(values["enemies"]["guard"]["health"], 70)
        report, _ = self.check('{ "enemy": {}, "enemies": {} }')
        self.assertTrue(contains(report.errors, 'use either "enemies" or the old "enemy" section'))

    def test_project_reports_gameplay_problems(self) -> None:
        self.check('{ "weapon": { "range": 0 } }')
        self.assertTrue(contains(self.validate().errors, "weapon.range must be a number > 0"))
        (self.root / "game" / "defs" / "gameplay.json").unlink()
        self.assertTrue(contains(self.validate().errors, "Missing gameplay definitions"))


class TestThemesAndConfigFiles(CampaignRuleTest):
    def test_theme_textures_must_exist(self) -> None:
        theme = self.root / "game" / "defs" / "themes" / "fx.cfg"
        theme.write_text(theme.read_text(encoding="utf-8").replace("exit=game/assets/fx.ppm", "exit=game/assets/gone.ppm"),
                         encoding="utf-8")
        self.assertTrue(contains(self.validate().errors, "themes/fx.cfg references non-existent file for 'exit'"))

    def test_theme_syntax_and_keys_like_the_engine(self) -> None:
        theme = self.root / "game" / "defs" / "themes" / "fx.cfg"
        text = theme.read_text(encoding="utf-8")
        theme.write_text(text.replace("floor=game/assets/fx.ppm\n", "floor=\nceiling game/assets/fx.ppm\n"), encoding="utf-8")
        errors = self.validate().errors
        self.assertTrue(contains(errors, "Empty key or value", "the engine ignores every entry"))
        self.assertTrue(contains(errors, "Missing '=' delimiter"))
        theme.write_text(text.replace("door_gold=game/assets/fx.ppm\n", ""), encoding="utf-8")
        self.assertTrue(contains(self.validate().errors, "Missing required key 'door_gold'"))
        theme.write_text(text.replace("\n", "\r\n") + "   # comment\r\n\r\n", encoding="utf-8")
        self.assertEqual(self.validate().errors, [])


if __name__ == "__main__":
    unittest.main()
