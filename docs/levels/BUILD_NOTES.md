# Phase 8 build notes (history)

> **History, frozen 2026-11-16.** These are the per-level briefs the two builders worked from
> in Roadmap Phase 8. The levels as built are described in their pages,
> [level01.md](level01.md) to [level10.md](level10.md), whose "Deviations and exceptions"
> sections say where and why a level differs from its brief. Do not update this file; change
> the level pages instead.
>
> The working parts of the original notes moved when the campaign was integrated:
> verification steps and light-zone how-to → [LEVEL_AUTHORING.md](../LEVEL_AUTHORING.md);
> balancing lessons → [LEVEL_AUTHORING.md, Balancing tips](../LEVEL_AUTHORING.md#balancing-tips-learned-building-the-10-level-campaign);
> the light zones as built → the level pages of levels 5, 7 and 10; the design, economy, bot
> targets, rules and measured results → [README.md](README.md).

Builder A built levels 01–05 (reworking `level01` into a tutorial and replacing the Phase 7
`level02`/`level03` and the stubs `level04`/`level05`); builder B built levels 06–10. Vocabulary:
*critical path* = the shortest route the bot takes (spawn → keys in order → exit); *on path* =
in a 1-wide corridor or doorway approach of that route; *side* = off the route, within 6 cells
of it unless stated; *pair / line / cluster* = the decoration landmarks of README rule 13.
Coordinates were guidance (± 2 cells); counts and the order of events were exact.

---

## Builder A

### level01 — Outpost Gate (`bunker`, 18×12) — tutorial

Idea: every basic action once, in a safe order: move and turn → walk over a pickup → shoot a
guard at range → open a door → pick up health → use the exit switch. No keys, no trigger.

```text
 +---------+            +----------------------+
 | R1      |   C1       | R2 yard              |
 | spawn P |==$a========|            G1        |
 | (pair at|  ammo      |                      |
 |  C1)    |            +---------D1-----------+
 +---------+                      |
                 +----------------v---------+--------+
                 | R3 hall             G2   | R4 exit|
                 | [$h alcove]  line>>>     |      X |
                 +--------------------------+--------+
```

- **R1 spawn bay** (west, about 4×4): player at its centre facing east (`angle` 0) into C1.
  Decoration **pair** at the mouth of C1.
- **C1** (1 wide, 4–5 long): one **ammo** pickup in the middle (on path: teaches walk-over
  pickups; `AMMO REPLENISHED`).
- **R2 yard** (about 8×5): guard **G1** in the far corner, **not** in line with C1 (rule 9: not
  in sight of the spawn), 6+ cells from where C1 enters, so it notices the player on entry and
  walks into plain sight. Teaches shooting.
- **D1** (unlocked) in the yard's south wall, framed by `A` walls so it reads as a door to
  open. Teaches `E`.
- **R3 hall** (about 8×4): guard **G2** at least 3 cells from D1's far side and in view the
  moment the door opens. **Health** pickup in a 1-cell alcove off the hall (side, 1 cell off the
  path): the first health the player sees, right after the two fights.
- **R4 exit room** (about 3×4, open to R3, no door): the **exit switch X** in the east outer
  wall, framed by `B` walls, visible from the R3/R4 opening. Decoration **line** of three along
  R3's wall pointing at it.
- Counts: 2 guards; 1 health, 1 ammo; 1 door; 0 keys. Bot target: 0–12 damage.

### level02 — Old Cellars (`stone`, 22×14) — key cards

Idea: meet a locked door first, learn what it needs, find the key in a side room, come back.

```text
 +--------+          +-------------------- R3 key room ----+
 | R1     |          |  G   $a        [blue key]           |
 | entry  |          +--------------D2---------------------+
 | P      |   C1     +--------------v----------------+   +--------------+
 |        |==========| R2 vaulted hall    G      G   |   | R4 exit vault|
 +--------+          |                         (pair)Db==>  G   $h     X |
                     +-------------------------------+   +--------------+
```

- **R1 entry cellar** (west): player facing east into C1.
- **R2 vaulted hall** (about 8×6, pillars of `A` optional): **2 guards** on either side of the
  far half, 5+ cells from the hall entrance, not visible from the spawn.
- **Db** = the **blue locked door**, in the hall's east wall straight ahead of the entrance,
  framed by a decoration **pair**: the player walks up, presses `E`, reads `NEED BLUE KEY`.
- **D2** (unlocked) in the hall's north wall, visible from Db, leads to **R3 key room**
  (a long cellar): **1 guard** at least 2 cells from D2, **1 ammo** (on path, on the way to the
  key), the **blue key** at the far end. Keep D2 and Db at least 3 cells apart (rule 10).
- **R4 exit vault** behind Db: **1 guard** 3+ cells from the door, **health** on the path after
  it, the **exit X** on the east outer wall framed by `B`, a decoration **line** leading to it.
- Counts: 4 guards; 1 health, 1 ammo; blue key → 1 blue door; 1 unlocked door. Bot target
  0–20 damage; `inspect-map` keys: `blue`.

### level03 — Server Annex (`tech`, 24×18) — hearing and loops

Idea: gunfire is heard along open cells; closed doors block it. A ring corridor lets enemies
come from both sides; a closed server room stays asleep until opened. First checkpoint trigger.

```text
 +--------------------------------------------------+
 | ring (2 wide)   G(NW)                G(NE)  Dr ==> R5 exit wing (B walls)
 |   +------------------------------------+         |   T  G  G  $h      X
 |   | R3 server room (closed)            |         |
 |   |  G      G       $a     [red key]   |         |
 |   +-----------------Ds-----------------+         |
 | ring          $a (on path)                       |
 +--R1 spawn (SW corner room) P---------------------+
```

- **Ring corridor** (2 wide) round a central **server room** (about 12×6). Spawn in a small
  room at the south-west corner, facing east along the south ring.
- **2 ring guards**, one in the north-west and one in the north-east part of the ring, 8+ cells
  from the spawn. The first shot in the ring wakes both: they come from **both directions**
  (the level's lesson, and its 5–30 bot damage).
- **Ds** (unlocked) on the server room's south side. Inside: **2 guards** (2+ cells from Ds),
  **1 ammo**, the **red key** at the back. With Ds closed they sleep through the ring fight;
  opening it during a fight wakes them. Decoration **pair** at Ds.
- **1 ammo** on the south ring (on path, before the first fight); **1 health** in an alcove of
  the north ring (side).
- **Dr** = **red locked door** at the ring's north-east corner, visible from the ring, leading
  to **R5 exit wing**: a `T` **trigger** just inside (`CHECKPOINT REACHED`), **2 guards** 3+
  cells beyond it, **health** on the path after them, the **exit X** at the wing's end framed by
  `B`, decoration **line** in the wing.
- Counts: 6 guards; 2 health, 2 ammo; red key → 1 red door; 1 unlocked door; 1 trigger. Bot
  target 5–30 damage; keys: `red`.

### level04 — Motor Pool (`bunker`, 26×20) — the brute

Idea: the brute, first seen safely across a big garage (4 shots while it lumbers in), then the
gold key and door.

```text
                  +--------------------- R3 garage (12x8) ----------------------+
                  |  A-pillars            B (brute, far north end)              |
 R4 pit office  <=D3       (seen from the south entrance, 8+ cells away)  (pair)Dg==> R5 fuel depot (L)
 G  G  $a [gold key]                                                           |    G  G  $h  -> R6 exit bay X
                  +-----------------------v-------------------------------------+
                                     R2 checkpoint: G  G  $a
                                          ^
                                     R1 gatehouse (spawn, south) P
```

- **R1 gatehouse** (south): player facing north. **R2 checkpoint** (small room or corridor
  chicane): **2 guards** (8+ cells from spawn), **1 ammo** on path.
- **R3 garage** (about 12×8, a few `A` pillars, open sight lines): the **brute** at the far
  north end, 8+ cells from the south entrance, so it is seen before it notices (it notices at
  5.5). No other enemy in the garage. **1 health** in an alcove of the garage (side).
- **D3** (unlocked) in the garage's west wall → **R4 pit office**: **2 guards** (2+ cells from
  D3), **1 ammo**, the **gold key**.
- **Dg** = **gold locked door** in the garage's east wall, framed by a decoration **pair**,
  visible from the garage entrance.
- **R5 fuel depot** (L-shaped corridor/room): **2 guards** round the bend (they meet the player
  at short range: the level's 5–35 bot damage), **health** on the path after the bend.
- **R6 exit bay**: **exit X** framed by `B`, decoration **line**.
- Counts: 6 guards, 1 brute; 2 health, 2 ammo; gold key → 1 gold door; 1 unlocked door. Bot
  target 5–35; keys: `gold`.

### level05 — Catacombs (`stone`, 28×24) — runners, two keys, a branch, a dim crypt

Idea: runners (fast, fragile, they rush from far), two keys in sequence, a fork with two
routes to the first key, and a dim crypt (a light zone; as built: [level05.md](level05.md#light-zones)).

```text
                         R1 stair landing (spawn, north centre) P
                                   |
               +------------ R2 ossuary junction ------------+
               |  G      $a (on path)         (pair) Dr      |   red door in the junction's south wall
          west branch                                   east branch
   ossuary gallery: 1 wide,                      flooded tunnel: 2 wide, winding
   alcoves; R R G                                R R G
               \                                       /
                +----------- R3 chapel (south-west) ---+
                     G   $a   $h   [red key]
   Dr -> R4 crypt (DIM, light zone; pair at entrance)  G  G  $h(side)  [blue key at far end]
          Db (blue door, seen on entering the crypt) -> R5 exit tomb: B (brute) seen across 6+ cells, $h after, X
```

- **R1 stair landing**: spawn facing south. **R2 junction** (8+ cells from spawn): **1 guard**,
  **1 ammo** on path, the **red door Dr** in its south wall with a **pair**, seen before the
  player chooses a branch.
- **Two branches** from the junction to **R3 chapel**, each with **2 runners + 1 guard**:
  west = a 1-wide gallery with alcoves (runners burst out of alcoves), east = a 2-wide winding
  tunnel (runners come round the bends). Both lengths within 4 cells of each other; the bot
  takes the shorter. The branches form a loop: the player can go out one way and back the other.
- **R3 chapel**: **1 guard**, **1 ammo** and **1 health** (on path, after the branch fight),
  the **red key**.
- **R4 crypt** behind Dr: the **dim zone** (a `lights` zone at 0.5 over the whole crypt; list it
  in the level page). **2 guards** (no brute in the crypt), **1 health** (side alcove), the
  **blue door Db** visible on entering the crypt, the **blue key** at the crypt's far end.
- **R5 exit tomb** behind Db: **1 brute** seen across 6+ cells, **1 health** on the path after
  it, the **exit X** on the tomb's south wall framed by `B`, decoration **line**.
- Counts: 6 guards, 1 brute, 4 runners; 3 health (chapel, crypt, tomb), 3 ammo (junction,
  chapel, plus one in a gallery alcove or tunnel bend, side); red key → red door, blue key →
  blue door; no unlocked doors needed. Bot target 10–45; keys: `red, blue`.

---

## Builder B

### level06 — Relay Station (`tech`, 30×26) — ambush rooms

Idea: closed doors hide waiting squads (signposted by a decoration pair, health before each);
an optional maintenance loop rewards exploring. Keys blue → gold.

```text
                         R4 antenna wing (corridor with 2 alcove guards: pincer) -> R5 control room
                         ^ Db (blue door)          $h before D5      D5 (AMBUSH 2, pair): B 1.2 behind + G; [gold key]
         +---- R3 relay hall (big, open): G G R R  $a ----+
         |   west side: duct loop entrance (optional)     |    duct loop: 1 wide, R R, $a, back to C1
 R1 airlock P ===== C1 main corridor (2 wide, W->E) ==$a===============================(pair) Dg ==> R6 uplink
         |      $h alcove        D2 (AMBUSH 1, pair)                                         T  B (open, 6+ cells)  $h  X
         +-------------- R2 switch room: G G G (2-3 cells behind D2)  [blue key at back]
```

- **R1 airlock** (west): spawn facing east into **C1** (2 wide, the spine of the level), **1 ammo**
  on C1 (on path).
- **R2 switch room = AMBUSH 1**: door **D2** on C1's south side, decoration **pair**, **1 health**
  in a C1 alcove within 6 cells before it. Inside: **3 guards** standing 2–3 cells behind the door
  (not closer), the **blue key** at the back.
- **R3 relay hall** (north of C1, open, big): **2 guards + 2 runners**, **1 ammo** (on path), the
  **blue door Db** in its north wall.
- **Duct loop** (optional, from R3's west side back to C1): 1 wide, **2 runners**, **1 ammo**
  (side). Rewards exploring; the bot never needs it.
- **R4 antenna wing** behind Db: a corridor with two alcoves, **1 guard** in each, facing each
  other (they close in from both sides: pincer), **1 ammo** on the path through it, **1 health**
  within 6 cells before D5.
- **R5 control room = AMBUSH 2**: door **D5**, decoration **pair**; inside **1 brute 1.2 cells
  behind the door** (the one allowed close enemy; it costs the bot about 25 HP) and **1 guard**
  3 cells back; the **gold key**.
- **R6 uplink** behind the **gold door Dg** at C1's east end (pair): a `T` **trigger** at the
  entrance, **1 brute** in the open 6+ cells away, **1 health** on the path after it, the **exit
  X** framed by `B`, decoration **line**.
- Counts: 8 guards, 2 brutes, 4 runners; 3 health (C1 alcove, R4, R6), 4 ammo (C1, R3, duct,
  R4); blue key → blue door, gold key → gold door; 2 unlocked doors (D2, D5); 1 trigger. Bot
  target 20–55; keys: `blue, gold`.

### level07 — Blackout Barracks (`bunker`, 32×28) — dark wings, keys in parallel

Idea: two dark wings, each ending in a lit key room; the keys can be fetched in either order,
and the exit wing needs both (red door, then gold door).

```text
                 R5 exit wing (north): Dr (red) --4+ cells-- Dg (gold) --> exit hall: G G  $h  X (B walls)
                                   ^ (pair at Dr)
 R2 WEST WING (dark, bunk rooms)   |                         R3 EAST WING (dark, armoury bays)
 G G G  R R  $a(path)          R1 PARADE GROUND (lit hub)    G G G  R R  $a(path)
 -> lit key room: B [red key] $h   cluster in the centre      -> lit key room: B [gold key] $h
                                   R R (rush from the mess)
                                   |
                              R4 MESS HALL (lit, south): $a $h (side)        spawn P at the hub's south edge
```

- **R1 parade ground** (lit hub, about 10×8, decoration **cluster** in the centre): spawn at its
  south edge facing north, the **red door Dr** visible on the north side (pair). **2 runners**
  start in **R4 mess hall** (south-east, lit) and rush in when they notice the player (8+ cells
  from the spawn). **1 ammo** in the hub (on path).
- **R2 west wing / R3 east wing**: each a dark corridor (a `lights` zone at 0.35, list it) of 2–3 small
  bunk/armoury rooms off it, open doorways (no doors). Each wing: **3 guards + 2 runners** waiting
  in the side rooms (so they step out at close range in the dark: the human challenge), **1 ammo**
  on path, then a **lit key room** at the far end with **1 brute** and the key (**red** west,
  **gold** east) and **1 health** on the path after the brute. Enter one key room round a corner
  (the brute is met close: about 25 bot HP), the other in line of sight across the room.
- **R4 mess hall** (side): **1 ammo, 1 health**.
- **R5 exit wing** (north): **Dr** (red), a corridor of 4+ cells, then **Dg** (gold) (rule 10),
  then the **exit hall**: **2 guards** 3+ cells from Dg, the **exit X** framed by `B`, decoration
  **line**; the 4th **health** on the path after the 2 guards.
- Counts: 8 guards, 2 brutes, 6 runners; 4 health (west key room, east key room, mess, exit
  hall), 4 ammo (hub, west wing, east wing, mess); red key → red door, gold key → gold door, either
  order; no unlocked doors. Bot target 25–60; `inspect-map` keys: `red, gold` or `gold, red`.

### level08 — Reactor Spine (`tech`, 34×32) — three keys in a chain, mixed squads

Idea: a long north–south spine cut by three locked doors; each key is in a side bay before its
door; squads mix types (a brute with guards, runner pairs).

```text
            R7 REACTOR CORE (arena, north; cluster round a central pillar block): B G G ; X on the far wall
                ^ Dg (gold, pair)  T + $h before it
   Bay 5 (W): turbine hall, pillars: B R R [gold key]     Bay 6 (E): G $h (side)
                ^ upper spine  $a (path)
                Dr (red, pair) ------------------------------------------
   Bay 4 (W): R R G $a (side)                              Bay 3 (E): reactor control: B G G [red key] $a(path) $h after
                ^ mid spine
                Db (blue, pair) ------------------------------------------
   Bay 1 (W): coolant pumps: G G R [blue key]              Bay 2 (E): supply: G G $a $h (side)
                ^ lower spine  R (roaming)
            R1 LOADING DOCK (spawn, south) P  $a (path)
```

- **Spine**: 2 wide, runs south → north through the middle; bays open off it through doorways
  (no doors except the three locks and at most one unlocked door per side). Spawn in **R1
  loading dock** facing north; **1 ammo** in the dock (on path); **1 runner** roaming the lower
  spine (8+ cells from spawn).
- **Lower section**: **Bay 1** (west, on path) **2 guards + 1 runner** and the **blue key**;
  **Bay 2** (east, side) **2 guards**, **1 ammo, 1 health**. **Db** (blue) across the spine.
- **Mid section**: **Bay 3** (east, on path, reactor control): **1 brute + 2 guards** as one squad
  (the brute in front, guards behind), **1 ammo** on the way in, the **red key**, **1 health**
  on the path out (after the fight). **Bay 4** (west, side): **2 runners + 1 guard**, **1 ammo**.
  **Dr** (red) across the spine.
- **Upper section**: **1 ammo** on the spine (on path). **Bay 5** (west, on path, turbine hall
  with `A` pillars): **1 brute + 2 runners** and the **gold key**. **Bay 6** (east, side):
  **1 guard**, **1 health**. A `T` **trigger** and **1 health** within 6 cells before **Dg** (gold,
  pair).
- **R7 reactor core** (arena): **1 brute + 2 guards**, a decoration **cluster** round a central
  pillar block, the **exit X** on the far (north) wall framed by `B`, visible from Dg; decoration
  **line** from Dg towards it.
- Counts: 10 guards, 3 brutes, 6 runners; 4 health (Bay 2, Bay 3, Bay 6, before Dg), 5 ammo
  (dock, Bay 2, Bay 3, Bay 4, upper spine); blue → red → gold. Bot target 30–70; keys:
  `blue, red, gold`.

### level09 — Sunken Archive (`stone`, 38×36) — non-linear hub

Idea: a central reading room with three wings; red and blue keys in either order; the gold key
behind the blue door; the exit behind red then gold. The player crosses the hub several times;
an optional sealed vault in the hub holds a squad and a supply cache.

```text
                         N WING: Map Room (behind Db, pair): B G G R R [gold key] $a(path) $h after
                                         ^ Db (blue)
 W WING: flooded stacks           +-- HUB: Great Reading Room (10x10, A pillars, cluster) --+        E WING: catalogue vaults
 G G G  R R  $a(path)   <=========+   G G  R R   $a(path)                                   +=========>  G G G  $a(path)  $h(side)
 key room: B (round a corner)     |   sealed vault (unlocked door Dv, pair): R R $a $h      |        key room: B (seen across) [blue key]
 [red key]  $h after (path)       +---------------------------------------------------------+
                                         v Dr (red, pair) -> S antechamber: B G G ; $h before Dg
 spawn corridor enters the hub from the south-west  P                    Dg (gold) -> exit cistern X (B walls, line)
```

- **Spawn** in a corridor entering the hub from the south-west, facing north-east; the hub's
  **2 guards + 2 runners** are 8+ cells away. **1 ammo** in the hub (on path).
- **Hub**: about 10×10, `A` pillars, decoration **cluster** in the centre. The **red door Dr**
  (south side) and the **blue door Db** (north side) are both visible from the hub (pairs).
- **W wing** (flooded stacks, long, aisles of shelving walls): **3 guards + 2 runners**, **1 ammo**
  on path, a key room entered **round a corner** with **1 brute** (close encounter) and the
  **red key**, **1 health** on the path out.
- **E wing** (catalogue vaults, rooms off a corridor): **3 guards**, **1 ammo** on path, **1
  health** (side), a key room where **1 brute** is **seen across** the room, and the **blue key**.
- **N wing** (map room, behind Db): **1 brute + 2 guards + 2 runners** as a mixed squad,
  **1 ammo** on the way in, the **gold key**, **1 health** on the path out.
- **Sealed vault** (optional, off the hub, unlocked door **Dv** with a pair): **2 runners**
  2+ cells behind the door, **1 ammo + 1 health** (side). The player passes its door every
  time they cross the hub.
- **S antechamber** behind Dr: **1 brute + 2 guards**, **1 ammo** (side), **1 health** within
  6 cells before **Dg** (gold, pair). Behind Dg the **exit cistern** with the **exit X** framed
  by `B`, decoration **line**. Keep Dr and Dg at least 4 cells apart (rule 10).
- Counts: 12 guards, 4 brutes, 8 runners; 5 health (W, E, N, vault, S antechamber), 6 ammo
  (hub, W, E, N, vault, S antechamber); red and blue in either order, gold behind blue, exit
  behind red then gold; 1 unlocked door (Dv). Bot target 35–80; `inspect-map` lists red and
  blue (in either order) before gold.

### level10 — Wolf's Den (`classic`, 40×40) — finale

Idea: everything once more at full strength, in a clear order: an open courtyard fight, a
tower brute round a corner, a pincer hall, an ambush room, a quiet supply room with a
checkpoint, a dim chapel, and a final arena with a brute squad in front of the exit.

```text
                        R6 THRONE ROOM ARENA (12x12, pillars): B B G G R R ; X behind the throne (north wall)
                                       ^ Dg (gold, pair)   $h before it
   R5 chapel corridor -> chapel crypt (dim): B (seen across the nave) G G R R [gold key]  $h(side)  $a(side)
                                       ^
                        R4 ARMOURY / SUPPLY (quiet, cluster): T  $h $h $a $a          (via Db, blue, pair)
                                       ^ Db
   R3 GREAT HALL (pillared; alcoves): G G G R R (pincer from side alcoves)  $a(path)  $h(side alcove, near D-barracks)
       E: D-barracks (AMBUSH, pair) -> barracks: B 1.2 behind + G G ; [blue key]  $a(side)
                                       ^ Dr (red, pair)
   R2 OUTER COURTYARD (open): G G G R R  $a(path)
       W: tower stair (1 wide, turns) -> tower top: B round a corner, [red key], $a(path), $h after
                                       ^
                        R1 DRAWBRIDGE GATE (spawn, south) P
```

- **R1 gate** (south): spawn facing north. **R2 outer courtyard** (open, big): **3 guards +
  2 runners** seen across open ground (an easy warm-up for the bot), **1 ammo** on path; the
  **red door Dr** in its north wall (pair).
- **West tower**: a 1-wide stair that turns twice, **1 brute met round the last corner**, the
  **red key**, **1 ammo** on the way up, **1 health** on the path down.
- **R3 great hall** behind Dr (pillared, side alcoves): **3 guards + 2 runners** in the alcoves
  (the first shot wakes them all: pincer), **1 ammo** on path; **1 health** in a side alcove
  within 6 cells of **D-barracks**.
- **Barracks = AMBUSH** (east of the hall, unlocked door with a pair): **1 brute 1.2 cells behind
  the door + 2 guards** 3 cells back, the **blue key**, **1 ammo** (side).
- **R4 armoury / supply room** behind **Db** (blue, pair, north of the hall): quiet, no enemies,
  decoration **cluster**, a `T` **trigger** (`CHECKPOINT REACHED`), **2 health + 2 ammo** (on
  path). All pickups use the default values.
- **R5 chapel corridor and crypt** (dim: a `lights` zone at 0.5, list it): **1 brute** seen across the
  nave, **2 guards + 2 runners**, the **gold key**, **1 health + 1 ammo** (side).
- **R6 throne room arena** behind **Dg** (gold, pair), **1 health** within 6 cells before Dg:
  **2 brutes + 2 guards + 2 runners**, `A` pillars placed so that **at most one brute can reach
  the player at a time** from the entrance; the **exit X** behind the throne on the north wall,
  framed by `B`, visible from Dg; decoration **line** up the aisle.
- Counts: 12 guards, 5 brutes (tower, barracks, chapel, arena ×2), 8 runners; 6 health (tower,
  hall alcove, supply ×2, chapel, before Dg), 7 ammo (courtyard, tower, hall, barracks, supply
  ×2, chapel); red → blue → gold; 1 unlocked door (barracks); 1 trigger. Bot target 45–95;
  keys: `red, blue, gold`.
