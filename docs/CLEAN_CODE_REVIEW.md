# Clean-code review

The Phase 12 review of the whole code base against [CODING_STANDARDS.md](CODING_STANDARDS.md).
Each finding records where it was, which principle it breaks, the fix, and its status
(**Fixed**, or **Kept** with the reason).

**Status: in progress.** The automated checks and formatting are in place; the module reviews
are being merged. This page is completed when Phase 12 is (see the [roadmap](ROADMAP.md)).

## Scope

| Area | Modules |
|---|---|
| Engine core | `engine/core`, `src/` |
| Platform and audio | `engine/platform`, `engine/audio`, `engine/particles` |
| Rendering and UI | `engine/render`, `engine/ui` |
| Game world and content | `engine/world`, `engine/assets` |
| C API and autoplay bot | `engine/interop`, `engine/autoplay` |
| Tests | `tests/` |
| Python tools | `tools/devkit`, `tools/ci` |

## Baseline before the review

| Check | Findings |
|---|---|
| clang-format | 74 of 92 C++ files needed formatting (fixed in one formatting-only commit) |
| ruff format | 34 of 81 Python files needed formatting (fixed in one formatting-only commit) |
| clang-tidy ([.clang-tidy](../.clang-tidy)) | 374 diagnostics |
| ruff check ([ruff.toml](../ruff.toml)) | 261 findings |
