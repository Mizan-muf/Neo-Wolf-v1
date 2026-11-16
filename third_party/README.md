# Third-party code vendored in this repository

Code in this folder is copied unchanged from its upstream release. Do not edit it here; update it
by downloading a newer release and recording the new version, URL and checksum below.
Licences are summarised in [docs/THIRD_PARTY_LICENSES.md](../docs/THIRD_PARTY_LICENSES.md).

## nlohmann/json

| | |
|---|---|
| What | JSON for Modern C++, single-header version |
| Version | 3.12.0 (tag `v3.12.0`, the latest release tag on 2026-11-16) |
| File | `nlohmann/json.hpp` |
| Downloaded from | <https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp> |
| SHA-256 | `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63` |
| Licence | MIT, `nlohmann/LICENSE.MIT` (from <https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT>, SHA-256 `46a65cffd1ea955132d95a8dd921640714a8d6b537d2e4e482d31145ae95b603`) |
| Used by | `engine/core/JsonDocument.h/.cpp` (shared parser), used by `engine/assets/GameplayConfig.cpp` (`gameplay.json`), `engine/assets/CampaignConfig.cpp` (`campaign.json`), `engine/world/EntityPlacementLoader.cpp` (`*.entities.json`) and `engine/core/Progress.cpp` (`progress.json`); also `engine/autoplay/AutoplayReport.cpp` (`--json` report) and `tests/LogicTests.cpp` |

The release asset is byte-identical to `single_include/nlohmann/json.hpp` at the `v3.12.0` tag
(checked when it was vendored). Both files use LF line endings, so the checksum also matches a
Git checkout on Windows.

CMake adds `third_party/` as a **SYSTEM** include directory through the `neowolf_json`
interface target (see `CMakeLists.txt`), so warnings inside the header never trigger the
project's warning flags or `-Werror` / `/WX`. Include it as `#include <nlohmann/json.hpp>` (or through
`engine/core/JsonDocument.h`), and only from `.cpp` files: the header is large and slows down every file that includes it.

To check the file:

```bash
shasum -a 256 third_party/nlohmann/json.hpp        # macOS / Linux
certutil -hashfile third_party\nlohmann\json.hpp SHA256   # Windows
```

SDL2 is not vendored: CMake downloads a pinned, checksum-verified source release at configure
time (see `CMakeLists.txt` and [docs/BUILD_MACOS.md](../docs/BUILD_MACOS.md)). SDL's source
bundles a few components under their own licences; two of them, HIDAPI and yuv2rgb, are linked
into the game, and the packages ship their notices from SDL's extracted source. The audit is in
[docs/THIRD_PARTY_LICENSES.md](../docs/THIRD_PARTY_LICENSES.md#components-bundled-in-sdl2).

Neo-Wolf's own images and sounds are not third-party: every file under `game/assets` is made by
a generator in `tools/devkit/` ([docs/ASSET_CREDITS.md](../docs/ASSET_CREDITS.md)).
