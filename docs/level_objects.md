# Level objects and crystal sources

Notes from reading the level files (`original/crocii/*.WAD`) with `tools/wad_objects.py` and
`tools/strat_disasm.py`. Everything here comes from the game data; items marked "inferred" still want an
in-game check.

## Placement records

Each level file has one table of 64-byte records, one per placed object (16 little-endian int32s):

| Field | Meaning |
|---|---|
| 8, 9, 10 | x, y, z position |
| 12 | type: offset of the object's script entry point from the file's script base |
| 13 | number of saved-state slots |
| 14 | offset of the object's saved state in the level (4 x field 13 apart; -1 = no saved state) |

Field 14 is a ready-made permanent ID for a collectable: it's unique in the level and the game uses it itself.

## Scripts

- The game creates an object with `script position = [croc2.exe+0xBC7CE0] + type` (`0x14005DC00`). In the file,
  that is `base + type`. The base is found by voting on the standard script opening (`176`, `212`); for
  `T5L4M006` it was confirmed against the running game.
- A script names itself: opcode 212 is "set name", with a byte offset to the string from the word after it.
- Opcode 124 pushes a type; 94 and 192 spawn it. Scripts that are only spawned are found that way.
- Opcode 173 is "gain crystal".
- Numbers are 20.12 fixed point (4096 = 1).

## White crystal sources

| Source | Crystals | How it's known |
|---|---|---|
| `crystal` (placed) | 1 | 747 placed records |
| `Smash Box`, `Smash Box Cave`, `Smash Box Snow`, `Snow Smash Box (Snowball)` | 5 | The box spawns one holder per crystal until a counter equals its variable 1; every box has 5 there |
| `ring of crystals` | 8 | Variable 0 of each ring is 8 (T4L6: 1 + 15 boxes + 3 rings = 100) |
| Treasure chest (`SeeSaw Chest Count`, T1L3) | 10 | The chest script loops "gain crystal" 10 times; 5 chests |
| `CCCSwitch` pattern (T2L3) | 28 (inferred) | Its script spawns crystals on a grid; 16 were present 4 s after load and still appearing. 14 + 50 + 8 + 28 = 100 |
| `inca baby` (T4L2) | 2 per rescue | Each of the baby script's five exits gives 2 crystals |

A box's holder objects are the ones named "Blank" in debug logs; they are what runs "gain crystal" for a box.

## White crystals per stage

All sub-maps of a stage added together.

| Stage | Placed | Boxes | Rings | Other | Total |
|---|---|---|---|---|---|
| T1L2 | 55 | 9 | 0 | | 100 |
| T1L3 | 40 | 2 | 0 | 5 chests x 10 | 100 |
| T1L4 | 60 | 8 | 0 | | 100 |
| T1L6 | 55 | 9 | 0 | | 100 |
| T1S1 | 60 | 8 | 0 | | 100 |
| T2L2 | 5 | 19 | 0 | | 100 |
| T2L3 | 14 | 10 | 1 | CCCSwitch pattern 28 (inferred) | 100 |
| T2L5 | 60 | 8 | 0 | | 100 |
| T2L7 | 35 | 13 | 0 | | 100 |
| T2S1 | 25 | 15 | 0 | | 100 |
| T3L2 | 35 | 13 | 0 | | 100 |
| T3L3 | 25 | 15 | 0 | | 100 |
| T3L5 | 12 | 16 | 1 | | 100 |
| T3S1 | 100 | 0 | 0 | | 100 |
| T4L2 "Save 30 Gobbo babies!" | 40 | 0 | 0 | 30 babies x 2 = 60. A baby that cries gives 0, so the level must stay replayable until every baby has paid out | 100 |
| T4L3 | 35 | 13 | 0 | | 100 |
| T4L6 | 1 | 15 | 3 | | 100 |
| T4S1 | 0 | 20 | 0 | | 100 |
| T1B1 | 20 | 6 | 0 | | 50 |
| T1B2 | 35 | 3 | 0 | | 50 |
| T3B1 | 0 | 10 | 0 | | 50 |
| T3B2 | 0 | 10 | 0 | | 50 |

`T0L1` is not one stage: `T0L1M001`..`M007` are separate maps (shorter replays of earlier levels and two boss
arenas), so they are listed on their own below. The boss totals are placed crystals and boxes in the arena, not a
script bonus (Soveena The Squid: 20 crystals + 6 boxes).

### Stages that don't use placed crystals

| Map | Title | What the files show | Status |
|---|---|---|---|
| T1L5M001 | Croc Vs Dantini Boat Race | 20 crystal holders ("Blank") live, none placed | count not verified |
| T2L6M001 | Hang Glider Valley | 100 crystal holders live, 9 `HGCrystalHoop` placed | matches 100 |
| T3L4M001 | Race Day at Goldrock | 50 crystal holders live | count not verified |
| T3L7M001 | Save 50 Trapped Gobbos! | 45 `F-Fire gobbo` + 5 `F-Fire rescue me gobbo` | crystals per rescue not read yet |
| T0L1M003 | Burning Down the Forest | 10 `F-Fire gobbo` + 4 `F-Fire rescue me gobbo` | crystals per rescue not read yet |
| T0L1M004 | ...the Wheels in the Jungle! | 7 crystals + 1 box = 12 | |
| T0L1M005 | He Stole my lunch! | 12 crystals + 2 boxes = 22 | |
| T0L1M006 | ...the Ice Trapped Gobbos! | 4 crystals + 11 boxes = 59 | |
| T0L1M007 | 3 Lost Treasure Chests | 12 crystals, chests not counted yet | |
| T4L5M001 | Bride of the Dungeon of Defright | no crystal objects found | |

T4L2 babies found so far: 9 placed, 5 in Inca Pots (pots whose variable 0 is set), 4 on the carousel, 2 on swings,
1 on a block stack, plus those carried by the 6 "Inca Dantini with Baby" and thrown along the 4 baby-chain
objects.

## Other collectables (placed records)

| Name | Records |
|---|---|
| `Heart` | 47 |
| `Key` | 19 |
| `Golden Gobbo` / `Special Golden Gobbo` | 16 / 2 |
| Coloured crystals (`bonus crystal`, from the map tour) | 86, plus 7 `Gobbo bonus crystal` and 13 `secret bonus crystal` |
| `ClockworkGobbo` (from the map tour) | 15 |

The coloured-crystal and Clockwork Gobbo scripts set their names in a way the tools don't read yet, so those two
rows come from the debug map tour's dump rather than the files.
