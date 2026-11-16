# Releases and packages

How Neo-Wolf is versioned, packaged, checked and released (Roadmap Phase 9). Building from
source for development: [BUILD_MACOS.md](BUILD_MACOS.md), [BUILD_WINDOWS.md](BUILD_WINDOWS.md),
[BUILD_AND_RELEASE.md](BUILD_AND_RELEASE.md).

| Package | Built on | File |
|---|---|---|
| macOS, universal (Apple silicon + Intel; Intel tested under Rosetta only), macOS 10.15+ | a Mac | `neo-wolf-<version>-macos-universal.zip` |
| Windows x64, Windows 10/11 | Windows with Visual Studio 2022+ | `neo-wolf-<version>-windows-x64.zip` |
| Linux x86_64 (best effort, not tested on a desktop) | Linux | `neo-wolf-<version>-linux-x86_64.tar.gz` |

Each run also writes or updates `SHA256SUMS` next to the archives.

## Versioning

- **One source:** `project(NeoWolf VERSION X.Y.Z)` in `CMakeLists.txt` (the marked block above
  it). Everything else is derived from it at configure time: `neo_wolf --version` and the first
  log line, the autoplay JSON report, `NeoWolf.app`'s `Info.plist`
  (`CFBundleShortVersionString`, `CFBundleVersion`), `neo_wolf.exe`'s version resource
  (Explorer → Properties → Details), the package `README.txt`, and the package file names
  (`cmake/Packaging.cmake`, which also writes `build/<preset>/neowolf-package.json` for the
  DevKit).
- **Scheme:** `MAJOR.MINOR.PATCH`. Development was `0.9.0` until the v1 release; Roadmap Phase 12
  set `1.0.0` and tagged `v1.0.0`. After that: patch for fixes, minor for new content or
  features, major for save or content-format breaks.
- **Why a literal and not a cache variable** (decision of Phase 9): a CMake cache variable
  keeps its first value in an existing build folder, so a later edit of the default would not
  reach it, and a tag built from source would still say whatever the builder's cache said. The
  literal changes in exactly one place, in the commit that gets the tag, and every fresh or
  existing build folder picks it up on the next build (CMake re-runs configure when
  `CMakeLists.txt` changes).
- **Check:** `devkit package --expect-version X.Y.Z` fails unless the CMake version is `X.Y.Z`.
  CI passes the tag's version on `v*` tags, so a tag that does not match `CMakeLists.txt`
  produces no package.

## Building the packages

The DevKit `package` command does everything: it validates the content (`validate --strict`
rules), configures and builds the package preset for this OS (`package-macos`,
`package-windows-msvc`, `package-linux`: Release, no tests, warnings as errors; build folder
`build/<preset>/`), runs `cmake --install` into a staging folder, checks the staged tree, then
writes the archive and `SHA256SUMS`. It exits non-zero and writes no archive when anything is
missing, in particular the executable (known issue D1, resolved).

```bash
./package.sh                          # macOS / Linux -> release/
./package.sh --out-dir dist --clean   # another folder, without older archives
python3 tools/ci/package_smoke.py release   # check what was built (see "Smoke test")
```

```cmd
package.cmd                           rem Windows, Command Prompt -> release\
.\package.ps1                         # Windows PowerShell
```

`make package` and `make package-smoke` do the same on macOS and Linux. All options:
`python3 tools/devkit/devkit.py package --help` ([DEVKIT_GUIDE.md](DEVKIT_GUIDE.md#package--release-package)).
The first run builds SDL2 again for the package preset (a few minutes); later runs only relink.
Without internet, configure the preset once with
`-DFETCHCONTENT_SOURCE_DIR_SDL2=<extracted SDL2-2.32.10>` ([BUILD_MACOS.md](BUILD_MACOS.md)).

Requirements: the normal build requirements of the OS plus Python 3 (it generates the icon and
the `README.txt` during the build). On macOS `iconutil` and `codesign` come with the Xcode
Command Line Tools.

Other ways, same result:
- `cmake --install build/<preset> --prefix <folder>` installs the package tree without an archive
  (the default prefix is `build/<preset>/install`, never `/usr/local`).
- `cpack --config build/<preset>/CPackConfig.cmake` writes the same archive into
  `build/<preset>/cpack/` (no `SHA256SUMS`, no checks).
- Any configured build folder, e.g. a cross-compile:
  `devkit package --build-dir <folder>`. A MinGW build is named `...-windows-x64-mingw` so it
  cannot be confused with the MSVC release; it is a build check only and not a release package.

## What is in each package

One top folder named like the archive, so extracting never spills files.

**macOS** (`neo-wolf-<version>-macos-universal/`):

```text
NeoWolf.app/
  Contents/Info.plist          bundle id io.github.mizan-muf.neowolf, version, macOS 10.15+,
                               high resolution, category Games, copyright
  Contents/MacOS/NeoWolf       universal (arm64 + x86_64); SDL2 linked statically
  Contents/Resources/NeoWolf.icns
  Contents/Resources/game/     maps, definitions, textures, sprites, sounds
  Contents/_CodeSignature/     ad-hoc signature (seals the content too)
README.txt                     how to start it, Gatekeeper, controls, saves and logs, licence
LICENSE.md                     the project licence (PolyForm Noncommercial 1.0.0)
docs/CONTROLS.md, docs/TROUBLESHOOTING.md
LICENSES/                      SDL2-LICENSE.txt (zlib), SDL2-hidapi-LICENSE-bsd.txt,
                               SDL2-yuv2rgb-LICENSE.txt, nlohmann-json-LICENSE.MIT.txt,
                               THIRD_PARTY_LICENSES.md
```

**Windows** (`neo-wolf-<version>-windows-x64/`): `neo_wolf.exe` (GUI subsystem, static C runtime
and SDL2, so no DLLs and no Visual C++ redistributable; icon and version resource with
`LegalCopyright`), `game/`, `README.txt` (CRLF line endings), `LICENSE.md`, `docs/`, `LICENSES/`.

**Linux** (`neo-wolf-<version>-linux-x86_64/`): `neo_wolf`, `game/`, `README.txt`, `LICENSE.md`,
`docs/`, `LICENSES/`. Best effort: the SDL2 it contains supports only the video and audio back
ends whose development packages were installed on the build machine
([BUILD_AND_RELEASE.md](BUILD_AND_RELEASE.md)).

Never shipped: the test executable `neo_wolf_tests`, the autoplay bot `neo_wolf_autoplay`, and
the entity `.csv` exports (the game loads the `.json` files named in `campaign.json`). The
DevKit and the smoke test fail if a test or autoplay executable appears.

How the pieces are made:
- **Icon:** drawn by `tools/devkit/gen_icon.py` (standard-library Python, deterministic, no
  third-party artwork) **at build time**: an `.iconset` turned into `NeoWolf.icns` by `iconutil`
  on macOS, a PNG-in-ICO `neowolf.ico` on Windows. Nothing generated is committed, so the icon
  can never be out of date with its generator. Without Python the build still works but has no
  icon and no `README.txt`, and `devkit package` refuses the result.
- **README.txt:** written by `tools/devkit/release_readme.py` per OS at build time. Its controls
  section is converted from [CONTROLS.md](CONTROLS.md) (the "Per screen" tables and the window
  keys), so the two cannot disagree; DevKit test `test_release_readme` checks every row.
- **Info.plist / version resource:** `packaging/macos/Info.plist.in`,
  `packaging/windows/neo_wolf.rc.in`, configured by `cmake/Packaging.cmake`.
- **Content root:** inside the app, SDL reports `Contents/Resources` as the base path, and the
  log says `Content root: .../NeoWolf.app/Contents/Resources (app bundle Resources)`. On
  Windows and Linux the content is next to the executable (`executable directory`). The game
  does not depend on the working directory ([TROUBLESHOOTING.md](TROUBLESHOOTING.md#game-content-not-found)).
- In the build tree, `build/<preset>/NeoWolf.app` exists but has no `game/` inside; it is only
  the input of `cmake --install`. Develop with `build/<preset>/neo_wolf`.

## Checksums

`SHA256SUMS` lists every archive written into the output folder, one `sha256  file name` line
each (other packages' lines are kept, so one folder can collect all of a release). Check a
download:

```bash
shasum -a 256 -c SHA256SUMS --ignore-missing     # macOS
sha256sum -c SHA256SUMS --ignore-missing         # Linux
```

```powershell
Get-FileHash .\neo-wolf-1.0.0-windows-x64.zip -Algorithm SHA256   # compare with SHA256SUMS
```

The checksums prove the download is the file CI built; they are not a signature.

## Smoke test

`tools/ci/package_smoke.py <archive | folder with SHA256SUMS>` (standard library only):

1. checks the archive against `SHA256SUMS`;
2. extracts it into a new folder whose path contains spaces, keeping the Unix permissions;
3. checks the layout: one top folder, the executable (and its execute bit), the content,
   `README.txt` with the version and the licence summary, `LICENSE.md` with its Required Notice,
   `docs/`, `LICENSES/`, no test or autoplay executable;
4. macOS: the `Info.plist` values and `plutil -lint`, `codesign --verify --deep --strict`,
   `lipo -info` (both architectures), `otool -L` (only `/usr/lib` and `/System/Library`);
   Windows: from the PE header of `neo_wolf.exe`, x64, GUI subsystem, an icon, a version resource
   with the version and copyright, and no SDL or C/C++ runtime DLL among the imports (these
   checks read the file, so they also run on a MinGW cross-build on macOS);
5. on the package's own OS: `--version`, then runs the game headless (SDL dummy drivers)
   with `--level 10 --frames 300 --pref-dir <temporary folder>`, once from the extracted folder
   and once with the working directory at the file-system root (`/`, or the current drive's root
   such as `C:\` on Windows); each run must exit 0 and its log must
   name the content root inside the package.

It never opens a window. `--no-run` keeps only the static checks (used by the DevKit test
`test_package_smoke_static_checks_pass_on_the_result`).

Where it runs:
- **CTest** (opt-in, slow: it builds the package preset): configure with
  `-DNEOWOLF_PACKAGE_TESTS=ON`, then `ctest --preset macos -L package` (tests `package.build`
  and `package.smoke`).
- **CI**, job `package` (macOS and Windows): `package.sh` / `package.cmd` → `package_smoke.py`
  → uploads the archive and `SHA256SUMS` as the artifact `neo-wolf-package-<OS>`. The job
  `windows-scripts` checks that `package.cmd` and `package.ps1` pass exit codes on.

## What is not done (and what players do instead)

- **No Apple notarization, no Developer ID signature.** The app is signed ad hoc only (it runs
  on Apple silicon and its content is sealed, but Gatekeeper does not know the signer). A
  downloaded copy is blocked on first start ("cannot be opened because Apple cannot check it",
  or "Apple could not verify ..."). Players: macOS 14 and older, Control-click the app → Open →
  Open; macOS 15 and newer, try to open it once, then System Settings → Privacy & Security →
  Open Anyway; or `xattr -dr com.apple.quarantine NeoWolf.app`. Notarization needs a paid Apple
  Developer account (out of scope for v1, Roadmap decisions).
- **No Windows code signing.** SmartScreen may show "Windows protected your PC" for a
  downloaded copy: More info → Run anyway (or Properties → Unblock on the zip before extracting).
- **No installer, no DMG**, no auto-update: unzip and run.
- **Linux** packages are best effort (not built in CI, not tested on a desktop).

The package `README.txt` and [TROUBLESHOOTING.md](TROUBLESHOOTING.md#packaged-builds) explain
these to players.

## Release checklist

1. Phase gates: Roadmap Phases 9–12 done (Phase 11 sign-off, Phase 12 clean-code review).
2. `CHANGELOG` / docs updated (Roadmap Phase 10).
   Content: `"bootIntoTestLevel"` is `false` or absent in `game/defs/campaign.json`, so the game
   starts on the main menu and not in the Test Chamber
   ([CONTENT_DEFINITIONS.md](CONTENT_DEFINITIONS.md#test-level)); remove `"testLevel"` too if
   the release should not offer the Test Chamber on the menu. `package --expect-version` (step
   3 and the `v*` tag build in CI) fails while the flag is `true`; other packages only warn.
3. In one commit: set `project(NeoWolf VERSION X.Y.Z)` in `CMakeLists.txt` (Phase 12: `1.0.0`),
   update version mentions in the docs if any, run the full verification
   (`./build.sh`, `ctest --preset ci-macos` after `cmake --build --preset ci-macos`,
   `./package.sh --clean --expect-version X.Y.Z`, `python3 tools/ci/package_smoke.py release`).
4. Push the commit; CI must be green on every job, including `package` on macOS and Windows.
5. Tag it: `git tag -a vX.Y.Z -m "Neo-Wolf X.Y.Z"` and push the tag. CI builds both packages
   again with `--expect-version X.Y.Z` and uploads them.
6. Download both artifacts, put both archives in one folder and merge the two `SHA256SUMS`
   files (or run `package_smoke.py` on each artifact folder), then check them by hand: on a Mac,
   unzip in Finder and start `NeoWolf.app` (Gatekeeper path above); on a Windows PC, extract
   and start `neo_wolf.exe` (SmartScreen path above). Play the first level briefly.
7. Create the GitHub release for the tag with both archives, the merged `SHA256SUMS`, the
   changelog, and the Gatekeeper/SmartScreen notes from above.

## Licence in the package

Every package ships `LICENSE.md` (the project's PolyForm Noncommercial License 1.0.0, with its
`Required Notice:` line, which the licence requires to travel with every copy) and the
third-party notices in `LICENSES/`. The `Info.plist` (`NSHumanReadableCopyright`) and the
Windows version resource (`LegalCopyright`) say `Copyright (c) 2026 Mizan-muf`. See
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
