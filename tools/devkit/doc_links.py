#!/usr/bin/env python3
"""Checks the relative links and anchors in the repository's Markdown files (Roadmap Phase 10).

Every relative link (``[text](path)``, ``![alt](path)``, ``[text][ref]`` definitions
``[ref]: path``) in every ``*.md`` file must point at a file or folder that exists, and a
``#fragment`` must name a heading (GitHub's anchor rules) or an explicit ``<a id/name>`` in the
target file. Links with a scheme (``https:``, ``mailto:``...) are not checked: the check needs no
network. Code blocks and inline code are ignored.

DevKit test ``test_doc_links`` runs it on the repository, so links cannot rot unnoticed:

    python3 tools/devkit/doc_links.py            # check the repository (exit 1 on a broken link)
    python3 tools/devkit/doc_links.py --list     # also list every checked file

Standard library only.
"""
from __future__ import annotations

import argparse
import re
import sys
import unicodedata
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Set, Tuple
from urllib.parse import unquote

REPO_ROOT = Path(__file__).resolve().parents[2]

# Folders never scanned: build output, packages, VCS data, dependencies fetched by CMake.
SKIPPED_DIRS = {".git", "build", "release", "dist", "_deps", "node_modules", "__pycache__", ".venv"}

FENCE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")
INLINE_CODE = re.compile(r"(`+)(?:.+?)\1")
# [text](target) and ![alt](target); the target may be <wrapped> and may carry a "title".
INLINE_LINK = re.compile(r"!?\[(?:[^\[\]]|\[[^\[\]]*\])*\]\(\s*(<[^>]*>|[^)\s]+)(?:\s+(?:\"[^\"]*\"|'[^']*'))?\s*\)")
REFERENCE_DEF = re.compile(r"^\s{0,3}\[[^\]]+\]:\s*(<[^>]*>|\S+)")
HEADING = re.compile(r"^\s{0,3}(#{1,6})\s+(.*?)\s*#*\s*$")
HTML_ANCHOR = re.compile(r"<a\s+[^>]*(?:id|name)\s*=\s*[\"']([^\"']+)[\"']", re.IGNORECASE)
SCHEME = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")


def _heading_text(markdown: str) -> str:
    """Rendered text of heading Markdown outside code spans."""
    text = re.sub(r"!\[([^\]]*)\]\([^)]*\)", r"\1", markdown)  # images -> alt text
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)      # links -> text
    text = re.sub(r"<[^>]+>", "", text)                          # inline HTML
    return text.replace("*", "")


def github_slug(heading: str) -> str:
    """The anchor GitHub gives a heading (without the -1, -2 suffix of repeated headings)."""
    parts = []
    last = 0
    for code in re.finditer(r"(`+)(.+?)\1", heading):  # code spans are kept literally
        parts.append(_heading_text(heading[last:code.start()]))
        parts.append(code.group(2))
        last = code.end()
    parts.append(_heading_text(heading[last:]))
    text = "".join(parts)
    out = []
    for ch in text.lower():
        if ch in " -_" or ch.isalnum() or unicodedata.category(ch).startswith("M"):
            out.append("-" if ch == " " else ch)
    return "".join(out)


def _strip_code(lines: Iterable[str], keep_inline_code: bool = False) -> List[Tuple[int, str]]:
    """The lines outside fenced code blocks, (line number, text); inline code is removed unless
    `keep_inline_code` (headings keep it: GitHub builds their anchors from the code's text)."""
    result: List[Tuple[int, str]] = []
    fence: Optional[str] = None
    for number, line in enumerate(lines, start=1):
        match = FENCE.match(line)
        if fence is not None:
            if match and match.group(1)[0] == fence[0] and len(match.group(1)) >= len(fence):
                fence = None
            continue
        if match:
            fence = match.group(1)
            continue
        result.append((number, line if keep_inline_code else INLINE_CODE.sub("", line)))
    return result


def anchors_of(path: Path, cache: Dict[Path, Set[str]]) -> Set[str]:
    """Every anchor a Markdown file defines: heading slugs (with GitHub's -N suffixes) and
    explicit HTML anchors."""
    if path in cache:
        return cache[path]
    anchors: Set[str] = set()
    counts: Dict[str, int] = {}
    text = path.read_text(encoding="utf-8", errors="replace")
    for _, line in _strip_code(text.splitlines(), keep_inline_code=True):
        heading = HEADING.match(line)
        if heading:
            slug = github_slug(heading.group(2))
            seen = counts.get(slug, 0)
            anchors.add(slug if seen == 0 else f"{slug}-{seen}")
            counts[slug] = seen + 1
        for anchor in HTML_ANCHOR.findall(line):
            anchors.add(anchor)
    cache[path] = anchors
    return anchors


def links_of(path: Path) -> List[Tuple[int, str]]:
    """(line number, target) of every link in a Markdown file, outside code."""
    text = path.read_text(encoding="utf-8", errors="replace")
    found: List[Tuple[int, str]] = []
    for number, line in _strip_code(text.splitlines()):
        for match in INLINE_LINK.finditer(line):
            found.append((number, match.group(1)))
        definition = REFERENCE_DEF.match(line)
        if definition:
            found.append((number, definition.group(1)))
    return [(number, target[1:-1] if target.startswith("<") else target) for number, target in found]


def markdown_files(root: Path) -> List[Path]:
    files = []
    for path in sorted(root.rglob("*.md")):
        relative = path.relative_to(root)
        if any(part in SKIPPED_DIRS for part in relative.parts[:-1]):
            continue
        files.append(path)
    return files


def _shown(path: Path, root: Path) -> str:
    """`path` relative to `root` when it is inside it (Python 3.8 has no Path.is_relative_to)."""
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def check(root: Path = REPO_ROOT, files: Optional[List[Path]] = None) -> List[str]:
    """Every broken relative link or anchor under `root`, as "file:line: message"."""
    root = root.resolve()
    problems: List[str] = []
    cache: Dict[Path, Set[str]] = {}
    for source in files if files is not None else markdown_files(root):
        source = source.resolve()
        shown = _shown(source, root)
        for number, target in links_of(source):
            if not target or SCHEME.match(target) or target.startswith("//"):
                continue
            path_part, _, fragment = target.partition("#")
            path_part = unquote(path_part)
            if path_part.startswith("/"):
                destination = root / path_part.lstrip("/")
            elif path_part:
                destination = source.parent / path_part
            else:
                destination = source
            if not destination.exists():
                problems.append(f"{shown}:{number}: {target}: {path_part} does not exist")
                continue
            if not fragment:
                continue
            if destination.is_dir() or destination.suffix.lower() != ".md":
                problems.append(f"{shown}:{number}: {target}: an anchor needs a Markdown file")
                continue
            if unquote(fragment).lower() not in {a.lower() for a in anchors_of(destination.resolve(), cache)}:
                problems.append(f"{shown}:{number}: {target}: no heading or anchor #{fragment} in "
                                f"{_shown(destination.resolve(), root)}")
    return problems


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Check relative links and anchors in Markdown files.")
    parser.add_argument("root", nargs="?", type=Path, default=REPO_ROOT, help="repository root (default: this repository)")
    parser.add_argument("--list", action="store_true", help="list every checked file")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    files = markdown_files(root)
    if args.list:
        for path in files:
            print(path.relative_to(root).as_posix())
    problems = check(root, files)
    for problem in problems:
        print(problem)
    print(f"{len(files)} Markdown file(s) checked: "
          + ("all relative links and anchors resolve." if not problems else f"{len(problems)} broken link(s)."))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
