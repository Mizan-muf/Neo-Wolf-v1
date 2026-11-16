# Asset credits

Every image and sound in Neo-Wolf is made by a script in this repository. There are no
third-party images, textures, sprites, fonts, recordings or samples, and nothing to attribute.
The assets fall under the project's licence, the PolyForm Noncommercial License 1.0.0
([LICENSE.md](../LICENSE.md), Roadmap decision 5), like the code that draws them.

Third-party **code** (SDL2, nlohmann/json and the components bundled in SDL's source) is
listed in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

## Asset families

| Family | Files | Generator | How it is made | Licence |
|---|---|---|---|---|
| Theme textures | `game/assets/themes/<theme>/*.ppm`: 40 files, 10 per theme (`bunker`, `stone`, `tech`, `classic`) | `tools/devkit/gen_textures.py` | Drawn pixel by pixel with seeded noise; 32×32 plain-text PPM ([ASSET_GUIDE.md](ASSET_GUIDE.md#4-texture-themes)) | Project licence |
| Sprites | `game/assets/sprites/*.ppm`: 12 files (3 enemies, 3 corpses, health, ammo, 3 key cards, the decoration vase) | `tools/devkit/gen_sprites.py` | Drawn from rectangles, lines and ellipses with seeded noise; 32×32 PPM, magenta = transparent ([ASSET_GUIDE.md](ASSET_GUIDE.md#5-sprites)) | Project licence |
| Sound effects | `game/assets/sfx_weapon_fire.wav`, `sfx_door_open.wav`, `sfx_pickup.wav`, `sfx_enemy_alert.wav` | `tools/devkit/gen_sounds.py` | Synthesised: band-limited square and triangle waves, seeded noise, filters, envelopes, pitch sweeps; 16-bit PCM mono, 44.1 kHz ([ASSET_GUIDE.md](ASSET_GUIDE.md#6-sound-effects)) | Project licence |
| Application icon | not committed: built into `NeoWolf.icns` (macOS) and `neowolf.ico` (Windows) | `tools/devkit/gen_icon.py`, run by the build (`cmake/Packaging.cmake`) | Drawn from geometry (corridor, wolf's head), no randomness ([RELEASE.md](RELEASE.md)) | Project licence |
| Documentation images | `docs/images/**/*.png` | the game itself (headless `--screenshot`, converted to PNG with `sips`); `sprites.png` is a contact sheet of the 12 generated sprites (how it was assembled is not recorded; no script for it is committed) | Screenshots of the generated assets in the engine ([TESTING.md](TESTING.md)) | Project licence |

The game's 5×7 pixel font is not an asset file: its glyphs are bitmaps in the engine's source
code (`engine/ui/UiRenderer.cpp`, since commit `b21c026`), part of the engine code and its
licence; no separate record of its design exists, as for the rest of that code. The texture generator has its own small letter
set (`LETTERS` in `gen_textures.py`) for the exit sign and the key initials.

## Regenerating

```bash
python3 tools/devkit/gen_textures.py     # game/assets/themes/
python3 tools/devkit/gen_sprites.py      # game/assets/sprites/
python3 tools/devkit/gen_sounds.py       # game/assets/sfx_*.wav
python3 tools/devkit/asset_provenance.py # check: every file under game/assets is reproduced
```

Each generator also takes `--check` (exit 1 if a committed file differs from what it would
write) and `--out DIR` (write somewhere else). They use only the Python standard library and
are deterministic: a fixed seed per file, so every run writes the same bytes. To change an
asset, edit its generator, run it, look at the result in the game, and commit the script and
the regenerated files together.

## Provenance check

`tools/devkit/asset_provenance.py` runs every generator in memory and compares the output with
`game/assets`, byte for byte. A file that no generator makes (an asset of unknown origin) or
one that differs from its generator's output fails it; DevKit test `test_asset_provenance`
runs it on every test run, so the check cannot drift.

```text
$ python3 tools/devkit/asset_provenance.py
All 56 files under game/assets are reproduced byte for byte by the generators (gen_sounds.py 4, gen_sprites.py 12, gen_textures.py 40).
```

Exceptions: none. Until Phase 10, five files had no generator and no record of how they were
made (known issue X5): the four sounds (added in commit `fdcbbf5`) and the hand-made 16×16
decoration `sprite_decor.ppm` (commit `b21c026`). They were replaced by generated ones with the
same names and roles, and `sprite_decor.ppm` was deleted (the decoration is now
`sprites/decoration.ppm`), so nothing of unknown origin ships.
