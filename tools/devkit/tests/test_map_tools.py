"""Unit tests for DevKit map tools (analysis, stats, generation, preview)."""

import tempfile
import unittest
from pathlib import Path

from tools.devkit.map_tools import (
    analyze_reachability,
    create_template_map,
    get_map_stats,
    load_map,
    render_ascii_preview,
    save_map,
)


class TestMapTools(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.dir_path = Path(self.temp_dir.name)

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def test_map_load_and_stats(self) -> None:
        map_content = (
            "######\n"
            "#....#\n"
            "#.A..#\n"
            "#..D.#\n"
            "#...T#\n"
            "######\n"
        )
        map_file = self.dir_path / "test.map"
        map_file.write_text(map_content, encoding="utf-8")

        m = load_map(map_file)
        self.assertEqual(m.width, 6)
        self.assertEqual(m.height, 6)
        self.assertEqual(m.get_tile(2, 2), "A")
        self.assertEqual(m.get_tile(3, 3), "D")
        self.assertEqual(m.get_tile(4, 4), "T")
        self.assertTrue(m.is_solid(0, 0))
        self.assertTrue(m.is_walkable(1, 1))

        stats = get_map_stats(map_file)
        self.assertEqual(stats["width"], 6)
        self.assertEqual(stats["height"], 6)
        self.assertEqual(stats["doors"], 1)
        self.assertEqual(stats["triggers"], 1)
        self.assertEqual(stats["walls_alt_a"], 1)

    def test_reachability_fully_connected(self) -> None:
        map_content = (
            "######\n"
            "#....#\n"
            "#....#\n"
            "#..D.#\n"
            "#....#\n"
            "######\n"
        )
        map_file = self.dir_path / "connected.map"
        map_file.write_text(map_content, encoding="utf-8")

        res = analyze_reachability(map_file, spawn_x=1.5, spawn_y=1.5)
        self.assertTrue(res["all_reachable"])  # without entities every 'D' counts as a door
        self.assertEqual(res["unreachable_count"], 0)
        self.assertFalse(res["success"])  # but there is no exit switch: the level cannot be completed
        self.assertEqual(res["exits"], 0)

    def test_reachability_isolated_region(self) -> None:
        map_content = (
            "########\n"
            "#..#...#\n"  # Column 3 is a solid wall dividing left and right
            "#..#...#\n"
            "#..#...#\n"
            "########\n"
        )
        map_file = self.dir_path / "partitioned.map"
        map_file.write_text(map_content, encoding="utf-8")

        res = analyze_reachability(map_file, spawn_x=1.5, spawn_y=1.5)
        self.assertFalse(res["success"])
        self.assertFalse(res["all_reachable"])
        self.assertGreater(res["unreachable_count"], 0)

    def test_create_template_map(self) -> None:
        out_map = self.dir_path / "template.map"
        m = create_template_map(out_map, width=16, height=12, style="arena")
        self.assertEqual(m.width, 16)
        self.assertEqual(m.height, 12)
        self.assertTrue(out_map.is_file())
        # Phase 5: an exit switch in the east wall, next to a walkable cell.
        self.assertEqual(m.get_tile(15, 6), "X")
        self.assertTrue(m.is_solid(15, 6))
        self.assertTrue(m.is_walkable(14, 6))
        self.assertEqual(get_map_stats(out_map)["exits"], 1)

    def test_ascii_preview(self) -> None:
        map_content = (
            "######\n"
            "#....#\n"
            "#....#\n"
            "######\n"
        )
        map_file = self.dir_path / "preview.map"
        map_file.write_text(map_content, encoding="utf-8")

        preview = render_ascii_preview(
            map_file,
            entity_placements=[{"type": "player", "x": 1.5, "y": 1.5}],
            color=False,
        )
        self.assertIn("=== Map Preview", preview)
        self.assertIn("P", preview)


if __name__ == "__main__":
    unittest.main()
