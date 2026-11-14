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

"Gain crystal" (opcode 173) takes an amount: 0 means one crystal, and the race scripts pass 100, 70 or 50.

| Source | Crystals | How it's known |
|---|---|---|
| `crystal` (placed) | 1 | Placed records |
| `Smash Box`, `Smash Box Cave`, `Smash Box Snow`, `Snow Smash Box (Snowball)` | 5 | The box spawns one holder per crystal until a counter equals its variable 1; every box has 5 there |
| `ring of crystals` | 8 | It spawns round a full circle in steps of an eighth (variable 3) |
| Treasure chest (`SeeSaw Chest Count`, T1L3) | 10 | The chest script loops "gain crystal" 10 times; 5 chests |
| `inca baby` (T4L2) | 2 per rescue, 0 if it cried | Each of the baby script's five exits gives 2; 30 babies |
| `HGCrystalHoop` (T2L6) | 10 | 9 hoops; the FAQ lists the same groups of 10 |
| Hang glider landing (T2L6) | 10 / 5 / 0 | On touchdown the `HangGlider` script stores which ring of the target Croc is on (0-3): the bullseye gives 10, the two middle rings 5, the outer ring 0 |
| Race result (T1L5, T3L4) | 100 / 70 / 50 | The finish script gives one of the three by position; the FAQ says 100 for winning |
| `F-Fire gobbo`, `F-Fire rescue me gobbo` | 1 per rescue | One "gain crystal" in each script's rescue path |
| `ForestFireGobboCheck` | 50 | Waits until the rescued count reaches 50, then loops "gain crystal" 50 times |
| `Talking Gobbo 1` puzzle (T4L5) | 20 each | Each of the 5 puzzle Gobbos sets its reward (variable 9) to 20 and loops "gain crystal" that many times; the FAQ says the puzzle must be solved quickly enough |

A box's holder objects are the ones named "Blank" in debug logs; they are what runs "gain crystal" for a box.

## White crystals per stage

Checked two ways: the level files (placed records and scripts) and a PlayStation FAQ, which states "There are
100 of these in every level" and describes the special cases below. The game has 18 levels and 8 bosses, plus
the secret levels and the lost levels.

| Map | Level | Placed | Boxes x5 | Other | Total |
|---|---|---|---|---|---|
| T1L2 | Find the Key! Save the Gobbo! | 55 | 9 | | 100 |
| T1L3 | Find 5 Lost Treasure Chests | 40 | 2 | 5 chests x 10 | 100 |
| T1L5 | Croc Vs Dantini Boat Race | 0 | 0 | 100 for 1st, 70 for 2nd, 50 for 3rd | 100 |
| T1L6 | Save the Bird from the Thief | 55 | 9 | | 100 |
| T1L4 | Get the Gobbo's Sandwich! | 60 | 8 | | 100 |
| T2L2 | Save the Ice Trapped Gobbos! | 5 | 19 | | 100 |
| T2L3 | Chase the Choo Choo Train | 32 | 12 | 1 ring x 8 | 100 |
| T2L6 | Hang Glider Valley | 0 | 0 | 9 hoops x 10, bullseye landing 10 | 100 |
| T2L5 | It's just Snowball Madness! | 60 | 8 | | 100 |
| T2L7 | KaBooom! It's Roger Red Ant | 35 | 13 | | 100 |
| T3L2 | Find the Wheels in the Jungle! | 35 | 13 | | 100 |
| T3L3 | Find the Wheels in the Mine! | 25 | 15 | | 100 |
| T3L4 | Race Day at Goldrock | 0 | 0 | 100 / 70 / 50 by position | 100 |
| T3L5 | Climb The Devil's Tower | 12 | 16 | 1 ring x 8 | 100 |
| T3L7 | Save 50 Trapped Gobbos! | 0 | 0 | 50 Gobbos x 1, plus 50 for rescuing all | 100 |
| T4L2 | Save 30 Gobbo babies! | 40 | 0 | 30 babies x 2 | 100 |
| T4L3 | Up the Waterfall | 35 | 13 | | 100 |
| T4L5 | Bride of the Dungeon of Defright | 0 | 0 | 5 timed puzzles x 20 | 100 |
| T4L6 | Goo Man Chu's Tower (boss 8) | 1 | 15 | 3 rings x 8 | 100 |
| T1S1 | Sailor Tribe - Secret Mine | 60 | 8 | | 100 |
| T2S1 | Cossack Tribe - Secret Ice Cap | 25 | 15 | | 100 |
| T3S1 | Caveman Tribe - Secret Mine | 100 | 0 | | 100 |
| T4S1 | Inca Tribe - Secret Maze | 0 | 20 | | 100 |
| T1B1 | Soveena The Squid | 20 | 6 | | 50 |
| T1B2 | Cannon Boat Keith | 35 | 3 | | 50 |
| T3B1 | Venus Fly Von-Trappe | 0 | 10 | | 50 |
| T3B2 | The Village Masher | 0 | 10 | | 50 |
| T2B1, T2B2, T4B1, T4B2 | Flavio, Lava Lamp Larry, Dante's Peak, Dante's Final Fight | 0 | 0 | none found | 0 |

Not every award is certain to be collected: a baby that cries pays 0, a race pays less for 2nd or 3rd, the
glider landing can pay 5, and a dungeon puzzle pays less (or nothing) when slow. Those stay replayable.

### Lost levels (`T5L2`, `T5L3`, `T5L4`)

"Secret Sailor Village", "Secret Cossack Village" and "Secret Caveman Village": a hub map (`M001`) and five levels
each (`M002`-`M006`, titled "Sailor Secret 1" and so on). They have no white crystals. Each level holds one
coloured crystal (`secret bonus crystal`); five in a village free its Croc egg.

### Demo copies (`T0L1M00x`)

`T0L1M001`-`M007` carry the titles of real levels but are small copies ("Find 3 Lost Treasure Chests", "He Stole my
lunch!", "Burning Down the Forest", "Village Masher", "Bride of the Dungeon of Defright" ...). The Cutting Room
Floor lists demo/attract-mode levels that use earlier versions of levels. They are not stages: in
`T0L1M003` the 50-Gobbo bonus script is present but only 14 Gobbos exist.

Note for the mod: `T0L1M003` is in the hook's stage-door table as "Inca Door 5". Inca Village's levels are Save 30
Gobbo Babies (`T4L2`), Up the Waterfall (`T4L3`), Dante's Peak (`T4B1`), Bride of the Dungeon of Defright (`T4L5`)
and Goo Man Chu's Tower (`T4L6`), so that entry is a mis-mapping.

T4L2 babies found in the scripts: 9 placed, 5 in Inca Pots (pots whose variable 0 is set), 4 on the carousel, 2 on
swings, 1 on a block stack, plus those carried by the 6 "Inca Dantini with Baby" and thrown along the 4 baby-chain
objects. The level title and guides give 30.

Source for the guide checks: Robshi's Croc 2 FAQ (PlayStation),
https://www.video-games-museum.com/en/faqs/Playstation/26124-Croc-2.pdf

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
