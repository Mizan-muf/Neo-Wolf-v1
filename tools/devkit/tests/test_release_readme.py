"""Tests for the package README generator (tools/devkit/release_readme.py, Roadmap Phase 9)."""

import re
import sys
import tempfile
import unittest
from pathlib import Path

DEVKIT_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = DEVKIT_DIR.parents[1]
sys.path.insert(0, str(DEVKIT_DIR))
import release_readme  # noqa: E402  (a stand-alone script, imported by path)

CONTROLS_MD = (REPO_ROOT / "docs" / "CONTROLS.md").read_text(encoding="utf-8")


def squash(text: str) -> str:
    return re.sub(r"\s+", " ", text)


class TestReleaseReadme(unittest.TestCase):
    def readme(self, platform: str) -> str:
        return release_readme.readme_text(platform, "9.8.7", "2.32.10", "3.12.0", CONTROLS_MD)

    def test_controls_come_from_controls_md(self) -> None:
        """Every row of every 'Per screen' table and of the window-keys table reaches README.txt."""
        screens, window_keys = release_readme.parse_controls(CONTROLS_MD)
        titles = [s.title for s in screens]
        self.assertEqual(
            titles,
            [
                m
                for m in re.findall(
                    r"^### (.+)$", CONTROLS_MD.split("## Per screen", 1)[1].split("\n## ", 1)[0], re.MULTILINE
                )
            ],
        )
        self.assertIn("Play", titles)
        self.assertGreaterEqual(len(window_keys), 2)
        for platform in release_readme.PLATFORMS:
            text = squash(self.readme(platform))
            for section in screens:
                self.assertIn(f"{section.title}:", text)
                self.assertTrue(section.rows, section.title)
                for keys, effect in section.rows:
                    self.assertIn(squash(f"{keys} {effect}"), text, (platform, keys))
            for keys, effect in window_keys:
                self.assertIn(squash(keys), text)
                self.assertIn(squash(effect), text)
        # The keys of the Play table, as written in CONTROLS.md (backticks removed).
        play = next(s for s in screens if s.title == "Play")
        self.assertIn(("Space, F, Ctrl", "Fire"), play.rows)
        self.assertIn(("E", next(e for k, e in play.rows if k == "E")), play.rows)

    def test_per_os_text(self) -> None:
        mac, win, linux = self.readme("macos"), self.readme("windows"), self.readme("linux")
        for text in (mac, win, linux):
            self.assertIn("Neo-Wolf 9.8.7", text)
            self.assertIn("Copyright (c) 2026 Mizan-muf", text)
            self.assertIn("PolyForm Noncommercial License 1.0.0", squash(text))
            self.assertIn("Commercial use needs the copyright holder's permission", squash(text))
            self.assertIn("LICENSE.md", text)
            self.assertNotIn("to be decided", text)
            self.assertIn("SDL2 2.32.10 (zlib licence)", squash(text))
            self.assertIn("nlohmann/json 3.12.0 (MIT licence)", squash(text))
            self.assertIn("progress.json", text)
            self.assertIn("neowolf.log", text)
            self.assertTrue(
                all(len(line) <= 78 for line in text.splitlines()),
                [line for line in text.splitlines() if len(line) > 78],
            )
            text.encode("ascii")  # plain ASCII: readable in any editor
        self.assertIn("xattr -dr com.apple.quarantine", squash(mac))
        self.assertIn("Privacy & Security", mac)
        self.assertIn("Open Anyway", squash(mac))
        self.assertIn("~/Library/Application Support/NeoWolf/NeoWolf/", mac)
        self.assertIn('"More info", then "Run anyway"', squash(win))
        self.assertIn("%APPDATA%\\NeoWolf\\NeoWolf\\", win)
        self.assertNotIn("Gatekeeper", win)
        self.assertNotIn("SmartScreen", mac)
        self.assertIn("~/.local/share/NeoWolf/NeoWolf/", linux)

    def test_command_line_and_newlines(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / "README.txt"
            self.assertEqual(release_readme.main(["--platform", "windows", "--version", "1.0.0", "--out", str(out)]), 0)
            data = out.read_bytes()
            self.assertIn(b"\r\n", data)
            self.assertNotIn(b"\n", data.replace(b"\r\n", b""))
            self.assertEqual(release_readme.main(["--platform", "macos", "--version", "1.0.0", "--out", str(out)]), 0)
            self.assertNotIn(b"\r", out.read_bytes())
            broken = Path(temp) / "CONTROLS.md"
            broken.write_text("# Controls\n\nNothing here.\n", encoding="utf-8")
            self.assertEqual(
                release_readme.main(
                    ["--platform", "linux", "--version", "1.0.0", "--controls", str(broken), "--out", str(out)]
                ),
                1,
            )


if __name__ == "__main__":
    unittest.main()
