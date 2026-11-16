# Neo-Wolf roadmap after v1.0

The v1.0 work is recorded in [ROADMAP.md](ROADMAP.md). This file is the to-do list for the next versions:
- **1.5.0** ships the horror game *Covenant of the Asylum* on the Neo-Wolf engine;
- **2.0** adds procedural generation and new game modes.

Neo-Wolf is the engine. The engine shows its splash screen at start-up, and each game is content built on it.

## Version 1.5.0: Covenant of the Asylum (in progress)

This is the master checklist for 1.5.0. Every item requested after the v1.0 play test, and every decision made since, has a box here; tick an item only once it has been verified (a test, a CI run, or a recorded manual check). Work happens on branch `completion/v1.5`.

### Decisions

- [x] Name and framing: the game is *Covenant of the Asylum*; Neo-Wolf is the engine and shows a splash screen at start-up
- [x] Version 1.5.0, on branch `completion/v1.5` from tag `v1.0.0`
- [x] Art direction: the current art, free asset packs and a generated 64×64 prototype were rendered in the engine at 640×360 and compared; the choice is generated art for all visuals, in one style
- [x] Roster: 12 enemy types (zombie, possessed nun and the ten listed under Enemies), 4 bosses and 7 weapons
- [x] Repository split: the engine stays here (public); the game gets its own private repository (see below)
- [x] Audio mixer (known issue C23): yes, needed for music and ambience
- [x] Mouse: for menus and the UI only, no mouse look
- [x] 2.0 scope: procedural generation, game-mode framework, roguelike, endless survival, full story campaign (see Version 2.0)
- [ ] Music direction: compare the licence-checked free tracks with procedurally generated ones per role (title, two ambiences, combat, boss, stingers), then choose

### Feedback from the v1.0 play test

The owner's play test on macOS (2026-11-16) found sound and mechanics working. Each change requested:

- [ ] Enemies are too weak: retune every enemy's stats and add a difficulty setting (Easy, Normal, Hard)
- [ ] The boss is too weak (v1.0 has no real boss): boss framework and four bosses
- [ ] A horror look instead of the military one: asylum, chapel, morgue and crypt themes; zombies, possessed nuns and other horror elements; darker lighting
- [ ] A minimap: it exists (Tab or M) but is off by default and not mentioned anywhere; make it on by default, add a setting and restyle it
- [ ] The UI looks machine-made: a hand-designed horror UI (pixel font, palette, textured panels)
- [ ] Vases look breakable but are not: breakable vases and other props
- [ ] Item drops from broken props, and rare item drops placed through each level
- [ ] A hand and gun on screen with a firing animation
- [ ] Shooting effects cover the whole screen (the muzzle flash is a world particle 0.4 cells in front of the camera): replace it with a flash on the on-screen weapon
- [ ] More enemy variety (12 types) and more weapon variety (7 weapons)
- [ ] Better enemy AI
- [ ] Enemy animations: walk, attack, pain and death
- [ ] Dying enemies turn whitish (the sprite hit-flash stays on): remove it
- [ ] More in-depth options: mouse sensitivity, key rebinding, separate music and sound volumes, difficulty, window size and fullscreen, render resolution, brightness, screen shake, head bob, crosshair, minimap default
- [ ] The mouse works in every menu and screen
- [ ] A controls screen (from the main menu and the pause menu) and tutorial hints in the first level
- [ ] A test level the game opens straight into during development (the Test Chamber)

### Engine

- [x] Configurable internal render resolution: default 640×360 (16:9), `--render-size WxH`, the same vertical field of view at every aspect ratio, UI scaled by whole pixels, gameplay unchanged (the bot's report stays identical) (commit `60e3e38`; CI run 43927263488, all seven jobs green)
- [ ] Test Chamber: a development level outside the campaign that grows with every feature (enemy gallery, pickups, keys and doors, props, weapon range, dark room)
- [ ] Engine/game boundary inside this repository:
  - [ ] engine built as a library target with public headers
  - [ ] a game manifest supplies the content folder, window title and start-up settings
  - [ ] no game-specific names or `game/...` paths in engine code (today: 12 hard-coded paths in 6 files; the guard/brute/runner names in 11 files; a fixed sprite-slot enum)
  - [ ] games live in their own folders: `games/sample/` (the v1.0 campaign, used by the engine's tests and the autoplay bot) and `games/covenant/`
  - [ ] a CI check that fails when engine code refers to anything game-specific
- [ ] Repository split, once the engine foundations are stable and before the ten levels are built:
  - [ ] the engine stays in this repository (public) with the sample game, its tests and the DevKit
  - [ ] *Covenant of the Asylum* moves to its own **private** repository, which pulls in the engine at a pinned tag
  - [ ] both repositories keep their full history (`git filter-repo`), and CI runs in each
- [ ] Engine foundations:
  - [ ] animated sprites (data-driven sprite registry with per-state frames)
  - [ ] data-driven enemy types (replacing the fixed guard, brute and runner)
  - [ ] damage types (ballistic, fire, holy, melee)
  - [ ] deferred spawning
  - [ ] projectiles
  - [ ] enemy ranged attacks with a warning
  - [ ] player status effects (bleeding, slowed)
  - [ ] screen overlays
  - [ ] ground hazards
  - [ ] runtime light layer
  - [ ] a list of noise events per frame, and alerts raised by enemies
  - [ ] teleporting
  - [ ] scripted door actions
- [ ] Audio mixer: overlapping sound effects, music and ambience streams, looping, positional sound, separate volumes

### Player and weapons

- [ ] First-person weapon view with hands: bob, recoil, muzzle flash, reload and switching animations
- [ ] Multi-weapon system: weapon slots, switching (keys and mouse wheel), ammo types, magazines, reloads, pellet spread, piercing
- [ ] Altar Candlestick, Service Revolver, Sexton's Coach Gun, Gatehouse Repeater, Votive Lamp, Sanctuary Arbalest, Reliquary of the Nameless Saint

### Enemies and bosses

- [ ] Zombie and possessed nun
- [ ] Bedcrawlers, Ironframe, Cantor, Lantern Watchman, Tollkeeper
- [ ] Embalmer, Snuffer, Sutured Pair, Shrine Bearer, Ember Prior
- [ ] Boss framework: phases, a health bar, scripted events
- [ ] Matron Halvard (level 3), The Anatomist (level 6), The Precentor and the Hollow Choir (level 8), The Abbess of the Covenant (level 10)

### Props and items

- [ ] Breakable props (vases, urns and others) with break animations and sounds
- [ ] Drop tables, rare drops and hidden rare items in each level
- [ ] New pickups for the new weapons and ammunition types

### Art, audio and levels

- [ ] Art generator in the DevKit (deterministic, standard library only):
  - [ ] wall, floor, ceiling and door textures for each theme
  - [ ] enemy and boss sprites with animation frames
  - [ ] weapon views
  - [ ] props and pickups
  - [ ] UI art and the icon
- [ ] Music, ambience and sound effects: free assets with recorded licences and credits, or generated by the DevKit
- [ ] A credits screen listing every third-party asset and its licence
- [ ] Ten horror levels in the new themes, with the bosses on levels 3, 6, 8 and 10

### UI

- [ ] Neo-Wolf engine splash screen
- [ ] *Covenant of the Asylum* title screen
- [ ] The new horror UI across the HUD, menus and overlays
- [ ] Options screen (see the play-test list), saved between sessions
- [ ] Controls screen and tutorial hints
- [ ] Mouse support in every menu
- [ ] Minimap: on by default, a setting, the new style

### Presentation and release

- [ ] README and docs present Neo-Wolf as an engine, with *Covenant of the Asylum* as the game built on it
- [ ] Autoplay bot, tests and CI updated for the new content and modes
- [ ] Packages for macOS and Windows; hands-on play test on both
- [ ] Release 1.5.0

### Carried over from v1.0

- [ ] Phase 11: hands-on play-through on a real Windows PC
- [ ] Owner decision: publish a GitHub release for `v1.0.0` (release checklist step 7)
- [ ] Owner decision: merge `completion/v1` into `main`
- [ ] Owner decision: contribution policy

## Version 2.0: procedural generation and game modes (to do)

### Engine: procedural generation

- [ ] Seeded random-number service: one seed reproduces a whole level or run, and the seed is shown so it can be shared
- [ ] Level generator module with room-and-corridor and BSP layouts for buildings and cellular automata for caves and crypts, selectable per theme
- [ ] Guaranteed solvable levels:
  - [ ] key and lock placement from a dependency graph
  - [ ] reachability check reusing the content validator's flood fill
  - [ ] exit always reachable
- [ ] Population by budget: enemies, props, pickups and rare drops placed by difficulty and depth, using data tables
- [ ] Theme and set-piece selection: prefab rooms authored by hand and stitched into generated layouts
- [ ] DevKit command to generate, preview and export levels from a seed (`gen-level --seed N`)
- [ ] Tests:
  - [ ] same seed gives the same level
  - [ ] every generated level passes validation
  - [ ] the autoplay bot finishes a large sample of seeds

### Engine: game-mode framework

- [ ] Game modes defined as data: rules, win and lose conditions, HUD elements, available content
- [ ] Mode selection in the main menu
- [ ] Separate saves per mode
- [ ] Versioned save format with migration from 1.x saves (the save format changes, which is why this is version 2.0; see [RELEASE.md](RELEASE.md))
- [ ] Event and trigger scripting: doors, lights, spawns, messages and objectives driven from level data

### Game: roguelike mode

- [ ] Runs made of generated floors of rising depth and difficulty; permadeath ends the run
- [ ] Random weapon, item and relic finds; a choice of perks or relics between floors
- [ ] Shops or shrines between floors, spending what was collected in the run
- [ ] Optional meta-progression: unlocks that carry over between runs
- [ ] Run summary screen with the seed, depth reached, kills and time; local best runs
- [ ] Daily seed: the same run for every player on a given day

### Game: endless survival mode

- [ ] Arena maps, hand-made or generated
- [ ] Waves that escalate: a spawn director chooses enemy mixes, numbers and elite variants
- [ ] Breaks between waves with pickups, a weapon unlock every few waves and occasional boss waves
- [ ] Score, wave reached and time survived, with a local high-score table

### Game: full story campaign

- [ ] Story and script: acts, characters and the cult's history, written as a design document first
- [ ] Storytelling in the world: notes, documents, audio logs and environmental details
- [ ] Intermissions and cutscenes: illustrated slides with text, voice or sound
- [ ] Dialogue and subtitle system
- [ ] Objectives, checkpoints and multiple save slots
- [ ] A longer campaign than 1.5's ten levels, with the 1.5 bosses as story beats
- [ ] Optional multiple endings
