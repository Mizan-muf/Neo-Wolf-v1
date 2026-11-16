"""DevKit command line (Roadmap Phase 7): create-map --level-id, inspect-map (known issue D3),
doctor (D4), root resolution, the launchers and terminal colours."""

import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from typing import List, Tuple

from fixture_builder import GOOD_LEVELS, write_content_root, write_entities
from tools.devkit import cli
from tools.devkit.doctor import render_doctor_report, run_doctor
from tools.devkit.term import supports_color
from tools.devkit.validator import validate_project

REPO_ROOT = Path(__file__).resolve().parents[3]
DEVKIT_DIR = REPO_ROOT / "tools" / "devkit"


def run_cli(*args: str) -> Tuple[int, str]:
    out = io.StringIO()
    with redirect_stdout(out):
        code = cli.main(["--no-color", *args])
    return code, out.getvalue()


class CliTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = write_content_root(Path(self.temp.name) / "content")

    def tearDown(self) -> None:
        self.temp.cleanup()


class TestCreateMap(CliTest):
    def test_level_stub_registered_and_valid(self) -> None:
        code, out = run_cli("--root", str(self.root), "create-map", "--level-id", "delta", "--style", "dungeon",
                            "--register", "--name", "Delta Depot")
        self.assertEqual(code, 0, out)
        map_path = self.root / "game" / "maps" / "delta.map"
        entities_path = self.root / "game" / "defs" / "delta.entities.json"
        self.assertTrue(map_path.is_file() and entities_path.is_file())
        stub = json.loads(entities_path.read_text(encoding="utf-8"))
        self.assertEqual(stub["level"], "delta")
        self.assertEqual([e["type"] for e in stub["entities"]].count("player"), 1)
        self.assertIn("door", [e["type"] for e in stub["entities"]])  # the dungeon template has a 'D'
        campaign = json.loads((self.root / "game" / "defs" / "campaign.json").read_text(encoding="utf-8"))
        self.assertEqual(campaign["levels"][-1], {"id": "delta", "name": "Delta Depot", "map": "game/maps/delta.map",
                                                  "entities": "game/defs/delta.entities.json", "theme": "fx"})
        report = validate_project(self.root, require_levels=4)
        self.assertEqual((report.errors, report.warnings), ([], []))

    def test_every_template_style_makes_a_valid_level(self) -> None:
        for style in ("box", "arena", "dungeon", "crossroads"):
            with self.subTest(style=style):
                level_id = f"t_{style}"
                code, out = run_cli("--root", str(self.root), "create-map", "--level-id", level_id, "--style", style,
                                    "--width", "14", "--height", "11", "--register")
                self.assertEqual(code, 0, out)
        report = validate_project(self.root, require_levels=7)
        self.assertEqual((report.errors, report.warnings), ([], []))

    def test_refuses_to_overwrite_without_force(self) -> None:
        self.assertEqual(run_cli("--root", str(self.root), "create-map", "--level-id", "delta")[0], 0)
        map_path = self.root / "game" / "maps" / "delta.map"
        map_path.write_text("#####\n#..X#\n#####\n", encoding="utf-8")
        code, out = run_cli("--root", str(self.root), "create-map", "--level-id", "delta")
        self.assertEqual(code, 1)
        self.assertIn("already exists; not overwritten", out)
        self.assertEqual(map_path.read_text(encoding="utf-8"), "#####\n#..X#\n#####\n")
        self.assertEqual(run_cli("--root", str(self.root), "create-map", "--level-id", "delta", "--force")[0], 0)
        self.assertNotEqual(map_path.read_text(encoding="utf-8"), "#####\n#..X#\n#####\n")
        # A plain map path is protected the same way.
        plain = Path(self.temp.name) / "plain.map"
        self.assertEqual(run_cli("create-map", str(plain), "--style", "box")[0], 0)
        self.assertEqual(run_cli("create-map", str(plain), "--style", "box")[0], 1)

    def test_register_errors(self) -> None:
        self.assertEqual(run_cli("--root", str(self.root), "create-map", "--register")[0], 2)
        self.assertEqual(run_cli("--root", str(self.root), "create-map", "--level-id", "Bad Id")[0], 2)
        code, out = run_cli("--root", str(self.root), "create-map", "--level-id", "alpha2", "--register",
                            "--theme", "nope")
        self.assertEqual(code, 1)
        self.assertIn("theme 'nope' not found", out)
        self.assertFalse((self.root / "game" / "maps" / "alpha2.map").exists())  # nothing written
        # An id the campaign already has (its files gone): refused unless --force.
        (self.root / "game" / "maps" / "beta.map").unlink()
        (self.root / "game" / "defs" / "beta.entities.json").unlink()
        code, out = run_cli("--root", str(self.root), "create-map", "--level-id", "beta", "--register")
        self.assertEqual(code, 1)
        self.assertIn("already has a level 'beta'", out)
        self.assertEqual(run_cli("--root", str(self.root), "create-map", "--level-id", "beta", "--register", "--force")[0], 0)
        campaign = json.loads((self.root / "game" / "defs" / "campaign.json").read_text(encoding="utf-8"))
        self.assertEqual([lvl["id"] for lvl in campaign["levels"]], ["alpha", "beta", "gamma"])

    def test_register_reports_the_campaign_position(self) -> None:
        # --force on an existing id names that entry's own position, not the last one's.
        code, out = run_cli("--root", str(self.root), "create-map", "--level-id", "alpha", "--register", "--force")
        self.assertEqual(code, 0, out)
        self.assertIn("Updated level alpha as level 1 ", out)
        code, out = run_cli("--root", str(self.root), "create-map", "--level-id", "beta", "--register", "--force")
        self.assertEqual(code, 0, out)
        self.assertIn("Updated level beta as level 2 ", out)
        code, out = run_cli("--root", str(self.root), "create-map", "--level-id", "delta", "--register")
        self.assertEqual(code, 0, out)
        self.assertIn("Registered level delta as level 4 ", out)
        campaign = json.loads((self.root / "game" / "defs" / "campaign.json").read_text(encoding="utf-8"))
        self.assertEqual([lvl["id"] for lvl in campaign["levels"]], ["alpha", "beta", "gamma", "delta"])


class TestInspectMap(CliTest):
    def test_every_shipped_campaign_level(self) -> None:
        # Data-driven: every level of the shipped campaign.json, by number and by id.
        campaign = json.loads((REPO_ROOT / "game" / "defs" / "campaign.json").read_text(encoding="utf-8"))
        levels = campaign["levels"]
        self.assertGreater(len(levels), 0)
        for number, level in enumerate(levels, start=1):
            for selector in (str(number), level["id"]):
                with self.subTest(level=selector):
                    code, out = run_cli("inspect-map", "--level", selector)
                    self.assertEqual(code, 0, out)
                    self.assertIn("Status: OK", out)
        # A map path alone: the entity file is found through the campaign.
        code, out = run_cli("inspect-map", str(REPO_ROOT / levels[-1]["map"]))
        self.assertEqual(code, 0, out)
        self.assertIn("Status: OK", out)

    def test_key_order_through_the_campaign(self) -> None:
        # The fixture's "beta" level: a blue door, then a red one; found through its campaign.
        code, out = run_cli("--root", str(self.root), "inspect-map", str(self.root / "game" / "maps" / "beta.map"))
        self.assertEqual(code, 0, out)
        self.assertIn("Keys, in the order they become reachable: blue, red", out)

    def test_fixture_levels_and_unknown_level(self) -> None:
        code, out = run_cli("--root", str(self.root), "inspect-map", "--level", "beta")
        self.assertEqual(code, 0, out)
        self.assertEqual(run_cli("--root", str(self.root), "inspect-map", "--level", "99")[0], 1)

    def test_light_zones_are_listed(self) -> None:
        path = self.root / "game" / "defs" / "alpha.entities.json"
        document = json.loads(path.read_text(encoding="utf-8"))
        document["lights"] = [{"x": 1, "y": 1, "w": 2, "h": 1, "level": 0.35}]
        path.write_text(json.dumps(document), encoding="utf-8")
        code, out = run_cli("--root", str(self.root), "inspect-map", "--level", "alpha")
        self.assertEqual(code, 0, out)
        self.assertIn("1 light zone(s)", out)
        self.assertIn("lights[0]: cells (1, 1) to (2, 1), 2x1, level 0.35", out)
        document["lights"].append({"x": 0, "y": 0, "w": 999, "h": 1, "level": 0.5})
        path.write_text(json.dumps(document), encoding="utf-8")
        code, out = run_cli("--root", str(self.root), "inspect-map", "--level", "alpha")
        self.assertEqual(code, 1, out)
        self.assertIn("lights[1]: cells (0, 0) to (998, 0) are not all inside", out)

    def test_spawn_inside_a_wall_is_reported_not_a_crash(self) -> None:
        ents = list(GOOD_LEVELS["alpha"][1])
        ents[0] = {"type": "player", "x": 0.5, "y": 0.5}
        write_entities(self.root / "game" / "defs" / "alpha.entities.json", "alpha", ents)
        code, out = run_cli("--root", str(self.root), "inspect-map", "--level", "alpha")
        self.assertEqual(code, 1)
        self.assertIn("Status: FAILED: Spawn (0.50, 0.50) is in cell (0, 0), inside the solid tile '#'", out)
        code, out = run_cli("--root", str(self.root), "inspect-map", "--level", "alpha", "--spawn-x", "1.5",
                            "--spawn-y", "1.5")
        self.assertEqual(code, 0, out)  # an explicit spawn overrides the entity

    def test_unreachable_exit_fails(self) -> None:
        (self.root / "game" / "maps" / "alpha.map").write_text("#######\n#..#..#\n#..T#.X\n#######\n", encoding="utf-8")
        code, out = run_cli("--root", str(self.root), "inspect-map", "--level", "alpha")
        self.assertEqual(code, 1)
        self.assertIn("Exit switches: 0 of 1 usable", out)
        self.assertIn("no exit switch can be reached", out)

    def test_without_entities_and_usage_errors(self) -> None:
        map_path = self.root / "game" / "maps" / "beta.map"
        code, out = run_cli("--root", str(self.root), "inspect-map", str(map_path), "--no-entities")
        self.assertEqual(code, 1)
        self.assertIn("no spawn", out)
        code, out = run_cli("--root", str(self.root), "inspect-map", str(map_path), "--no-entities", "--spawn-x",
                            "1.5", "--spawn-y", "1.5")
        self.assertEqual(code, 0, out)  # every 'D' counts as an openable door
        self.assertEqual(run_cli("--root", str(self.root), "inspect-map", str(map_path), "--spawn-x", "1.5")[0], 2)
        self.assertEqual(run_cli("--root", str(self.root), "inspect-map")[0], 2)
        self.assertEqual(run_cli("inspect-map", str(self.root / "nope.map"))[0], 1)
        bad = self.root / "bad.map"
        bad.write_text("#####\n#..X#  \n#####\n", encoding="utf-8")
        code, out = run_cli("inspect-map", str(bad), "--no-entities", "--spawn-x", "1.5", "--spawn-y", "1.5")
        self.assertEqual(code, 1)
        self.assertIn("the engine cannot load", out)


class TestSyncEntities(CliTest):
    """sync-entities, and --level (one campaign level by id or 1-based number)."""

    def setUp(self) -> None:
        super().setUp()
        self.defs = self.root / "game" / "defs"
        # Stale CSVs for every level: only the synced ones change.
        for level_id in ("alpha", "beta", "gamma"):
            (self.defs / f"{level_id}.entities.csv").write_text("# stale\nplayer,9.5,9.5\n", encoding="utf-8")

    def csv_text(self, level_id: str) -> str:
        return (self.defs / f"{level_id}.entities.csv").read_text(encoding="utf-8")

    def test_without_level_syncs_every_pair(self) -> None:
        code, out = run_cli("--root", str(self.root), "sync-entities")
        self.assertEqual(code, 0, out)
        self.assertIn("(3 file pairs)", out)
        for level_id in ("alpha", "beta", "gamma"):
            self.assertNotIn("# stale", self.csv_text(level_id))

    def test_level_by_id_and_by_number(self) -> None:
        code, out = run_cli("--root", str(self.root), "sync-entities", "--level", "beta")
        self.assertEqual(code, 0, out)
        self.assertIn("Synced beta.entities.json -> beta.entities.csv", out)
        self.assertIn("(1 file pairs)", out)
        self.assertNotIn("# stale", self.csv_text("beta"))
        self.assertIn("# stale", self.csv_text("alpha"))
        self.assertIn("# stale", self.csv_text("gamma"))
        # Number 3 is gamma (1-based campaign order); alpha stays stale.
        code, out = run_cli("--root", str(self.root), "sync-entities", "--level", "3")
        self.assertEqual(code, 0, out)
        self.assertIn("Synced gamma.entities.json -> gamma.entities.csv", out)
        self.assertNotIn("alpha", out)
        self.assertNotIn("# stale", self.csv_text("gamma"))
        self.assertIn("# stale", self.csv_text("alpha"))

    def test_level_from_csv(self) -> None:
        (self.defs / "alpha.entities.csv").write_text("player,2.5,2.5\n", encoding="utf-8")
        beta_before = (self.defs / "beta.entities.json").read_text(encoding="utf-8")
        code, out = run_cli("--root", str(self.root), "sync-entities", "--level", "1", "--from-csv")
        self.assertEqual(code, 0, out)
        self.assertIn("Synced alpha.entities.csv -> alpha.entities.json", out)
        alpha = json.loads((self.defs / "alpha.entities.json").read_text(encoding="utf-8"))
        self.assertEqual(alpha["entities"], [{"type": "player", "x": 2.5, "y": 2.5}])
        self.assertEqual((self.defs / "beta.entities.json").read_text(encoding="utf-8"), beta_before)
        # A level without a CSV: an error, nothing written.
        (self.defs / "gamma.entities.csv").unlink()
        gamma_before = (self.defs / "gamma.entities.json").read_text(encoding="utf-8")
        code, out = run_cli("--root", str(self.root), "sync-entities", "--level", "gamma", "--from-csv")
        self.assertEqual(code, 1, out)
        self.assertIn("gamma.entities.csv not found; nothing was synced", out)
        self.assertEqual((self.defs / "gamma.entities.json").read_text(encoding="utf-8"), gamma_before)

    def test_level_keeps_the_light_zone_warning(self) -> None:
        path = self.defs / "beta.entities.json"
        document = json.loads(path.read_text(encoding="utf-8"))
        document["lights"] = [{"x": 1, "y": 1, "w": 2, "h": 1, "level": 0.35}]
        path.write_text(json.dumps(document), encoding="utf-8")
        code, out = run_cli("--root", str(self.root), "sync-entities", "--level", "beta")
        self.assertEqual(code, 0, out)
        self.assertIn("Warning: beta.entities.json has 1 light zone(s); the CSV cannot hold them", out)
        self.assertIn("# lights: 1 light zone(s) in the JSON are not exported", self.csv_text("beta"))

    def test_unknown_level_fails_without_writing(self) -> None:
        for wanted in ("delta", "0", "4", "99", "-1", ""):
            with self.subTest(level=wanted):
                code, out = run_cli("--root", str(self.root), "sync-entities", "--level", wanted)
                self.assertEqual(code, 1, out)
                self.assertIn(f"Error: campaign.json has no level '{wanted}'", out)
                self.assertIn("or a number from 1 to 3", out)
        for level_id in ("alpha", "beta", "gamma"):
            self.assertIn("# stale", self.csv_text(level_id))
        # A campaign level whose entity file is missing: an error, not a crash.
        (self.defs / "beta.entities.json").unlink()
        code, out = run_cli("--root", str(self.root), "sync-entities", "--level", "beta")
        self.assertEqual(code, 1, out)
        self.assertIn("not found", out)
        self.assertIn("beta.entities.json", out)
        self.assertIn("# stale", self.csv_text("beta"))

    def test_every_shipped_campaign_level_resolves(self) -> None:
        # Data-driven over the shipped campaign.json, on a scratch copy of game/defs (the real
        # CSVs are not touched): id and number name the same entity pair.
        campaign = json.loads((REPO_ROOT / "game" / "defs" / "campaign.json").read_text(encoding="utf-8"))
        scratch = Path(self.temp.name) / "shipped"
        shutil.copytree(REPO_ROOT / "game" / "defs", scratch / "game" / "defs")
        shutil.copytree(REPO_ROOT / "game" / "maps", scratch / "game" / "maps")
        for number, level in enumerate(campaign["levels"], start=1):
            stem = Path(level["entities"]).name.replace(".entities.json", "")
            for selector in (str(number), level["id"]):
                with self.subTest(level=selector):
                    code, out = run_cli("--root", str(scratch), "sync-entities", "--level", selector)
                    self.assertEqual(code, 0, out)
                    self.assertIn(f"Synced {stem}.entities.json -> {stem}.entities.csv", out)
                    self.assertIn("(1 file pairs)", out)


class FakeEnv:
    """A stand-in for which/run so doctor can be tested for every platform."""

    def __init__(self, tools: dict, outputs: dict) -> None:
        self.tools = tools
        self.outputs = outputs

    def which(self, name: str):
        return f"/fake/{name}" if name in self.tools else None

    def run(self, command: List[str]):
        key = Path(command[0]).name if command[0].endswith("vswhere.exe") else command[0]
        if key == "vswhere.exe" or key == "vswhere":
            return 0, self.outputs.get(f"vswhere {command[-1]}", "")
        return (0, self.outputs[key]) if key in self.outputs else (-1, "")


class TestDoctor(unittest.TestCase):
    def doctor(self, system: str, env: FakeEnv, root: Path = REPO_ROOT, **kwargs):
        return run_doctor(root, system=system, which=env.which, run=env.run, environ=kwargs.pop("environ", {}),
                          check_network=kwargs.pop("check_network", False), **kwargs)

    def test_macos_with_everything(self) -> None:
        env = FakeEnv({"cmake", "ninja", "clang++", "make"},
                      {"cmake": "cmake version 3.31.6\n", "ninja": "1.12.1\n", "clang++": "Apple clang version 17.0.0\n"})
        report = self.doctor("Darwin", env)
        self.assertEqual(report.find("CMake").status, "PASS")
        self.assertEqual(report.find("Ninja").status, "PASS")
        self.assertIn("Apple clang", report.find("C++ compiler").message)
        self.assertEqual(report.find("Content validation").status, "PASS")
        self.assertFalse(report.has_failures, [(i.name, i.message) for i in report.items if i.status == "FAIL"])
        self.assertIn("System Doctor", render_doctor_report(report, color=False))

    def test_linux_old_cmake_and_missing_tools(self) -> None:
        env = FakeEnv({"cmake"}, {"cmake": "cmake version 3.22.1\n"})
        report = self.doctor("Linux", env)
        cmake = report.find("CMake")
        self.assertEqual(cmake.status, "FAIL")
        self.assertIn("older than the required 3.25", cmake.message)
        self.assertIn("Ubuntu 22.04", cmake.remediation)
        self.assertEqual(report.find("Ninja").status, "FAIL")
        self.assertIn("apt install ninja-build", report.find("Ninja").remediation)
        self.assertEqual(report.find("C++ compiler").status, "FAIL")
        self.assertEqual(report.find("make").status, "INFO")  # optional: never a failure
        self.assertTrue(report.has_failures)

    def test_windows_visual_studio_through_vswhere(self) -> None:
        program_files = tempfile.TemporaryDirectory()
        self.addCleanup(program_files.cleanup)
        vswhere = Path(program_files.name) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        vswhere.parent.mkdir(parents=True)
        vswhere.write_bytes(b"")
        env = FakeEnv({"cmake"}, {"cmake": "cmake version 3.29.0\n",
                                  "vswhere displayName": "Visual Studio Community 2022\n",
                                  "vswhere catalog_productDisplayVersion": "17.10.3\n"})
        report = self.doctor("Windows", env, environ={"ProgramFiles(x86)": program_files.name})
        self.assertEqual(report.find("C++ compiler").status, "PASS")
        self.assertIn("Visual Studio Community 2022 17.10.3", report.find("C++ compiler").message)
        self.assertEqual(report.find("Ninja").status, "INFO")  # not needed with the VS generator
        self.assertFalse(report.has_failures)
        report = self.doctor("Windows", FakeEnv({"cmake"}, {"cmake": "cmake version 3.29.0\n"}),
                             environ={"ProgramFiles(x86)": str(Path(program_files.name) / "none")})
        self.assertEqual(report.find("C++ compiler").status, "FAIL")
        self.assertIn("Desktop development with C++", report.find("C++ compiler").remediation)

    def test_old_python_fails(self) -> None:
        report = self.doctor("Linux", FakeEnv(set(), {}), python_version=(3, 7, 9))
        self.assertEqual(report.find("Python").status, "FAIL")

    def test_sdl_source_cache_and_network(self) -> None:
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = write_content_root(Path(temp.name) / "content")
        tarball_bytes = b"not really a tarball"
        sha = hashlib.sha256(tarball_bytes).hexdigest()
        (root / "CMakeLists.txt").write_text(
            'set(NEOWOLF_SDL2_VERSION "2.32.10")\n'
            'set(NEOWOLF_SDL2_URL\n    "https://github.com/libsdl-org/SDL/releases/download/release-${NEOWOLF_SDL2_VERSION}/'
            'SDL2-${NEOWOLF_SDL2_VERSION}.tar.gz")\n'
            f'set(NEOWOLF_SDL2_SHA256 "{sha}")\n', encoding="utf-8")
        env = FakeEnv({"cmake", "ninja", "g++"}, {"cmake": "cmake version 3.28.3", "ninja": "1.11.1", "g++": "g++ 13"})
        sdl = lambda **kw: self.doctor("Linux", env, root=root, **kw).find("SDL2 source")  # noqa: E731
        self.assertEqual(sdl(check_network=False).status, "INFO")
        self.assertEqual(sdl(check_network=True, network=lambda: True).status, "PASS")
        offline = sdl(check_network=True, network=lambda: False)
        self.assertEqual(offline.status, "WARN")  # a warning, not a failure: the build may still be offline-ready
        self.assertIn("FETCHCONTENT_SOURCE_DIR_SDL2", offline.remediation)
        tarball = root / "build" / "linux" / "_deps" / "sdl2-subbuild" / "sdl2-populate-prefix" / "src" / "SDL2-2.32.10.tar.gz"
        tarball.parent.mkdir(parents=True)
        tarball.write_bytes(tarball_bytes)
        self.assertEqual(sdl(check_network=True, network=lambda: False).status, "PASS")
        header = root / "build" / "linux" / "_deps" / "sdl2-src" / "include" / "SDL_version.h"
        header.parent.mkdir(parents=True)
        header.write_text("#define SDL_MAJOR_VERSION   2\n#define SDL_MINOR_VERSION   32\n#define SDL_PATCHLEVEL      10\n")
        self.assertIn("already extracted", sdl().message)
        tarball.unlink()
        header.unlink()
        offline_dir = Path(temp.name) / "SDL2-2.32.10"
        (offline_dir / "include").mkdir(parents=True)
        (offline_dir / "include" / "SDL_version.h").write_text("")
        self.assertEqual(sdl(environ={"FETCHCONTENT_SOURCE_DIR_SDL2": str(offline_dir)}).status, "PASS")

    def test_content_failures(self) -> None:
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = write_content_root(Path(temp.name) / "content")
        (root / "game" / "defs" / "campaign.json").write_text("{}", encoding="utf-8")
        report = self.doctor("Linux", FakeEnv(set(), {}), root=root)
        self.assertEqual(report.find("Content validation").status, "FAIL")

    def test_real_environment_smoke(self) -> None:
        code, out = run_cli("doctor", "--no-network")
        self.assertIn(code, (0, 1))
        for section in ("[Host]", "[Toolchain]", "[SDL2]", "[Project]", "[Content]", "Result:"):
            self.assertIn(section, out)
        self.assertIn("Content validation: Maps, assets, definitions and campaign are valid", out)


class TestRootLaunchersAndColours(unittest.TestCase):
    def test_root_resolves_from_the_devkit_location(self) -> None:
        here = os.getcwd()
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        try:
            os.chdir(temp.name)
            self.assertEqual(cli.find_project_root(), REPO_ROOT)
            code, out = run_cli("validate", "--map", str(REPO_ROOT / "game" / "maps" / "level01.map"))
            self.assertEqual(code, 0, out)
        finally:
            os.chdir(here)

    def test_windows_launcher(self) -> None:
        data = (DEVKIT_DIR / "neo_devkit.cmd").read_bytes()
        self.assertIn(b"\r\n", data)
        self.assertNotIn(b"\n", data.replace(b"\r\n", b""))  # CRLF only (cmd.exe labels/blocks)
        text = data.decode("ascii")
        for part in ("setlocal", "endlocal & exit /b %NEO_RC%", 'set "NEO_PY=py -3"', "sys.version_info >= (3, 8)",
                     'set "NEO_RC=%ERRORLEVEL%"', "%NEO_PY% \"%SCRIPT_DIR%devkit.py\" %*"):
            self.assertIn(part, text)

    def test_unix_launcher(self) -> None:
        text = (DEVKIT_DIR / "neo_devkit").read_text(encoding="utf-8")
        self.assertIn('export PYTHONPATH="$REPO_ROOT${PYTHONPATH:+:$PYTHONPATH}"', text)
        bash = shutil.which("bash")
        if sys.platform == "win32" or bash is None:
            self.skipTest("bash launcher: not on this platform")
        env = {k: v for k, v in os.environ.items() if k != "PYTHONPATH"}
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        ok = subprocess.run([bash, str(DEVKIT_DIR / "neo_devkit"), "--no-color", "validate", "--map",
                             str(REPO_ROOT / "tests" / "fixtures" / "parity" / "crlf.map")],
                            cwd=temp.name, env=env, capture_output=True, text=True, timeout=60)
        self.assertEqual(ok.returncode, 0, ok.stdout + ok.stderr)
        bad = subprocess.run([bash, str(DEVKIT_DIR / "neo_devkit"), "--no-color", "validate", "--map",
                              str(REPO_ROOT / "tests" / "fixtures" / "parity" / "cr_only.map")],
                             cwd=temp.name, env=env, capture_output=True, text=True, timeout=60)
        self.assertEqual(bad.returncode, 1, bad.stdout + bad.stderr)  # the exit code is passed on

    def test_colours_only_on_a_capable_terminal(self) -> None:
        class Tty(io.StringIO):
            def isatty(self) -> bool:
                return True

        self.assertFalse(supports_color(False, Tty(), "linux", {}))
        self.assertFalse(supports_color(True, io.StringIO(), "linux", {}))  # a pipe or file
        self.assertFalse(supports_color(True, Tty(), "linux", {"NO_COLOR": "1"}))
        self.assertFalse(supports_color(True, Tty(), "linux", {"TERM": "dumb"}))
        self.assertTrue(supports_color(True, Tty(), "linux", {"TERM": "xterm-256color"}))
        if sys.platform != "win32":
            self.assertFalse(supports_color(True, Tty(), "win32", {}))  # no Windows console to enable VT on
        code, out = run_cli("validate", "--map", str(REPO_ROOT / "game" / "maps" / "level01.map"))
        self.assertNotIn("\033[", out)


if __name__ == "__main__":
    unittest.main()
