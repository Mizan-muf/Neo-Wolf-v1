# Operation Neo-Wolf: campaign design (Roadmap Phase 8)

The design of the 10-level campaign: what each level is, how hard it is, how much health and
ammo it hands out, and the rules every level follows. Each level has its own page,
[level01.md](level01.md) to [level10.md](level10.md): map, entities, walkthrough, the bot's
measured results and any justified exception to the rules below.

**Status (2026-11-16):** all 10 levels are built and integrated (Roadmap Phase 8). Every level
passes `validate --strict --require-levels 10`, and the autoplay bot completes each one on its
own and the whole campaign with carry-over, inside every target below
([Measured results](#measured-results-phase-8-integration)). Builder A built levels 1–5,
builder B levels 6–10, from the per-level briefs now kept as history in
[BUILD_NOTES.md](BUILD_NOTES.md).

The [Test Chamber](testchamber.md) has a page here too, but it is not part of the campaign: it
is the development level the game starts in during 1.5, and none of the rules and targets below
apply to it.

References: tiles [MAP_FORMAT.md](../MAP_FORMAT.md), entity fields
[CONTENT_DEFINITIONS.md](../CONTENT_DEFINITIONS.md), rules of play [GAMEPLAY.md](../GAMEPLAY.md),
the step-by-step checklist [LEVEL_AUTHORING.md](../LEVEL_AUTHORING.md), the bot
[TESTING.md](../TESTING.md#autoplay-bot).

## The campaign at a glance

Title: **Operation Neo-Wolf** (unchanged). The player breaks into an enemy complex: through the
outer bunkers, down into the old cellars and catacombs under it, through the tech wings and the
reactor, and finally into the castle at its heart, the Wolf's Den.

Enemies are `G/B/R` = guard / brute / runner. Pickups are `H/A` = health / ammo pickups at
the default values (+25 HP, +12 ammo). A key "→" a door means the key opens that locked door.
Sizes are the design grid size in cells (width × height); a built level may differ by ±2 cells
(built: level 1 20×13, level 2 24×14, level 3 26×17, level 8 34×34; the others as listed).

| # | Id | Name | Theme | Size | Enemies G/B/R | Pickups H/A | Keys and locks | New mechanic or idea | Difficulty (1–10) | Human time |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `level01` | Outpost Gate | `bunker` | 18×12 | 2/0/0 | 1/1 | none | Tutorial: move and turn, walk-over pickups, shoot a guard at range, open a door (`E`), use the exit switch | 1 | 1–2 min |
| 2 | `level02` | Old Cellars | `stone` | 22×14 | 4/0/0 | 1/1 | blue key → 1 blue door | Key cards and locked doors: meet the locked door first (`NEED BLUE KEY`), then fetch the key from a side room | 2 | 2–3 min |
| 3 | `level03` | Server Annex | `tech` | 24×18 | 6/0/0 | 2/2 | red key → 1 red door | Gunfire is heard (closed doors block it): a ring corridor round a closed server room; a checkpoint trigger | 3 | 3–4 min |
| 4 | `level04` | Motor Pool | `bunker` | 26×20 | 6/1/0 | 2/2 | gold key → 1 gold door | The brute: first seen across an open garage, so the player learns it takes 4 shots while it lumbers in | 4 | 4–5 min |
| 5 | `level05` | Catacombs | `stone` | 28×24 | 6/1/4 | 3/3 | red key → red door; blue key (behind red) → blue door | Runners; two keys in sequence; a branching route (two ways to the red key); a dim crypt (light zone, see [Light zones](#light-zones)) | 5 | 5–7 min |
| 6 | `level06` | Relay Station | `tech` | 30×26 | 8/2/4 | 3/4 | blue key → blue door; gold key (behind blue) → gold door | Ambush rooms behind closed doors; an optional maintenance loop with extra supplies | 6 | 6–8 min |
| 7 | `level07` | Blackout Barracks | `bunker` | 32×28 | 8/2/6 | 4/4 | red and gold keys, either order; the exit wing is behind a red door, then a gold door | Dark wings (light zones); two keys collected in parallel | 7 | 7–9 min |
| 8 | `level08` | Reactor Spine | `tech` | 34×32 | 10/3/6 | 4/5 | blue → red → gold, a chain (each key behind the previous door) | Three keys; mixed squads (brute with guards, runner pairs) along a long spine with side bays | 8 | 8–11 min |
| 9 | `level09` | Sunken Archive | `stone` | 38×36 | 12/4/8 | 5/6 | red and blue keys, either order; gold key behind the blue door; exit behind the red door, then the gold door | A non-linear hub with three wings, crossed several times; an optional sealed vault in the hub (a squad and a supply cache) | 9 | 10–13 min |
| 10 | `level10` | Wolf's Den | `classic` | 40×40 | 12/5/8 | 6/7 | red → blue → gold, a chain | Finale: a gauntlet of the earlier ideas, a supply room with a checkpoint, then a final arena with a brute squad in front of the exit | 10 | 12–15 min |
| | | **Total** | | | **74/18/36 = 128** | **31/35** | | | | **≈ 60–80 min** |

Themes: `bunker` 3 levels (1, 4, 7), `stone` 3 (2, 5, 9), `tech` 3 (3, 6, 8), `classic` once,
for the finale, so the last level looks unlike the rest.

### The arc

- **Levels 1–3: learn.** One new idea per level, introduced safely before it is tested:
  movement, pickups, shooting and doors (1); keys and locked doors (2); hearing, loops and
  triggers (3). Only guards, and no enemy attacks the player by surprise at close range:
  - an enemy behind a closed door stands **at least 2 cells** from the door's far-side cell
    (rule 9 asks 1.5 of every level) and is in view from the doorway once the door is open;
  - nothing waits round a blind corner of the critical path to meet the player within 2 cells,
  - **except level 3's hearing lesson:** at most one guard may wait out of sight in a side room
    *without a door* off the ring and come round its corner when gunfire wakes it. It must be
    the level's designed lesson (the shot at one guard wakes the other, which comes from another
    direction), cost the bot at most one hit, and be listed in the level page. (Integration
    review, Phase 8: level 3's closet guard is exactly this; its two server-room guards stand
    2.24 cells behind the closed door Ds, which the first point allows.)
- **Levels 4–7: widen.** Each adds an enemy type or a structural idea: the brute (4), runners,
  two keys and a branch (5), ambush rooms (6), dark areas and keys in parallel (7). Mixed
  groups of 2–3 (squads that fight together) appear from level 5; earlier levels may place
  two guards in one room.
- **Levels 8–10: combine.** Large three-key levels with mixed squads; level 9 is non-linear,
  level 10 is a finale with a supply room and a final arena.

## What the levels are built from

| Mechanic | Rule that matters for design | Source |
|---|---|---|
| Player | 100 HP (max 100), 24 ammo at New Game (max 99), 3.0 cells/s, radius 0.22 | `gameplay.json` |
| Weapon | 35 damage, one shot every 0.25 s, hitscan up to 12 cells; every shot is a **noise** heard 10 cells along open cells | `gameplay.json`, [GAMEPLAY.md](../GAMEPLAY.md#enemy-ai) |
| Guard | 50 HP = **2 shots**; 12 damage per 1.0 s; 1.6 cells/s; notices at 6.5 cells; reacts after 0.5 s | `enemies.guard` |
| Brute | 140 HP = **4 shots**; 25 damage per 1.6 s; 1.0 cells/s; notices at 5.5; reacts after 0.8 s | `enemies.brute` |
| Runner | 25 HP = **1 shot**; 6 damage per 0.6 s; 2.8 cells/s; notices at 8.0; reacts after 0.2 s | `enemies.runner` |
| All enemies | Melee only (attack within 1.0–1.3 cells), see all round, never open doors, closed doors block sight and sound (C16) | [GAMEPLAY.md](../GAMEPLAY.md#enemy-ai) |
| Pickups | +25 HP / +12 ammo by default, collected by walking within 0.6 cells (no key press; also when full: the surplus is lost) | `pickups.*` |
| Keys | Red, blue, gold. Reset on every level load and on death; never carried over | [GAMEPLAY.md](../GAMEPLAY.md#keys-and-locked-doors) |
| Doors | `E` within 1.05 cells toggles **every** door in reach (C19); doors open instantly; a door does not close on a living body | [MAP_FORMAT.md](../MAP_FORMAT.md) |
| Exit | `X` wall tile, `E` within 1.1 cells while facing it | [MAP_FORMAT.md](../MAP_FORMAT.md) |
| Triggers | `T` + `trigger` entity: `CHECKPOINT REACHED` message only (no respawn point: death restarts the level) | [MAP_FORMAT.md](../MAP_FORMAT.md) |
| Light | Per-cell light zones in the entity JSON (`"lights"`), 0.2–1.0; cosmetic only (see [Light zones](#light-zones)) | `engine/world/Map.h`, `engine/world/LevelBuilder.cpp` |
| Decorations | One sprite only (the blue vase); non-blocking; not shown on the minimap | `sprites.cfg` |

### What hurts the bot (measured)

The autoplay bot is an excellent shot and a poor tactician (V1): it fires only when the shot
will hit (shots used = exactly 2 per guard, 4 per brute, 1 per runner) but never retreats or
dodges. It only takes damage when enemies reach melee range. Measured on macOS with this build
(2026-11-16; scratch levels in `/tmp`, not committed, so this table cannot be re-checked from the
repository; runs are deterministic per build):

| Situation (fresh start) | Bot damage |
|---|---|
| A guard, a brute, 4 runners, 3 guards, or a 5-enemy mixed squad, all seen across an open corridor or room | **0** |
| A guard round a corner, or 1.1 cells behind a door the bot opens | 0 |
| A brute 1.6–4.5 cells behind a door, or 2 guards 3 cells behind it | 0 |
| A brute round a corner (meets it within ~1.5 cells) | 25 |
| A brute 1.1 cells behind a door (in reach the moment the door opens) | 25 |
| 2 brutes at a corner, or 2 brutes 1.2 cells behind a door | 50 |
| 4 guards packed 1.1–2.5 cells behind a door | 12 |
| A brute and 3 runners behind a door | 25 |
| 2 guards + 2 brutes + 2 runners behind a door | 49 |
| "Pincer": 4 guards and 2 runners in alcoves along a 1-wide corridor, waking on gunfire | 30 |

So, for builders: **open sight lines cost the bot nothing; brutes met at close range,
enemies converging from several directions at once, and tight spaces do.** A level that should
cost the bot 30–60 HP needs 2–3 of those moments, not more enemies in open rooms. A person
takes more damage than the bot from the same layout (slower aim, misses), but can retreat.

## Resource economy

Health and ammo **carry over** between levels (clamped to 100 / 99); keys do not. A death
restarts the level with what the player had on entering it, so a level must stay winnable from
a weak entry state too (see the bot targets below, which test exactly that).

**Ammo.** The threat of a level in shots is `T = 2·guards + 4·brutes + 1·runner` (a perfect
shot). Each level supplies about **1.5 × T** in ammo pickups (12 each), rounded up, so a person
who misses one shot in three breaks even and a good player banks a surplus for later.

**Health.** Each level supplies about **1.5 × the upper end of its bot damage target**
(between 1.25× and 2.1×), in whole pickups of 25 HP and at least one per level: a person takes
more hits than the bot from the same layout but can back off, and carried-over health covers
the rest.

| # | T (shots) | Ammo pickups (ammo) | Supply / T | Running T | Running supply (incl. 24 at start) | Health pickups (HP) | Bot damage target (upper) |
|---|---|---|---|---|---|---|---|
| 1 | 4 | 1 (12) | 3.0 | 4 | 36 | 1 (25) | 12 |
| 2 | 8 | 1 (12) | 1.5 | 12 | 48 | 1 (25) | 20 |
| 3 | 12 | 2 (24) | 2.0 | 24 | 72 | 2 (50) | 30 |
| 4 | 16 | 2 (24) | 1.5 | 40 | 96 | 2 (50) | 35 |
| 5 | 20 | 3 (36) | 1.8 | 60 | 132 | 3 (75) | 45 |
| 6 | 28 | 4 (48) | 1.7 | 88 | 180 | 3 (75) | 55 |
| 7 | 30 | 4 (48) | 1.6 | 118 | 228 | 4 (100) | 60 |
| 8 | 38 | 5 (60) | 1.6 | 156 | 288 | 4 (100) | 70 |
| 9 | 48 | 6 (72) | 1.5 | 204 | 360 | 5 (125) | 80 |
| 10 | 52 | 7 (84) | 1.6 | 256 | 444 | 6 (150) | 95 |

Over the campaign: 256 shots of threat against 444 rounds supplied (1.73×), and 775 HP of
pickups. A player who arrives at level 10 with the 99-ammo cap loses the surplus, which is fine:
the finale's own supply covers its threat.

**Placement rules that make the economy work** (for the bot too, which picks up only what it
walks over or what it fetches when low: health ≤ 40, ammo ≤ 6):

1. **At least half the ammo pickups (rounded up) lie on the critical path**: in a 1-cell-wide
   corridor or doorway approach the shortest route crosses, so anyone walking it collects them.
2. **The first ammo pickup on the critical path comes before the first fight that needs more
   than 8 shots.** The bot does not fight with 0 ammo, and a person arriving with little ammo
   must not be stuck.
3. **Health on the critical path sits just after a fight**, where damage is near-certain, never
   at the very start (it would be wasted at full health). The rest of the health goes in side
   rooms or alcoves within 6 cells of the route.
4. **One health pickup within 6 cells before every designated ambush or arena door.**
5. Pickups may carry a custom `value` only for a deliberate reward (e.g. a 50-HP "medkit"; no
   shipped level uses one); record it in the level page and count it in the table above.

## Bot difficulty targets

Two runs, both with `neo_wolf_autoplay` (time limit 300 s per level):

- **Fresh start**: `--level N`, starting with 100 HP and 24 ammo. Tests that each level is fair on
  its own.
- **Campaign**: `--all`, health and ammo carried over. Tests the curve.

The integrator balances against these numbers. A value outside its range is a finding to fix in
the layout (move an enemy, a pickup, a door), not by tuning the bot.

| # | Fresh: damage taken | Fresh: HP at exit ≥ | Fresh: kills ≥ | Fresh: bot time ≤ | Campaign: HP at exit ≥ | Campaign: ammo at exit ≥ |
|---|---|---|---|---|---|---|
| 1 | 0–12 | 88 | 1 of 2 | 45 s | 85 | 12 |
| 2 | 0–20 | 80 | 2 of 4 | 60 s | 70 | 12 |
| 3 | 5–30 | 70 | 3 of 6 | 75 s | 60 | 12 |
| 4 | 5–35 | 65 | 4 of 7 | 90 s | 50 | 12 |
| 5 | 10–45 | 55 | 6 of 11 | 100 s | 45 | 12 |
| 6 | 20–55 | 45 | 7 of 14 | 110 s | 40 | 12 |
| 7 | 25–60 | 40 | 8 of 16 | 120 s | 35 | 12 |
| 8 | 30–70 | 35 | 10 of 19 | 140 s | 30 | 15 |
| 9 | 35–80 | 30 | 12 of 24 | 160 s | 30 | 20 |
| 10 | 45–95 | 25 | 15 of 25 | 180 s | 25 | – |

Notes on the targets:

- "Damage taken" counts every hit, so it can exceed the HP lost when the bot heals in between
  (it fetches health at ≤ 40 HP). It must never reach 100 before the first health pickup the
  bot can reach.
- For levels 8–10 the "HP at exit" floor is deliberately higher than 100 minus the damage
  ceiling: those levels must hand health back **after** their big fights (economy rule 3), so
  the player leaves with something to carry into the next level.
- "Kills ≥" checks that enemies sit on or near the route; the bot ignores enemies it cannot
  see, so it never needs to kill everyone.
- A **lower bound above 0** (levels 3–10) means the level must actually threaten: if the bot
  walks through unhurt, the level is too easy for its slot in the curve.
- Levels 1 and 2 use only guards and no close-range moments, so a bot damage of 0 there is
  fine as long as it stays below the upper bound. Level 3's floor of 5 is its one designed hit
  (the hearing lesson, see [The arc](#the-arc)).
- Every result must also hold for the whole campaign: `--all` reports `Carry-over: OK` and
  `Result: PASS (10 of 10 level(s) completed, campaign complete)`.
- Human time is the intended first-play time for a person who explores; it is not measured
  automatically. Bot time is game time from the report.

## Measured results (Phase 8 integration)

Measured 2026-11-16 by the integrator on macOS with `neo_wolf_autoplay` (fixed 1/60 s steps,
300 s limit per level). **Every value is inside its target.** The JSON reports are
byte-identical for the arm64 and x86_64 (Rosetta) slices of the universal `build/macos` binary
and for the `ci-macos` (`-Werror`) build, both slices. On Windows MSVC x64
(CI run 43861234139) every level's frames, damage, HP and ammo are identical to
macOS; the Linux CI job's results are identical too (CI runs 43861234139 and 43866272111; known issue V2).

| # | Level | Fresh: time s | Fresh: damage (target) | Fresh: HP at exit | Fresh: kills | Campaign: start HP / ammo | Campaign: time s | Campaign: damage | Campaign: HP at exit | Campaign: ammo at exit | Jitter: damage range |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | [`level01`](level01.md) | 8.6 | **0** (0–12) | 100 | 2/2 | 100 / 24 | 8.6 | 0 | 100 | 32 | 0–12 |
| 2 | [`level02`](level02.md) | 22.6 | **0** (0–20) | 100 | 4/4 | 100 / 32 | 22.6 | 0 | 100 | 36 | 0 |
| 3 | [`level03`](level03.md) | 30.3 | **12** (5–30) | 100 | 6/6 | 100 / 36 | 30.3 | 12 | 100 | 36 | 12 |
| 4 | [`level04`](level04.md) | 34.5 | **24** (5–35) | 100 | 7/7 | 100 / 36 | 34.5 | 24 | 100 | 32 | 12–24 |
| 5 | [`level05`](level05.md) | 50.3 | **24** (10–45) | 100 | 10/11 | 100 / 32 | 50.3 | 24 | 100 | 37 | 12–24 |
| 6 | [`level06`](level06.md) | 52.9 | **37** (20–55) | 88 | 13/14 | 100 / 37 | 52.9 | 37 | 88 | 46 | 24–49 |
| 7 | [`level07`](level07.md) | 70.2 | **49** (25–60) | 100 | 12/16 | 88 / 46 | 70.2 | 49 | 100 | 58 | 43–55 |
| 8 | [`level08`](level08.md) | 64.8 | **43** (30–70) | 82 | 15/19 | 100 / 58 | 64.8 | 43 | 82 | 62 | 31–62 |
| 9 | [`level09`](level09.md) | 106.0 | **62** (35–80) | 75 | 21/24 | 82 / 62 | 106.0 | 62 | 70 | 65 | 62–86 |
| 10 | [`level10`](level10.md) | 95.1 | **81** (45–95) | 75 | 23/25 | 70 / 65 | 100.0 | 81 | 75 | 64 | 56–81 |

Campaign: `Carry-over: OK`, `Result: PASS (10 of 10 level(s) completed, campaign complete)`,
540 s of game time in all (535 s for the ten fresh runs). Per-level details, and
where each hit comes from (from `--trace`), are in the level pages.

"Jitter" is a robustness check, not a target: every enemy of the level moved by up to ±0.15
cells in 8 seeded scratch copies (the seeds were not recorded, so the jitter ranges on this page
and in the level pages cannot be reproduced exactly; unverified), so that a result that only holds by a few hundredths of a
second shows up (how: [LEVEL_AUTHORING.md](../LEVEL_AUTHORING.md#is-the-result-luck-a-robustness-check)).
All 80 variants complete.

### Balance decisions (Phase 8 integration)

- **Levels 3 and 4 sat one 12-point hit above their floor of 5.** A guard hit is 12, so any
  damage above 0 is at least 12; the question is whether the hit is luck.
  - Level 3: robust. The closet guard's hit landed in all 8 jitter variants and in all 12
    variants moving only the two ring guards by up to 0.2 cells. Band and layout kept.
  - Level 4: fragile. Moving either fuel-depot guard by up to 0.2 cells gave 0 in 2 of 18
    variants. **Content nudged:** the fuel-store guard moved one cell, (23, 2) → (23, 3); the
    same 18 variants now give 12–24, the 8 full-jitter variants 12–24, the shipped run 24.
    Band kept (5–35).
- **Level 10's arena does not depend on squad timing:** moving each arena enemy alone by up to
  0.2 cells (36 variants) gave 81 every time; full jitter 56–81. Band kept (45–95).
- **Level 9** keeps its band (35–80) and layout: the shipped run (62) has 18 points of margin;
  one of 8 full-jitter variants reached 86, when both map-room entrance guards and the guard of catalogue vault V3
  reach the bot. Watch it if level 9 changes.
- **Level 8**'s lowest jitter variant (31) is one point above its floor (30); the shipped run
  is 43. Kept.
- **A bot stall found by the jitter check, fixed in the bot:** one level 10 variant timed out
  (300 s), the bot standing in the great hall aiming at an idle runner whose centre it could see
  but whose shot grazed the alcove's corner. The bot now gives up on such an enemy after 2 s
  and walks on for 3 s (`engine/autoplay/AutoplayBot.cpp`, CTest `autoplay.fixture_aim_blocked`).
  Levels 1–9 are byte-identical with the fix; the shipped level 10 also had one such wait and
  now finishes about 5–7 s sooner with the same damage, kills and health (measured once at the
  time; the bot without the fix is not kept, so this is not re-checkable).

## Design rules (every level)

**Validator and engine** (checked by `validate --strict`; must pass with 0 warnings):

1. Map: rows of equal width; only `. # A B D T X`; a solid outer ring; LF; comments only as
   whole lines starting with `;` in column 1. The first line of every map is a `;` comment with
   the level id and name.
2. Exactly one player, at a cell centre (≥ 0.22 from walls), facing into the level.
3. One `door` entity per `D`, one `trigger` entity per `T`; nothing else inside solid cells.
4. Enemies at cell centres (≥ 0.25 from walls; known issue C20: a body that starts in a wall
   cannot slide out).
5. Every lock colour has a key of that colour in the level, reachable before its door; the exit
   switch is reachable with the keys collected in order; every pickup and enemy is reachable.
6. `inspect-map --level N` exits 0 and lists the keys in the intended order.
7. The CSV export is up to date: `neo_devkit sync-entities` after every edit of the JSON
   (`native.entity_placements_json_csv` checks every shipped level).

**Layout and fairness:**

8. **Readable layout.** Rooms have a purpose and a shape; corridors are 1–2 cells wide and
   lead somewhere; use `A` and `B` wall variants to give each area its own colour band (e.g. `B`
   round the exit, `A` round key rooms) so the minimap and the 3D view both read.
9. **No unfair traps.** There are no one-way doors in this engine, but these count as traps and
   are not allowed:
   - an enemy whose start cell is within **1.5 cells of a door's far-side cell** (it hits the
     player the moment the door opens: 25 HP for a brute), except in a designated, signposted
     ambush room (levels 6+, at most one such enemy per door);
   - an enemy that can reach the spawn within 3 s of the level starting (no enemy within
     8 cells of the spawn with a clear path, nothing in sight of the spawn on levels 1–4);
   - a brute met round a blind corner in a 1-wide corridor before level 6;
   - a fight that needs more ammo than the critical path has supplied up to that point.
10. **Doors are not too close together** (known issue C19, `E` toggles every door within 1.05
    cells): no two door cells may be adjacent (diagonals included) or exactly two cells apart in
    a straight line (`D.D`). Any other spacing is fine.
11. **Exits are visible or signposted.** The exit switch `X` is in the last room of the
    critical path and can be seen from that room's entrance; the room is framed with `B` walls
    and marked by a decoration landmark (below). If the exit is behind a locked door, the door
    is visible from the hub or main corridor so the player knows where the key leads.
12. **Keys before locks on a sensible route.** The player normally meets a locked door before
    its key (so `NEED <COLOUR> KEY` tells them what to look for), and the key's room is
    reachable without walking past the locked door more than once. A key is never inside the
    room it unlocks, never in a dead end more than 15 cells off the route without a pickup to
    reward the trip, and never guarded only by a brute before level 6.
13. **Decorations as landmarks.** There is one decoration sprite; use it in patterns that read
    from a distance:
    - **pair**: two vases flanking a doorway = "important door" (key room, locked door, ambush
      room, arena);
    - **line**: three vases in a row along a wall = "this way to the exit";
    - **cluster**: four vases in a square = a hub or a room to remember (the spawn, the supply
      room).
    Every level uses at least one pair and one line; decorations never stand in a 1-wide
    corridor (they are not solid, but they hide pickups behind them).
14. **One idea at a time.** The level's new mechanic is first shown safely (seen across a room,
    or with nothing else going on), then tested once or twice, then combined with old ones.
15. **Size and pace.** Stay within ±2 cells of the table's size. The critical path should take
    the bot no longer than the time target; side areas add the rest of the human time.
16. **Triggers** (`T`) are used only as checkpoint messages at a level's halfway point or in
    front of an arena; they do not save anything. Levels 3, 6, 8 and 10 have one each.

## Light zones

The design asks for dark or dim areas in levels 5, 7 and 10. Since known issue C21 was resolved
(Phase 8), a level's entity JSON sets them with an optional top-level `"lights"` array of cell
rectangles, `{ "x": 3, "y": 4, "w": 6, "h": 5, "level": 0.5 }`, applied in order (later zones
override), `level` from 0.2 to 1.0, every other cell 1.0. Schema:
[CONTENT_DEFINITIONS.md](../CONTENT_DEFINITIONS.md#light-zones-lights); how to draw them and the
fairness rules: [LEVEL_AUTHORING.md](../LEVEL_AUTHORING.md#dark-and-dim-areas-light-zones). The
zones as built, with screenshots: [level05.md](level05.md#light-zones) (the dim crypt, a dark
alcove), [level07.md](level07.md#light-zones) (two dark wings with soft edges) and
[level10.md](level10.md#light-zones) (the dim chapel, two dark alcoves).

- Levels: 0.5 = dim, 0.35 = dark; 0.2 is the floor (the renderer still draws sprites at 0.35 or
  brighter, so enemies stay visible). Design the area so it is also fair at full light.
- List each zone in the level page (`docs/levels/levelNN.md`) as its rectangle and level.
- The bot and the enemy AI ignore light (a deliberate v1 simplification), so light zones change
  human difficulty only: inside a dark zone use guards and runners, not brutes within 3 cells
  of the entrance, and mark every dark zone's entrance with a decoration pair.

## Known issues that shape the design

| # | Effect on the levels |
|---|---|
| C16 | Melee-only enemies that see all round and never open doors: closed doors make safe rooms and sealed ambushes; damage comes from corners, doorways and converging groups, not from range. |
| C19 | One `E` toggles every door within 1.05 cells: rule 10 keeps doors apart. |
| C20 | Bodies placed into walls get stuck: rules 2 and 4 (cell centres). |
| C21 | Resolved: light zones are authored in the entity JSON ([Light zones](#light-zones)); cosmetic only (the AI and the bot ignore light). |
| V1 | The bot does not dodge or retreat and fetches pickups only when low: rules 1–4 of the economy, and the measured table above. |
| V3 | Runs are deterministic per build but may differ across compilers and CPUs: check results near a band edge with the jitter check ([Balance decisions](#balance-decisions-phase-8-integration)). |

## Process

Phase 8 is built and integrated; this is how the campaign changes from now on.

1. Change a level with the checklist in [LEVEL_AUTHORING.md](../LEVEL_AUTHORING.md): the map,
   the entity JSON (then `neo_devkit sync-entities --level N` for its CSV), the level page
   (the shipped map and the annotated grid: `python3 -m tools.devkit.level_docs --write`; DevKit
   test `test_level_docs` checks both), and the measured table above.
2. `campaign.json` holds the ids, names, themes and order; renaming or re-theming a level is a
   design change to this page first.
3. The tests do not depend on the shipped levels' details: engine tests that need exact
   positions use the frozen Phase 7 levels in `tests/fixtures/reference/`, and the campaign-wide
   tests read `campaign.json` (`autoplay.level.N` per level, `autoplay.campaign`). A level edit
   should not need a C++ test edit; one exception is `autoplay.trace`, which expects level 3 to
   contain a key, a locked door and kills.
4. Re-measure against the targets (fresh, `--all`, and the jitter check for any value within a
   hit of a band edge) and update the level page and [Measured results](#measured-results-phase-8-integration).
5. History: the builders' per-level briefs are in [BUILD_NOTES.md](BUILD_NOTES.md) (frozen);
   their durable lessons are in [LEVEL_AUTHORING.md](../LEVEL_AUTHORING.md#balancing-tips-learned-building-the-10-level-campaign).
