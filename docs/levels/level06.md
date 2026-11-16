# Level 6: Relay Station (`level06`)

| Id | Theme | Size | Difficulty | Human time | New idea |
|---|---|---|---|---|---|
| `level06` | `tech` | 30×26 | 6 | 6–8 min | Ambush rooms behind closed doors; an optional duct loop |

![Start view of level06](../images/levels/level06.png)

*Start view (headless `--frames 5` screenshot, 320×240).*

## Design intent

Level 6 introduces **ambush rooms**: a closed door blocks sight and sound, so a squad waits
behind it until the player opens it. Both ambush doors are signposted the same way, with a decoration
**pair** and a **health pickup within 6 cells before the door**, so the player learns the pattern. The first
(R2) holds three guards standing back from the door; the second (R5) a brute right behind it (the one
allowed close enemy). Between them, the relay hall is an open fight and the antenna wing a short pincer.
The **duct loop** is an optional 1-wide side route with two runners and an ammo pickup that rewards
exploring; the bot never needs it. Keys run blue → gold; the gold door at the east end of the main corridor
is in sight from the start, so the goal is clear from the first second.

## Map

`game/maps/level06.map` (grid only; the file starts with two `;` comment lines). Coordinates are (column, row) from the top-left, starting at 0.

```text
######################AAAAAAAA
##################.#..A......A
#########.............D......A
##########.###.#####..A......A
##############D#######A......A
########.............#A......A
##...................#A.A..A.A
##.#####...A.....A...#A......A
##.#####...A.....A...#A......A
##.#####.............#A......A
##.....#.............#A......A
######.#######..######AAAAAAAA
#....#.#######..###...B......B
#.....................DT.....B
#.....................B......B
#....####.###D########B......B
#######AAAAAA.AAAAAA##B......B
#######A...........A##B......B
#######A...........A##B......B
#######A...........A##B......B
#######A..A.....A..A##B......B
#######A...........A##B......B
#######A...........A##B......B
#######A...........A##B......B
#######AAAAAAAAAAAAA##B#B.B##B
######################BBBXBBBB
```

Legend: `.` floor, `#` wall, `A` wall variant A, `B` wall variant B, `D` door (see the doors below), `T` checkpoint trigger, `X` exit switch.

The same grid with the entities (generated from the entity file): `P` player, `g` guard, `b` brute, `r` runner, `h` health, `a` ammo, `k` key card, `v` decoration (vase), drawn on its floor cell.

```text
   0         1         2
   012345678901234567890123456789
 0 ######################AAAAAAAA
 1 ##################g#hvA......A
 2 #########.......a.....Db.g...A
 3 ##########g###.#####.vA......A
 4 ##############D#######A......A
 5 ########.....vav...r.#A......A
 6 ##..r.......g........#A.A..A.A
 7 ##a#####...A.....A...#A......A
 8 ##r#####...A.....A...#A......A
 9 ##.#####.r.........g.#A....k.A
10 ##.....#.............#A......A
11 ######.#######..######AAAAAAAA
12 #....#.#######..###..vB......B
13 #.P...a...............DT.....B
14 #...........v.v......vB......B
15 #....####h###D########B......B
16 #######AAAAAA.AAAAAA##B......B
17 #######A...........A##B.v....B
18 #######A...g.g.g...A##B......B
19 #######A...........A##B.v....B
20 #######A..A.....A..A##B...b..B
21 #######A...........A##B.v....B
22 #######A.....k.....A##B......B
23 #######A...........A##B..h...B
24 #######AAAAAAAAAAAAA##B#B.B##B
25 ######################BBBXBBBB
```

### Areas

- **R1 airlock** x 1–4, y 12–15: spawn (2, 13) facing east into C1.
- **C1 main corridor** x 5–21, y 13–14 (2 wide), with a health alcove (9, 15) and a 3×3 lobby x 19–21,
  y 12–14 in front of the gold door.
- **R2 switch room = ambush 1** x 8–18, y 17–23 (`A` walls, two `A` switch cabinets), behind D2 (13, 15) and a
  1-cell vestibule (13, 16). Blue key at the back.
- **R3 relay hall** x 8–20, y 5–10, four `A` relay racks, open to C1 through a 2-wide passage (14–15, 11–12);
  the blue door Db (14, 4) in its north wall.
- **Duct loop** (optional), 1 wide: from R3's west side (7, 6) west to (2, 6), south to (2, 10), east to
  (6, 10) and down into C1 at (6, 12).
- **R4 antenna wing** behind Db: a neck (14, 3), a corridor along y 2 (x 9–19) with two alcoves (10, 3) and
  (18, 1), and a lobby x 20–21, y 1–3 in front of D5.
- **R5 control room = ambush 2** x 23–28, y 1–10 (`A` walls, two consoles), behind D5 (22, 2). Gold key in the
  far corner.
- **R6 uplink** x 23–28, y 12–23 (`B` walls) behind the gold door Dg (22, 13): the trigger just inside, an exit
  niche (25, 24) with the switch `X` at (25, 25).

## Enemies

| Type | Cell | Where |
|---|---|---|
| guard | (11, 18) | R2 (ambush 1) |
| guard | (15, 18) | R2 (ambush 1) |
| guard | (13, 18) | R2 (ambush 1): the three stand in a line 3 cells in |
| guard | (12, 6) | R3 |
| guard | (19, 9) | R3 |
| guard | (10, 3) | R4 west alcove (pincer) |
| guard | (18, 1) | R4 east alcove (pincer) |
| guard | (25, 2) | R5, 3 cells behind D5 |
| brute | (23, 2) at x 23.7, y 2.5 | R5 (ambush 2): 1.2 cells behind D5, in reach when it opens |
| brute | (26, 20) | R6, 7.6 cells from Dg in the open |
| runner | (9, 9) | R3 |
| runner | (19, 5) | R3 |
| runner | (2, 8) | duct |
| runner | (4, 6) | duct |

Total 8/2/4 (guard/brute/runner) = 14, as in the design table. Threat T = 2·8 + 4·2 + 4 = 28 shots.

## Pickups

All at the default values (+25 HP, +12 ammo).

| Kind | Cell | Where | On the critical path? |
|---|---|---|---|
| ammo | (6, 13) | C1, at the airlock mouth | yes |
| health | (9, 15) | C1 alcove, 4 cells before D2 | side (1 cell off) |
| ammo | (14, 5) | R3, in front of Db | yes (doorway approach) |
| ammo | (2, 7) | duct loop | side (optional loop) |
| ammo | (16, 2) | R4 wing corridor | yes |
| health | (20, 1) | lobby, 2 cells before D5 | side |
| health | (25, 23) | R6, in front of the exit niche (after the brute) | yes |

3 health (75 HP), 4 ammo (48 rounds): as in the economy table.

## Keys, locks and doors

| Door | Cell | Lock | Key card at | Notes |
|---|---|---|---|---|
| D2 | (13, 15) | none | – | Ambush 1 (R2); pair; health 4 cells before |
| Db | (14, 4) | blue | (13, 22) | R3's north wall; pair |
| D5 | (22, 2) | none | – | Ambush 2 (R5); pair; health 2 cells before |
| Dg | (22, 13) | gold | (27, 9) | End of C1, seen from the spawn; pair |

Order: **blue** (R2) → Db → **gold** (R5) → Dg. `inspect-map --level 6`: keys `blue, gold`.

Checkpoint trigger `T` at (23, 13): the first cell behind Dg; `CHECKPOINT REACHED` as the player enters the uplink.

## Decorations (landmarks)

- **Pairs**: D2 (12, 14) / (14, 14); Db (13, 5) / (15, 5); D5 (21, 1) / (21, 3); Dg (21, 12) / (21, 14).
- **Line**: (24, 17), (24, 19), (24, 21) along R6's west side, pointing south to the exit niche.
- No cluster: one in the airlock filled the start view, and one in the relay hall hid the blue door from the
  passage, so both were removed (a cluster is optional).

## Light zones

None (full light everywhere).

## Intended route (walkthrough)

1. From the airlock, east along C1 (ammo at the mouth). The gold door (pair) is in view at the far end.
2. D2 on C1's south side (pair; the health alcove is 4 cells before it): open it; the three guards stand in
   a line 3 cells in. The blue key is at the back of R2.
3. Back to C1 and north through the passage into R3: 2 guards and 2 runners in the open. Ammo in front of
   Db (pair).
4. Db → neck → wing corridor: the guards in the two alcoves close in from both sides. Ammo on the corridor;
   the lobby with health and the D5 pair.
5. D5: the brute is in reach as the door opens, the guard 3 cells back. Gold key in the far corner (27, 9).
6. Back through the wing, R3 and C1 to the lobby at the east end; Dg → checkpoint → the uplink: the brute is
   7.6 cells away in the open; health in front of the exit niche; the switch at (25, 25).
7. Optional: the duct loop from R3's west side (2 runners, ammo) back to C1 by the airlock.

## Bot results

Measured 2026-11-16 on macOS with the existing `build/macos/neo_wolf_autoplay` (build of commit `04b86d0`, no rebuild); runs are deterministic per build.

| Run | Result | Time s | Damage | HP start → exit | Ammo start → exit | Kills | Pickups | Keys | Shots |
|---|---|---|---|---|---|---|---|---|---|
| Fresh (`--level 6`) | complete | 52.93 | 37 | 100 → 88 | 24 → 33 | 13/14 | 6/9 | blue, gold | 27 |
| Campaign (`--all`) | complete | 52.93 | 37 | 100 → 88 | 37 → 46 | 13/14 | 6/9 | blue, gold | 27 |

| Target (README) | Value | Met |
|---|---|---|
| Fresh: damage taken 20–55 | 37 | yes |
| Fresh: HP at exit ≥ 45 | 88 | yes |
| Fresh: kills ≥ 7 of 14 | 13 | yes |
| Fresh: bot time ≤ 110 s | 52.9 | yes |
| Campaign: HP at exit ≥ 40 | 88 | yes |
| Campaign: ammo at exit ≥ 12 | 46 | yes |

The campaign run used builder A's levels 1–5 as they were in the working tree at the time (still in progress); the integrator re-measures the final campaign.

Where the bot takes its damage (fresh run, from `--time-limit` cut-offs and removing one enemy at a time in a scratch copy):

- R2 ambush 1: **12** (one guard reaches the bot);
- R3, the wing pincer, the duct: **0**;
- R5 ambush 2: **25** (the brute behind D5);
- R6 uplink: **0** (the brute is seen across the room).

## Deviations from the brief

- None in the counts.
- Ambush 1's guards stand **3.0–3.6 cells** from D2 (a line across R2, 3 cells in) instead of 2–3. At about
  2.2 cells, two of them would be within 1.5 cells of the door's far-side cell (the vestibule), and rule 9
  allows only one such enemy per ambush door. Spreading them to the sides instead made a pincer that cost
  the bot 61 HP.
- No decoration cluster (see Decorations).
