"""Every relative link and anchor in the repository's Markdown files resolves
(tools/devkit/doc_links.py, Roadmap Phase 10)."""

import contextlib
import io
import shutil
import tempfile
import unittest
from pathlib import Path

from tools.devkit import doc_links


class TestDocLinks(unittest.TestCase):
    def test_repository_links_resolve(self) -> None:
        problems = doc_links.check(doc_links.REPO_ROOT)
        self.assertEqual(problems, [], "\n".join(problems))

    def test_github_slugs(self) -> None:
        cases = {
            "Light zones (`lights`)": "light-zones-lights",
            "2. Entity placements: `<level>.entities.json`": "2-entity-placements-levelentitiesjson",
            "`validate` — content and campaign validation": "validate--content-and-campaign-validation",
            "Event trace (`--trace`)": "event-trace---trace",
            "Download & play": "download--play",
            "Measured results (Phase 8 integration)": "measured-results-phase-8-integration",
            "**Bold** and [a link](x.md)": "bold-and-a-link",
            "snake_case_name": "snake_case_name",
        }
        for heading, slug in cases.items():
            self.assertEqual(doc_links.github_slug(heading), slug, heading)

    def test_broken_links_and_anchors_are_reported(self) -> None:
        root = Path(tempfile.mkdtemp(prefix="nw-doc-links-"))
        self.addCleanup(shutil.rmtree, root, True)
        (root / "docs").mkdir()
        (root / "docs" / "images").mkdir()
        (root / "docs" / "images" / "a.png").write_bytes(b"")
        (root / "docs" / "B.md").write_text("# Title\n\n## Part one\n\n## Part one\n\n<a id=\"custom\"></a>\n",
                                           encoding="utf-8")
        (root / "README.md").write_text(
            "# Readme\n\n"
            "[ok](docs/B.md) [ok](docs/B.md#part-one) [ok](docs/B.md#part-one-1) [ok](docs/B.md#custom)\n"
            "![ok](docs/images/a.png) [ok](docs/) [ok](#readme) [web](https://example.com/x.md)\n"
            "[missing](docs/C.md) [bad anchor](docs/B.md#part-two) ![gone](docs/images/b.png)\n"
            "`[in code](docs/nowhere.md)`\n"
            "```text\n[in a fence](docs/nowhere.md)\n```\n"
            "[ref]: docs/missing-ref.md\n",
            encoding="utf-8")
        # Build output is never scanned.
        (root / "build").mkdir()
        (root / "build" / "x.md").write_text("[broken](nowhere.md)\n", encoding="utf-8")

        problems = doc_links.check(root)
        self.assertEqual(len(problems), 4, "\n".join(problems))
        joined = "\n".join(problems)
        for expected in ("docs/C.md", "#part-two", "docs/images/b.png", "docs/missing-ref.md"):
            self.assertIn(expected, joined)
        self.assertTrue(all(p.startswith("README.md:") for p in problems), joined)
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(doc_links.main([str(root)]), 1)
        self.assertIn("4 broken link(s)", output.getvalue())


if __name__ == "__main__":
    unittest.main()
