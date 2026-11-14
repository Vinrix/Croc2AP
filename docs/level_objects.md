# Level objects and crystal sources

Notes from reading the level files (`original/crocii/*.WAD`) with `tools/wad_objects.py` and
`tools/strat_disasm.py`. Everything here comes from the game data; items marked "inferred" still want an
in-game check.

## Placement records

Each level file has one table of 64-byte records, one per placed object (16 little-endian int32s):

| Field | Meaning |
|---|---|
| 0, 1 | usually 0 |
| 2 | flags |
| 4, 5, 6 | x, y, z position |
| 8 | type: offset of the object's script entry point from the file's script base |
| 9 | number of saved-state slots |
| 10 | offset of the object's saved state in the level (4 x field 9 apart; -1 = no saved state) |
| 11 | number of script variables |
| 12 | small number |
| 13 | flags |
| 15 | offset of the object's parameter block (-1 = none) |

Field 10 is a ready-made permanent ID for a collectable: it's unique in the level and the game uses it itself.

`tools/wad_objects.py` finds the table as the longest run of records that pass range checks on these fields (it
returns them in an older frame that starts 16 bytes earlier; see the file). The remaster builds its map header in
memory, so there is no count to read from the file. The result was audited on every map: 3,094 records in total,
and in no gameplay map does the record just before or just after the table look like a placement record. Three
earlier versions of the check each dropped real records (a "field 5 is zero" rule cut the train level's table
short, a lower limit on the type hid the two race maps' tables, and a "field 0 is zero" rule dropped the first
record of two maps).

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
| T1B1 | Soveena The Squid | 20 | 6 | 50 when Soveena is beaten | 100 |
| T1B2 | Cannon Boat Keith | 35 | 3 | 50 when Keith is beaten | 100 |
| T3B1 | Venus Fly Von-Trappe | 0 | 10 | In the fight: 10 from Venus's script per stage and 20 from a second object (amounts from the scripts; 3 stages + 20 = 50 is inferred) | 100? |
| T3B2 | The Village Masher | 0 | 10 | 50 when the Masher is beaten | 100 |
| T2B1 | Flavio the Thermal Fish | 0 | 0 | 50 when game value 91 is 0, otherwise 25, each time the award runs (50 + 25 + 25 over three hits is inferred) | 100? |
| T2B2 | Lava Lamp Larry | 0 | 0 | 100 when Larry is beaten | 100 |
| T4B1, T4B2 | Dante's Peak, Dante's Final Fight | 0 | 0 | no crystal award in their scripts | 0 |

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

## Other collectables

Counted from the placed records of every map, named by each object's own script (`tools/strat_disasm.py`).
The demo copies (`T0L1`) are left out.

| Map | Level | Coloured crystals | Hearts | Keys | Golden Gobbo | Clockwork Gobbo | Other |
|---|---|---|---|---|---|---|---|
| T1L2 | Find the Key! Save the Gobbo! | 5 | 4 | 1 | 1 | 1 | |
| T1L3 | Find 5 Lost Treasure Chests | 5 | 4 | | 1 | 1 | |
| T1L4 | Get the Gobbo's Sandwich! | 5 (`Gobbo bonus crystal`) | 4 | 1 | 1 | 1 | |
| T1L5 | Croc Vs Dantini Boat Race | 5 | | | 1 special | | |
| T1L6 | Save the Bird from the Thief | 5 | 3 | | 1 | 1 | 1 `Heart Pot` |
| T2L2 | Save the Ice Trapped Gobbos! | 5 | 2 | | 1 | 1 | |
| T2L3 | Chase the Choo Choo Train | 5 | 3 | 1 | 1 | 1 | |
| T2L5 | It's just Snowball Madness! | 5 | 2 (`Snow Heart`) | | 1 special | | |
| T2L6 | Hang Glider Valley | 5 | | | 1 special | | |
| T2L7 | KaBooom! It's Roger Red Ant | 5 | 1 | | 1 | 1 | |
| T3L2 | Find the Wheels in the Jungle! | 5 | 2 | 2 | 1 | 1 | 2 `Wheel` |
| T3L3 | Find the Wheels in the Mine! | 5 | 3 | | 1 | 1 | 2 wheels: `Pushable Wheel Block`, `Dan Cart Wheel Block` |
| T3L4 | Race Day at Goldrock | 5 | | | 1 special (seen in-game; its record's script isn't named) | | |
| T3L5 | Climb The Devil's Tower | 5 | 4 | | 1 | 1 | |
| T4L5 | Bride of the Dungeon of Defright | | | 1 | | | |
| T3L7 | Save 50 Trapped Gobbos! | 5 | 3 | 1 | 1 | 1 | |
| T4L2 | Save 30 Gobbo babies! | 5 | 3 | 3 | 1 | 1 | |
| T4L3 | Up the Waterfall | 5 | 2 | | 1 | 1 | |
| T1S1 | Sailor Tribe - Secret Mine | | 2 | 1 | | | 1 `JigsawPiece` |
| T2S1 | Cossack Tribe - Secret Ice Cap | | | 1 | | | 1 `JigsawPiece` |
| T3S1 | Caveman Tribe - Secret Mine | | 1 | | | | 1 `JigsawPiece` |
| T4S1 | Inca Tribe - Secret Maze | | | | | | 1 `JigsawPiece` |
| T1B1M002 | Soveena The Squid | | 1 | | | | |

Totals: 85 coloured crystals (17 levels x 5), 12 keys (plus 7 in the lost levels), 17 Golden Gobbos (13 `Golden Gobbo` and 4 `Special Golden Gobbo`, the
special ones being in the two races, the snowball level and the hang glider level), 13 Clockwork Gobbos, 44
hearts, 4 wheels, 4 jigsaw pieces, 1 placed heart pot. Bride of the Dungeon of Defright, Goo Man Chu's Tower and
the bosses have no coloured crystals. This matches the FAQ: five coloured crystals in a level give its Golden
Gobbo, all of a village's Golden Gobbos open its Golden Gobbo door, and the secret level behind it holds the
village's jigsaw piece (4 in the game).

The wheels: the jungle level has two `Wheel` objects; in the mine the two wheel blocks each run "give inventory
item" (opcode 172) when opened.

The two race maps are small files, so their script offsets are small; the parser's lower limit on the type field
had been hiding their tables.

### Lost levels

| Map | Title | `secret bonus crystal` | Keys | Other |
|---|---|---|---|---|
| T5L2M001 | Secret Sailor Village | | | 1 `secretbonusegg` |
| T5L2M002-M006 | Sailor Secret 1-5 | 1 each | 1 in Secret 1 | |
| T5L3M001 | Secret Cossack Village | | | 1 `secretbonusegg` |
| T5L3M002-M006 | Cossack Secret 1-5 | 1 each (Secret 3 has it placed twice) | 1 each in Secret 1, 2, 4, 5 | |
| T5L4M001 | Secret Caveman Village | | | 1 `secretbonusegg` |
| T5L4M002-M006 | Caveman Secret 1-5 | 1 each | 1 each in Secret 1 and 3 | |

16 `secret bonus crystal` records for 15 levels. A crystal's colour is its variable 0, and the five levels of a
village use colours 0-4 in order. Cossack Secret 3 has two records, both colour 2: one inside a glass cage
(`DODGlassCage1`) and one among the crates. Either one gives the level's crystal, so it is one crystal with two
placements; the FAQ describes a single crystal at the end of that level.

### Village hubs

| Map | Objects of interest |
|---|---|
| T1L1M001 Sailor Village | `Hub 1 Heart Pot`, `trampoline`, `Gobbo Says`, `Reward Gobbo`, `Throwable Gobbo Pickup`, 1 `Smash Box toolbox` |
| T2L1M001 Cossack Village | Snowman parts: `Hub2Snowmanarm` x2, `Hub2SnowmanCarrot`, `Hub2SnowmanHat` (FAQ: 100 crystals for all four) |
| T3L1M001 Caveman Village | Ginger soda parts: `Hub3FizzBox`, `Hub3GingerRoot`, `Hub3SugarBox`, `Hub3Bucket` |
| T4L1M001 Inca Village | `BurpingGobbo`, `Hub4Bowl` (FAQ: win the burping contest for 100 crystals) |

### Which collectables have a save slot

A save slot (record field 14) is the game's own per-object memory and makes a ready ID.

| Has a save slot | No save slot (tracked some other way) |
|---|---|
| `crystal` (1 slot), Smash Boxes (2), `ring of crystals` (2), coloured crystals (2; `secret bonus crystal` 1), `Heart` / `Snow Heart` (1), `Heart Pot` (1), `ClockworkGobbo` (1), `Wheel` (1) | `Key`, `Golden Gobbo`, `Special Golden Gobbo`, `JigsawPiece`, `secretbonusegg`, `Hub 1 Heart Pot` |

For the ones without a slot, the record's position in the table (or its map and position) can serve as the ID.

### Is anything missing?

Named objects come straight from each script's own name, so the only place a collectable could hide is among
the objects whose scripts don't name themselves (258 of 3,084 in the maps that load). Each of those was checked
for pickup opcodes (171 heart pot, 172 give inventory item, 173 gain crystal, 248 Clockwork Gobbo, 249 key, 250
Gobbo, 270 binoculars) and for a save slot. What turned up:

- The race-finish script (100 / 70 / 50) in both race maps, and the boss award in Venus's second map.
- The lost levels' cage object, which handles the coloured-crystal pickup.
- Objects with a save slot but no pickup code: hub level doors, save gongs, the race maps' track markers, and
  level machinery (train, mine and fire-level pieces).
- Everything else is scenery: static models, falling-platform spawners, and the three item slots in Swap Meet
  Pete's shop maps (`T1L1M004` and the other villages' `L1M004`).

## Every pickup, traced

`strat_disasm.trace()` follows a script's control flow: jumps (132, 54), conditional jumps (130, 128, 131, 129),
calls and returns (126, 125, 127), the end of a script (133), switch tables (137) and registered event handlers
(79). The opcode set comes from scanning all 468 handlers in `croc2.exe` for code that moves the script position.
Argument counts come from `tools/opcode_args.py`, which also looks inside helpers that are handed the script
(19 opcodes read their inline words that way), and opcode 9 (a debug print) is decoded as variable-length.

Running it from every placed object and everything those objects spawn reaches 401 pickup instructions. What
the pickup opcodes really do, read from their handlers:

| Opcode | Effect | The hook's header calls it |
|---|---|---|
| 171 | add a heart pot | heart pot |
| 172 `<type>` | give an inventory item of that type | inventory item |
| 173 | gain crystals: the value pushed before it is the amount (0 = one white crystal; 1-5 = that coloured crystal; larger = that many) | gain crystal |
| 248 | add 1 to the counter at `0xBA9B38`; run only by `JigsawPiece` | "clockwork" |
| 249 | add 1 to the counter at `0xBA9B30`, then the first-Golden-Gobbo achievement check; run only by `Golden Gobbo` and `Special Golden Gobbo` | "key" |
| 250 | add 1 to the counter at `0xBA9B34`; no traced script runs it | "gobbo" |
| 270 | add 100 crystals to the wallet (`0xBA9A9C`) if `0xBA9B7C` is set; run by hub reward characters | "binoculars" |

Inventory item types given by opcode 172:

| Type | Given by | Item |
|---|---|---|
| 2 | an object in Sailor Village (`T1L1M001`) | binoculars (seen in-game from the crate) |
| 3 | `Key` (19 placed) | key |
| 5, 6, 7, 8 | Swap Meet Pete | strawberry, lime and orange jelly; Clockwork Gobbo |
| 10 | `Wheel` (2 in `T3L2`) and the object the two mine wheel blocks spawn (`T3L3`) | racer wheel |
| 12 | `throwable gobbo 2` in `T1L4` | not identified |
| 14 | `FFRefill`, `Forest Fire Setup` (fire levels) | water |

### Who gives what

| Script | Gives | Placed |
|---|---|---|
| `crystal` | 1 white crystal | 730 |
| crystal holder (unnamed; spawned by Smash Boxes, rings, hoops) | 1 white crystal | spawned |
| `bonus crystal`, `Gobbo bonus crystal`, `secret bonus crystal` | the coloured crystal of its colour | 80, 5, 16 |
| `inca baby` | 2 white crystals on each of its five exits | 9 placed, the rest spawned |
| chest object spawned by `SeeSaw Chest Count` (`T1L3`) | 10 white crystals | spawned |
| `F-Fire gobbo`, `F-Fire rescue me gobbo` | 1 white crystal | 45, 5 |
| `ForestFireGobboCheck` | 50 white crystals | 1 |
| `Talking Gobbo 1` (`T4L5`) | its reward variable, 20 | 5 |
| `HangGlider` | landing bonus, 10 or 5 | 1 |
| race-finish script (unnamed, `T1L5`, `T3L4`) | 100, 70 or 50 | 1 each |
| `Soveena`, `CBKHimself`, `VMMonster` | 50 | bosses |
| `IceCube` (Lava Lamp Larry) | 100 | boss |
| `FlavioSandBag` | 50 or 25 | boss |
| `VenusVonFlyTrappe`, and an unnamed object in `T3B1M002` | 10, and 20 | boss |
| `Key` | item 3 | 19 |
| `Wheel`, mine wheel blocks | item 10 | 2 + 2 |
| `Heart Pot` (`T1L6`), `Hub 1 Heart Pot` (`T1L1`) | a heart pot | 1, 1 |
| `JigsawPiece` | opcode 248 | 4 |
| `Golden Gobbo`, `Special Golden Gobbo` | opcode 249 | 13, 4 |
| `Reward Gobbo`, `Trampoline Gobbo` (Sailor Village) | 100 crystals to the wallet | hub |
| `Hub Snowman Kids` (Cossack Village) | 100 crystals to the wallet | hub |
| `Gobbo King 3` (Caveman Village) | 100 crystals to the wallet | hub |
| `BurpingGobbo` (Inca Village) | 100 crystals to the wallet | hub |
| `Inca Baby Bouncer` (`T4L2`) | 100 crystals to the wallet | spawned |
| `SMP Croc` (Swap Meet Pete, all four shops) | see below | 1 per shop |

Hearts (`Heart`, `Snow Heart`) and Clockwork Gobbos (`ClockworkGobbo`) run none of these opcodes: a heart heals and
a Clockwork Gobbo marker starts its section, so they are found by name, not by pickup code.

### Swap Meet Pete's shop

The same script runs in all four shop maps (`T1L1M004`, `T2L1M004`, `T3L1M004`, `T4L1M004`). For each item it
checks how many Croc holds (the cap is 9), compares the wallet with the price, then gives the item:

| Item | How it's given | Price in the script | FAQ |
|---|---|---|---|
| Orange jelly | item 7 | 40 | 40 |
| Lime jelly | item 6 | 60 | 60 |
| Strawberry jelly | item 5 | 80 | 80 |
| Clockwork Gobbo | item 8 | 50 | 50 |
| Heart pot | opcode 171 | 250 | 250 |

Each shop map also has three unnamed objects that are the item display slots.

### What the tracer doesn't reach

A scan for pickup opcodes that no trace reaches finds two kinds of thing, neither a missed collectable:

- Dead copies of shared code. The coloured-crystal collector and the crystal holder are compiled into every map,
  including bosses and secret levels that have no coloured crystals or boxes.
- Coincidences: a jump offset or variable index that happens to equal an opcode number (four were checked by hand).

