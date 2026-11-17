# Neo-Wolf roadmap after v1.0

The v1.0 work is recorded in [ROADMAP.md](ROADMAP.md). This file is the to-do list for the
engine after v1.0.

Neo-Wolf is a **game engine**. The 10-level v1.0 campaign is its **demo**: it shows what the
engine does, and the engine's tests and the autoplay bot run on it. Games built on the engine
live in their own repositories and only supply content.

Work happens on branch `completion/v1.5`. Tick an item only once it has been verified (a test,
a CI run, or a recorded manual check).

## Done since v1.0

- [x] Configurable internal render resolution: default 640×360 (16:9), `--render-size WxH`, the same vertical field of view at every aspect ratio, UI scaled by whole pixels, gameplay unchanged (the bot's report stays identical) (commit `60e3e38`; CI run 43927263488, all seven jobs green)
- [x] Test Chamber: a development level outside the campaign, with an enemy gallery, pickups, keys and doors, props, a weapon range and a dark room; `campaign.json` `"bootIntoTestLevel"` starts the game in it (off in the shipped demo) ([levels/testchamber.md](levels/testchamber.md))
- [x] Enemy types and sprite slots defined by the game's data: `gameplay.json` `enemies` defines any number of types with their own sprites, and the engine no longer knows the demo's guard, brute and runner (commit `ca0c243`; the bot's report stays identical)

## Showcase preparation

- [x] The demo starts on the main menu again (`"bootIntoTestLevel": false`)
- [ ] README and docs present Neo-Wolf as an engine, with the v1.0 campaign as its demo
- [ ] A showcase build for macOS (and Windows), played through by hand
- [ ] Owner decision: the version number of the showcase build

## Engine/game boundary (paused for the showcase)

- [x] Enemy types and sprites come from the game's data (see above)
- [ ] A game manifest (`game.json`) supplies the content folder, window title and start-up settings
- [ ] No game-specific names or `game/...` paths in engine code
- [ ] The demo lives in its own folder (`games/sample/`), used by the engine's tests and the autoplay bot
- [ ] A CI check that fails when engine code refers to anything game-specific

## Feedback from the v1.0 play test

The owner's play test on macOS (2026-11-16) found sound and mechanics working. The changes it
asked for that belong to the engine and its demo:

- [ ] A minimap: it exists (Tab or M) but is off by default and not mentioned anywhere; make it on by default, add a setting and restyle it
- [ ] Shooting effects cover the whole screen (the muzzle flash is a world particle 0.4 cells in front of the camera): replace it with a flash on an on-screen weapon
- [ ] Dying enemies turn whitish (the sprite hit-flash stays on): remove it
- [ ] Vases look breakable but are not: breakable props
- [ ] A difficulty setting (Easy, Normal, Hard)
- [ ] More in-depth options: mouse sensitivity, key rebinding, separate music and sound volumes, difficulty, window size and fullscreen, render resolution, brightness, screen shake, head bob, crosshair, minimap default
- [ ] The mouse works in every menu and screen
- [ ] A controls screen (from the main menu and the pause menu) and tutorial hints in the first level
- [x] A test level the game opens straight into during development (the Test Chamber)

## Engine features (later)

- [ ] Animated sprites: a data-driven sprite registry with per-state frames (walk, attack, pain, death), per-sprite scale and an item glint
- [ ] First-person weapon view: bob, recoil, muzzle flash, reload and switching animations
- [ ] Multi-weapon system: weapon slots, switching (keys and mouse wheel), ammo types, magazines, reloads, pellet spread, piercing
- [ ] Damage types, projectiles, and enemy ranged attacks with a warning
- [ ] Deferred spawning, teleporting and scripted door actions
- [ ] Player status effects, screen overlays and ground hazards
- [ ] A runtime light layer
- [ ] A list of noise events per frame, and alerts raised by enemies
- [ ] Breakable props, drop tables and rare drops
- [ ] Boss framework: phases, a health bar, scripted events
- [ ] Audio mixer (known issue C23): overlapping sound effects, music and ambience streams, looping, positional sound, separate volumes
- [ ] An engine splash screen

## Version 2.0: procedural generation and game modes (ideas)

- [ ] Seeded random-number service: one seed reproduces a whole level or run, and the seed is shown so it can be shared
- [ ] Level generator module: room-and-corridor and BSP layouts for buildings, cellular automata for caves, selectable per theme
- [ ] Guaranteed solvable levels: key and lock placement from a dependency graph, a reachability check reusing the content validator's flood fill, an exit that is always reachable
- [ ] Population by budget: enemies, props, pickups and rare drops placed by difficulty and depth from data tables
- [ ] Prefab rooms authored by hand and stitched into generated layouts
- [ ] DevKit command to generate, preview and export levels from a seed (`gen-level --seed N`)
- [ ] Game modes defined as data (rules, win and lose conditions, HUD elements), mode selection in the main menu, separate saves per mode
- [ ] Versioned save format with migration from 1.x saves (the save format changes, which is why this is version 2.0; see [RELEASE.md](RELEASE.md))
- [ ] Event and trigger scripting: doors, lights, spawns, messages and objectives driven from level data

## Carried over from v1.0

- [ ] Phase 11: hands-on play-through on a real Windows PC
- [ ] Owner decision: publish a GitHub release for `v1.0.0` (release checklist step 7)
- [ ] Owner decision: merge `completion/v1` into `main`
- [ ] Owner decision: contribution policy
