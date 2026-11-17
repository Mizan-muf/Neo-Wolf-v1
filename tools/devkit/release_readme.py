#!/usr/bin/env python3
"""Writes the README.txt that ships in a Neo-Wolf release package (Roadmap Phase 9).

One text per OS (macos, windows, linux): how to start the game, the Gatekeeper or SmartScreen
warning on first start, the controls, where saves and logs live, and the licence notices.

The controls are not written here: they are converted from docs/CONTROLS.md (its "Per screen"
section and its window-keys table), so the package can never disagree with that page. The
DevKit test `test_release_readme` checks that every row of those tables reaches the text.

The build calls this script (cmake/Packaging.cmake) and installs the result; the DevKit
`package` command checks that the package contains it. Standard library only.

Usage:
    python3 tools/devkit/release_readme.py --platform macos --version 1.1.0 --out README.txt
"""

from __future__ import annotations

import argparse
import re
import sys
import textwrap
from pathlib import Path
from typing import List, Sequence, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTROLS = REPO_ROOT / "docs" / "CONTROLS.md"
REPOSITORY_URL = "https://github.com/Mizan-muf/Neo-Wolf-v1"
COPYRIGHT = "Copyright (c) 2026 Mizan-muf"  # the "Required Notice" of LICENSE.md
WIDTH = 78
KEY_COLUMN_MAX = 26  # wider key cells get a line of their own, so the effects stay aligned
PLATFORMS = ("macos", "windows", "linux")

Table = List[Tuple[str, str]]


def plain(markdown: str) -> str:
    """Inline Markdown to plain text: links keep their text; backticks and bold markers go."""
    text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", markdown)
    text = text.replace("**", "").replace("`", "")
    text = text.replace("\\|", "|").replace("×", "x").replace("→", "->").replace("…", "...")
    return text.strip()


def _split_row(line: str) -> list[str]:
    cells = re.split(r"(?<!\\)\|", line.strip().strip("|"))
    return [plain(cell) for cell in cells]


def _parse_table(lines: Sequence[str], start: int) -> tuple[Table, int]:
    """A two-column Markdown table starting at lines[start] (the header row)."""
    rows: Table = []
    i = start + 2  # skip the header and the |---|---| line
    while i < len(lines) and lines[i].lstrip().startswith("|"):
        cells = _split_row(lines[i])
        if len(cells) >= 2:
            rows.append((cells[0], cells[1]))
        i += 1
    return rows, i


class ControlsSection:
    def __init__(self, title: str) -> None:
        self.title = title
        self.rows: Table = []
        self.notes: list[str] = []


def parse_controls(markdown: str) -> tuple[list[ControlsSection], Table]:
    """The "Per screen" subsections (tables and their notes) and the window-keys table."""
    lines = markdown.splitlines()
    screens: list[ControlsSection] = []
    window_keys: Table = []
    in_per_screen = False
    current: ControlsSection | None = None
    paragraph: list[str] = []

    def flush_paragraph() -> None:
        if paragraph and current is not None:
            current.notes.append(plain(" ".join(paragraph)))
        paragraph.clear()

    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if stripped.startswith("## "):
            flush_paragraph()
            in_per_screen = stripped == "## Per screen"
            current = None
        elif stripped.startswith("### ") and in_per_screen:
            flush_paragraph()
            current = ControlsSection(plain(stripped[4:]))
            screens.append(current)
        elif stripped.startswith("Window keys") and not window_keys:
            # "Window keys, handled by the platform layer ...:" followed by a table.
            j = i + 1
            while j < len(lines) and not lines[j].strip():
                j += 1
            if j < len(lines) and lines[j].lstrip().startswith("|"):
                window_keys, i = _parse_table(lines, j)
                continue
        elif stripped.startswith("|") and current is not None:
            flush_paragraph()
            rows, i = _parse_table(lines, i)
            current.rows.extend(rows)
            continue
        elif current is not None:
            if stripped:
                paragraph.append(stripped)
            else:
                flush_paragraph()
        i += 1
    flush_paragraph()
    return screens, window_keys


def _format_table(rows: Table, indent: str = "  ") -> list[str]:
    key_width = min(KEY_COLUMN_MAX, max((len(k) for k, _ in rows), default=0))
    out: list[str] = []
    for keys, effect in rows:
        lead = f"{indent}{keys.ljust(key_width)}  "
        if len(keys) > key_width:
            out.append(f"{indent}{keys}")
            lead = " " * (len(indent) + key_width + 2)
        hanging = " " * (len(indent) + key_width + 2)
        out.extend(
            textwrap.wrap(effect, WIDTH, initial_indent=lead, subsequent_indent=hanging, break_on_hyphens=False)
            or [lead.rstrip()]
        )
    return out


def controls_text(markdown: str) -> list[str]:
    """The CONTROLS section of README.txt, converted from docs/CONTROLS.md."""
    screens, window_keys = parse_controls(markdown)
    if not screens or not window_keys:
        raise ValueError("docs/CONTROLS.md: the 'Per screen' section or the 'Window keys' table was not found")
    out: list[str] = []
    for section in screens:
        out.append(f"{section.title}:")
        out.extend(_format_table(section.rows))
        for note in section.notes:
            out.extend(textwrap.wrap(note, WIDTH, initial_indent="  ", subsequent_indent="  "))
        out.append("")
    out.append("Window (any screen):")
    out.extend(_format_table(window_keys))
    return out


def _heading(title: str) -> list[str]:
    return ["", title, "-" * len(title)]


def _wrap(text: str, indent: str = "", hanging: str | None = None) -> list[str]:
    return textwrap.wrap(
        text,
        WIDTH,
        initial_indent=indent,
        subsequent_indent=indent if hanging is None else hanging,
        break_long_words=False,
        break_on_hyphens=False,
    )


def readme_text(platform: str, version: str, sdl_version: str, json_version: str, controls_markdown: str) -> str:
    if platform not in PLATFORMS:
        raise ValueError(f"unknown platform {platform!r} (expected one of {', '.join(PLATFORMS)})")
    label = {
        "macos": "macOS (universal: Apple silicon and Intel)",
        "windows": "Windows (64-bit)",
        "linux": "Linux (64-bit)",
    }[platform]
    title = f"Neo-Wolf {version} for {label}"
    out: list[str] = [title, "=" * len(title), ""]
    out += _wrap(
        "A retro first-person shooter: a software raycaster with a ten-level campaign. "
        "Find the exit switch of each level; key cards open locked doors."
    )

    out += _heading("What is in this folder")
    if platform == "macos":
        out += ["  NeoWolf.app   the game (its content is inside the app)"]
    elif platform == "windows":
        out += ["  neo_wolf.exe  the game", "  game\\         the game's content (keep it next to neo_wolf.exe)"]
    else:
        out += ["  neo_wolf      the game", "  game/         the game's content (keep it next to neo_wolf)"]
    sep = "\\" if platform == "windows" else "/"
    out += [
        "  README.txt    this file",
        "  LICENSE.md    the game's licence (PolyForm Noncommercial 1.0.0)",
        f"  docs{sep}         CONTROLS.md (every key, per screen) and TROUBLESHOOTING.md",
        f"  LICENSES{sep}     notices and licence texts of the third-party libraries",
    ]

    out += _heading("Starting the game")
    if platform == "macos":
        out += _wrap(
            "Requires macOS 10.15 (Catalina) or newer. Double-click NeoWolf.app. You may move "
            "it anywhere first, for example into Applications; it does not need this folder."
        )
        out += _heading('First start: "cannot be opened" (Gatekeeper)')
        out += _wrap(
            "This app is signed ad hoc but not notarized by Apple (the project has no Apple "
            "Developer ID yet), so macOS blocks the first start of a downloaded copy with a "
            'message such as "NeoWolf.app cannot be opened because Apple cannot check it for '
            'malicious software" or "Apple could not verify NeoWolf.app is free of malware". '
            "To allow it once:"
        )
        out += [""]
        out += _wrap(
            "- macOS 14 (Sonoma) and older: Control-click (right-click) NeoWolf.app, choose "
            "Open, then Open again in the dialog.",
            "  ",
            "    ",
        )
        out += _wrap(
            "- macOS 15 (Sequoia) and newer: double-click NeoWolf.app once and close the "
            "message, then open System Settings > Privacy & Security, scroll down to "
            '"NeoWolf.app was blocked", click Open Anyway and confirm.',
            "  ",
            "    ",
        )
        out += _wrap(
            "- Or, in Terminal (any version): xattr -dr com.apple.quarantine "
            "/path/to/NeoWolf.app (drag the app into the Terminal window to type its path).",
            "  ",
            "    ",
        )
        out += [""]
        out += _wrap("Do not turn Gatekeeper off for the whole system. After the first start the app opens normally.")
    elif platform == "windows":
        out += _wrap(
            "Requires 64-bit Windows 10 or 11. Extract the whole zip first (right-click > "
            "Extract All...); the game cannot find its content when started from inside the "
            "zip. Then double-click neo_wolf.exe. No installer and no Visual C++ "
            "redistributable are needed."
        )
        out += _heading('First start: "Windows protected your PC" (SmartScreen)')
        out += _wrap(
            "neo_wolf.exe is not code-signed (the project has no code-signing certificate), "
            "so Microsoft Defender SmartScreen may stop the first start of a downloaded copy. "
            'Click "More info", then "Run anyway". To avoid the prompt, you can instead '
            "right-click the downloaded zip > Properties > tick Unblock > OK before extracting it."
        )
        out += [""]
        out += _wrap("Only do this for a copy downloaded from the project's own release page.")
    else:
        out += _wrap(
            "Best effort: built in CI, not yet tested on a Linux desktop. Run ./neo_wolf from "
            "this folder (or from anywhere: it finds game/ next to itself). It needs a desktop "
            "session (X11 or Wayland) with the usual system libraries."
        )

    out += _heading("Controls")
    out += controls_text(controls_markdown)

    out += _heading("Saves and log files")
    if platform == "macos":
        folder = "~/Library/Application Support/NeoWolf/NeoWolf/"
        how = "(Finder > Go > Go to Folder..., then paste the path)"
    elif platform == "windows":
        folder = "%APPDATA%\\NeoWolf\\NeoWolf\\"
        how = "(paste it into the Explorer address bar)"
    else:
        folder = "~/.local/share/NeoWolf/NeoWolf/ (or $XDG_DATA_HOME/NeoWolf/NeoWolf/)"
        how = ""
    out += _wrap(f"Progress and logs are kept per user, outside the game's folder, in {folder} {how}".rstrip())
    out += [
        "",
        "  progress.json      which levels are unlocked (delete it to start over)",
        "  neowolf.log        the log of the current or last run",
        "  neowolf.prev.log   the log of the run before that",
        "",
    ]
    out += _wrap(
        "Deleting or replacing the game keeps your progress. If the game does not start, "
        f"look at the log first and see docs{sep}TROUBLESHOOTING.md."
    )

    out += _heading("Command-line options (optional)")
    exe = {"macos": "NeoWolf.app/Contents/MacOS/NeoWolf", "windows": "neo_wolf.exe", "linux": "./neo_wolf"}[platform]
    out += [f"  {exe}", "      --windowed | --fullscreen | --version | --help"]
    out += _wrap(
        "--pref-dir <folder> uses another folder for saves and logs (for example a portable copy on a USB stick).",
        "  ",
        "    ",
    )

    out += _heading("Licence")
    out += _wrap(
        f"{COPYRIGHT}. Neo-Wolf is free for any noncommercial use under the PolyForm "
        "Noncommercial License 1.0.0: personal play, study, hobby projects and modding, and "
        "use by schools, charities and public bodies. Commercial use needs the copyright "
        "holder's permission. The full terms are in LICENSE.md."
    )
    out += [""]
    out += _wrap(
        f"Third-party software in this game: SDL2 {sdl_version} (zlib licence) and "
        f"nlohmann/json {json_version} (MIT licence). Their notices are in the LICENSES "
        "folder."
    )
    out += ["", f"Version {version}. Source, issues and releases:", f"  {REPOSITORY_URL}"]
    return "\n".join(out) + "\n"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--platform", required=True, choices=PLATFORMS)
    parser.add_argument("--version", required=True, help="the game's version (CMake's PROJECT_VERSION)")
    parser.add_argument("--sdl-version", default="2", help="the SDL2 version linked into the game")
    parser.add_argument("--json-version", default="3", help="the nlohmann/json version compiled in")
    parser.add_argument("--controls", type=Path, default=DEFAULT_CONTROLS, help="docs/CONTROLS.md")
    parser.add_argument("--out", type=Path, required=True, help="the README.txt to write")
    args = parser.parse_args(argv)
    try:
        text = readme_text(
            args.platform, args.version, args.sdl_version, args.json_version, args.controls.read_text(encoding="utf-8")
        )
    except (OSError, ValueError) as exc:
        print(f"release_readme: {exc}", file=sys.stderr)
        return 1
    newline = "\r\n" if args.platform == "windows" else "\n"  # Notepad on older Windows 10
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with open(args.out, "w", encoding="utf-8", newline=newline) as handle:
        handle.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
