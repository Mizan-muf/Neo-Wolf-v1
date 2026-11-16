# Level Authoring: adding level N

An end-to-end checklist for adding a campaign level (Roadmap Phase 8 uses it ten times). Each
step names the tool that checks it. A level is done when `validate --strict` passes, the autoplay
bot completes it on its own and in the whole campaign, it has been looked at, and it has its
level page.

Reference: tiles [MAP_FORMAT.md](MAP_FORMAT.md), entity fields
[CONTENT_DEFINITIONS.md](CONTENT_DEFINITIONS.md), rules of play [GAMEPLAY.md](GAMEPLAY.md),
DevKit commands [DEVKIT_GUIDE.md](DEVKIT_GUIDE.md), the bot [TESTING.md](TESTING.md#autoplay-bot).

Below, `levelNN` is the new level's id (for example `level04`) and `N` its number in the
campaign. `neo_devkit` stands for `./tools/devkit/neo_devkit` (Windows:
`tools\devkit\neo_devkit.cmd`).

## 1. Plan

- [ ] Theme (`bunker`, `stone`, `tech`, `classic`, or a new `game/defs/themes/<id>.cfg`), size,
      enemies, pickups, keys and locked doors, the new mechanic, the intended difficulty
      (Phase 8: the design table in `docs/levels/README.md`).
- [ ] Remember what the player arrives with: health and ammo **carry over** from level N-1
      (clamped to the maxima, health at least 1); keys do **not** carry over.

## 2. Create the stub

```bash
neo_devkit create-map --level-id levelNN --style dungeon --width 24 --height 16 \
    --register --name "Level Name" --theme stone
```

- [ ] This writes `game/maps/levelNN.map` (closed outer wall, an exit switch `X` in the east
      wall), `game/defs/levelNN.entities.json` (the player, and a door/trigger entity for every
      `D`/`T` of the template) and appends the level to `game/defs/campaign.json`. It refuses to
      overwrite existing files (`--force` to replace).
- [ ] The stub already passes `neo_devkit validate --strict`; check that before editing.
- [ ] The campaign order is the play order: move the new entry in `campaign.json` if it is not
      the last level. Ids must be unique and use `a-z 0-9 _ -`; names longer than 32 characters
      are shortened on screen.

## 3. Draw the map (`game/maps/levelNN.map`)

- [ ] Tiles: `.` floor, `#` `A` `B` walls (three textures), `D` door, `T` trigger, `X` exit switch.
      Nothing else: no spaces, no tabs, no lower case.
- [ ] Every row has the same width; the outer ring is solid (`# A B X`).
- [ ] Comments only as whole lines starting with `;` **in column 1**. No indented comments, no
      blank lines with spaces, no trailing spaces: the engine treats them as grid rows and the
      map fails to load (the DevKit reports exactly what the engine would).
- [ ] At least one `X`, in a wall, with a floor cell directly beside it (left, right, above or
      below; diagonal does not count). The player presses `E` there while facing it.
- [ ] Every `D` is a door between two walls (a door the player can walk round is legal but
      pointless). Every `D` needs a door entity (step 4), or it stays a wall forever.
- [ ] Corridors at least one cell wide; enemies (radius 0.25) and the player (0.22) fit.
- [ ] The file is saved with LF line endings (`.gitattributes` enforces LF for `game/**`); CRLF
      also loads, but keep the repository consistent.

## 4. Place the entities (`game/defs/levelNN.entities.json`)

Positions `x`/`y` are in cells; the centre of cell `(cx, cy)` is `(cx + 0.5, cy + 0.5)`; angle 0
looks to +x (right on the map), 90 to +y (down).

- [ ] **Player**: exactly one, on a floor cell (`.` or `T`), its body clear of walls: keep it at
      least 0.22 from any wall edge (the cell centre is always safe). Point `angle` into the level.
- [ ] **Doors**: one `door` entity per `D`, `"x": cx + 0.5, "y": cy + 0.5, "mapX": cx, "mapY": cy`.
      A door with `"lock": "red"` (`blue`, `gold`) needs that key card.
- [ ] **Keys**: `{ "type": "pickup", "kind": "key", "key": "red", ... }` on a floor cell that can
      be reached **before** the red door (not behind it, and not behind another lock that needs a
      key placed after it). Every lock colour needs a key in the same level.
- [ ] **Triggers**: one `trigger` entity per `T` (checkpoint message; it does not end the level).
- [ ] **Enemies**: `"variant": "guard"`, `"brute"` or `"runner"` on floor cells, at least 0.25
      from walls (otherwise they start wedged: a warning). Think about the carried-over health
      and ammo: a brute takes 4 shots, a guard 2, a runner 1 (weapon damage 35).
- [ ] **Pickups**: `"kind": "health"` or `"ammo"` (optional `"value"`), where the player can reach
      them; **decorations** anywhere on the floor (they block nothing).
- [ ] Nothing inside a wall, the exit switch or a door cell (only the door itself).
- [ ] Optional: `neo_devkit sync-entities --level levelNN` (or `--level N`) refreshes this
      level's CSV export (`levelNN.entities.csv`) and nothing else; without `--level` it
      rewrites every level's CSV. A level with light zones prints a `Warning:` line: expected,
      the CSV cannot hold them (the JSON, which the game loads, keeps them).

### Dark and dim areas (light zones)

A level darkens areas with the optional `lights` array of its entity JSON (schema and errors:
[CONTENT_DEFINITIONS.md](CONTENT_DEFINITIONS.md#light-zones-lights)):

```json
  "lights": [
    { "x": 3, "y": 12, "w": 7, "h": 6, "level": 0.5 },
    { "x": 5, "y": 14, "w": 3, "h": 2, "level": 0.35 }
  ]
```

| `level` | Use |
|---|---|
| 1.0 | Normal (every cell without a zone) |
| 0.7 | Slightly shaded: a side room, a mood change |
| 0.5 | **Dim**: a crypt, a store room |
| 0.35 | **Dark**: a blackout wing |
| 0.2 | The darkest allowed: small pockets only (a closet, a dead-end corridor) |

![The same room at light levels 1.0, 0.7, 0.4 and 0.2](images/light-levels.png)

How to draw them:

- [ ] Cover the **floor cells** of the area; the walls round it follow (a wall face takes the
      light of the cell in front of it). Including the surrounding walls does no harm.
- [ ] Zones apply in order, later ones win: one zone for the wing, then smaller, darker (or
      lighter) zones inside it.
- [ ] Edges are hard (no gradient). Put them at a doorway, a corridor mouth or a corner, where
      a change of light reads as a different place; for a soft edge, step down in two zones
      (e.g. a 0.5 cell row in front of a 0.35 room).
- [ ] Every zone lies fully inside the map, with `level` from 0.2 to 1.0, or the level does not
      load; `validate` reports it with the zone's index.

Fairness: **light changes only the picture.** Enemies see and hear the player in a dark zone
exactly as in the light, and the autoplay bot ignores light (a deliberate v1 simplification),
so the bot's result does not prove a dark area is fair; looking at it does. The renderer keeps
every sprite at least at 0.35 brightness, so enemies stay visible even at 0.2, but they are
harder to spot at range. Rules:

- [ ] Inside a dark zone (< 0.5) use guards and runners; no brute within 3 cells of the zone's
      entrance (it hits for 25 before the player has made it out).
- [ ] Mark every dark zone's entrance with a decoration pair, so the player knows it is
      deliberate and where it starts.
- [ ] Keep what the player must read in at least 0.5: the cell in front of a locked door (its
      colour), the exit switch `X` and its room, and the cell of a key card on the critical path.
      Pickups off the path may sit in the dark as a reward for exploring.
- [ ] No ambush at a dark doorway: the rule "no enemy within 1.5 cells of a door's far side"
      applies twice as strictly in the dark (use 3 cells).
- [ ] Look at it: copy the content to a scratch folder, move the player there so that it looks
      into the zone, and take a headless screenshot (headless runs show gameplay). Check that
      each enemy is visible against the background and that nothing on the critical path is
      lost. Never edit the shipped files for a screenshot.

```bash
rm -rf /tmp/levelNN-light && mkdir -p /tmp/levelNN-light && cp -R game /tmp/levelNN-light/game
# edit /tmp/levelNN-light/game/defs/levelNN.entities.json: the player's x, y and angle look into the zone
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/macos/neo_wolf --data /tmp/levelNN-light --level N \
    --frames 5 --pref-dir /tmp/levelNN-light/prefs --screenshot /tmp/levelNN-light/zone.bmp
sips -s format png /tmp/levelNN-light/zone.bmp --out /tmp/levelNN-light/zone.png   # 320x240
sips -z 480 640 /tmp/levelNN-light/zone.png --out /tmp/levelNN-light/zone-big.png   # easier to judge
```

## 5. Validate

```bash
neo_devkit validate --strict                 # the whole project, warnings fail
neo_devkit inspect-map --level levelNN       # the grid with the entities, reachability, key order, light zones
```

- [ ] `validate --strict` passes. It checks everything above: the map and entity files with the
      engine's own parsing, one player on floor, door/`D` and trigger/`T` pairing, nothing in
      walls, an exit reachable with every key collected before its lock, every pickup and enemy
      reachable, light zones (range, fields, inside the map), no orphan files, `gameplay.json`,
      `campaign.json`, the theme's textures. It does not judge whether a dark area is fair.
- [ ] `inspect-map --level levelNN` exits 0 and the key order is the intended one.
- [ ] When the campaign is complete: `neo_devkit validate --strict --require-levels 10`.

## 6. Prove it can be finished: the autoplay bot

```bash
cmake --build --preset macos                 # re-runs configure: campaign.json changed
build/macos/neo_wolf_autoplay --level N --json /tmp/levelNN.json
build/macos/neo_wolf_autoplay --all          # the whole campaign, with carry-over
ctest --preset macos -L autoplay             # autoplay.level.1 ... N and autoplay.campaign
```

- [ ] `--level N` reports `complete` (exit 0). Read the table: time, damage taken, health and
      ammo at the exit, kills, pickups, keys used, shots. The bot starts this run with the
      initial health and ammo from `gameplay.json`.
- [ ] `--all` completes every level and reports `Carry-over: OK`: level N is also beatable with
      what the bot carries in from level N-1.
- [ ] A new `autoplay.level.N` CTest test exists (it is generated from `campaign.json` at
      configure time) and passes.
- [ ] If the bot fails, its reason names the problem (`stuck: no route to an exit switch ...`,
      `died ...`, `timeout ...`). It is a straightforward player (it does not dodge, retreat or
      look for pickups it does not need: [TESTING.md](TESTING.md#autoplay-bot)), so a level that
      only an expert can finish shows up here. Make the level fair rather than tuning it to the
      bot, and record anything deliberate in the level page.
- [ ] Windows: the CI job runs `neo_wolf_autoplay --all --json` on every OS and uploads the
      report (`autoplay-report-<preset>`); check it after pushing.

### Balancing with `--trace`

The table says *how much* damage the bot took; the event trace says *where and from whom*.
`--trace FILE` writes one line per event (format: [TESTING.md](TESTING.md#event-trace---trace)):

```bash
build/macos/neo_wolf_autoplay --level N --trace /tmp/levelNN-trace.txt
build/macos/neo_wolf_autoplay --all --trace /tmp/campaign-trace.txt    # with carried-over health/ammo
grep hit-taken /tmp/levelNN-trace.txt          # every hit on the player: time, attacker, damage, where
grep -E 'hit-taken|pickup kind=health' /tmp/campaign-trace.txt | grep '^LN '   # damage vs healing on level N
grep -E 'idle->|->attack' /tmp/levelNN-trace.txt   # when and where enemies notice and engage
```

- A `hit-taken` line gives the attacker (`by=brute#12`, its cell `from=3,19`), the damage, the
  player's health after it and the player's position `pos=`: look the cells up with
  `neo_devkit inspect-map --level N`. Several hits from one spot point at an ambush corner or
  an enemy that reaches the player before the player can see it.
- `hp=` after the last hits plus the `pickup kind=health` lines show how close the level runs
  to the 40 % health detour (and to `died`); with `--all`, `level-start hp=` shows what the next
  level inherits.
- `enemy … idle->alert` far from the player's path (or never) shows enemies the route never
  meets; `hit-dealt` lines count the shots each kill needed.
- The trace is deterministic: change one thing, run again and `diff` the two traces to see
  exactly which encounters changed.

### Balancing tips (learned building the 10-level campaign)

The calibration table "What hurts the bot" in [docs/levels/README.md](levels/README.md#what-hurts-the-bot-measured)
gives the bot's damage per situation. What the Phase 8 builders and the integrator learned on
top of it:

- **Open sight lines cost the bot nothing.** It shoots everything it can see before it arrives
  (2 shots per guard, 4 per brute, 1 per runner). Damage comes from brutes met at close range,
  enemies converging from two directions, and enemies that step out beside or behind it.
- **Gunfire is heard 10 path cells along open cells; closed doors block it.** A squad behind an
  open doorway within 10 path cells of a fight hears it, walks out into the open and is shot at
  range (level 8's bays first had open doorways: 0 damage). To keep a squad for a close fight,
  put it behind a closed (unlocked) door, or more than 10 path cells from the earlier fights
  (level 9's key-room brute sits behind a winding 12-cell passage).
- **Alcoves:** a straight 1-cell alcove lets the bot see and shoot its occupant first; an
  L-shaped niche hides it until it steps out beside the player (level 5's gallery).
- **Two enemies in one place arrive one after the other; two from different directions arrive
  together.** Two guards round the same bend are both shot (0); one round the bend and one in a
  side store across it cost 12–24 (level 4's depot). Guards spread to both sides of a door's far
  side make a pincer (61 in an early level 6); a line across the room 3 cells in costs about 12.
- **A brute 1.2 cells behind a door costs exactly one brute hit (25)**; at 1.6 cells or more, 0.
- **Pickups on the critical path must be in a 1-wide corridor or a doorway approach.** The bot
  does not reliably collect a pickup in a 2-wide corridor: it may walk the other half (level 10's
  gate ammo). The bot fetches health only at 40 % or less and ammo at 6 or less.
- **Damage comes in steps** (guard 12, brute 25, runner 6), so a band floor above 0 on a
  guards-only level rests on one hit landing. Check that it is not luck (below).

### Is the result luck? A robustness check

Runs are deterministic per build, but a hit that lands by a few hundredths of a second may not
land on another compiler or CPU (known issue V3). Before trusting a number near a band edge,
move each enemy by up to ±0.15 cells in a scratch copy of the content (keep them 0.25 or more
from walls) and run `--level N --data <copy>` a few times: the Phase 8 campaign was checked with
8 seeded variants per level (results in each level page). If the damage leaves the band in some
variants, nudge the layout until it does not (level 4's store guard moved one cell), or widen the
band with a reason in the README.

## 7. Look at it

```bash
mkdir -p /tmp/levelNN
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/macos/neo_wolf --level N --frames 5 \
    --pref-dir /tmp/levelNN/prefs --screenshot /tmp/levelNN/start.bmp
```

- [ ] The start screenshot shows the intended view (textures of the theme, nothing missing as a
      checkerboard, the level banner). Headless runs never pause on focus loss (the `dummy` and
      `offscreen` video drivers ignore focus events), so the screenshot shows gameplay.
- [ ] Play it by hand in a window (`build/macos/neo_wolf --level N`): the minimap (`M`) shows the
      layout, doors open, keys work, the exit switch reads `EXIT`.

## 8. Document it

- [ ] `docs/levels/levelNN.md` (Phase 8), with the same sections as the other level pages:
      a header table (id, name, theme, size, difficulty, human time, new mechanic), the start
      screenshot, **Idea**, **Map** (the shipped `.map` file byte for byte, then the annotated
      grid), **Areas**, **Enemies**, **Pickups**, **Keys, doors and triggers**, **Landmarks
      (decorations)**, **Light zones**, **Walkthrough (critical path)**, **Bot results** (fresh
      and campaign rows against the README targets, where the bot takes its damage from
      `--trace`, the robustness check) and **Deviations and exceptions** (anything that breaks a
      design rule on purpose, and why).
- [ ] Generate the annotated grid and check both map blocks:

```bash
python3 -m tools.devkit.level_docs --write   # rewrite the annotated grids from the entity files
python3 -m tools.devkit.level_docs           # check: exit 1 if a page's map differs from its .map file
```

  DevKit test `test_level_docs` runs the check on every campaign level, so a map edit without
  the matching page edit fails `ctest`. The shipped-map block is never rewritten for you: copy
  the `.map` file in by hand, so a changed map is also a visible change of the page.
- [ ] The design table and the measured table in `docs/levels/README.md`, and the campaign
      table in [GAMEPLAY.md](GAMEPLAY.md#the-campaign).

## 9. Before committing

- [ ] `ctest --preset macos` is green (`content_validation`, `devkit_tests`, `autoplay.*`,
      `game_smoke_level_N`).
- [ ] New files are added: the map, the entity JSON (and CSV export), `campaign.json`, the level
      page.
