# Achievements as Archipelago locations (proposal)

Status: **research + logging only.** The hook logs every achievement the game awards (with
`debug_watch = 1`), but nothing is sent to Archipelago yet. This file collects what is known so the
feature can be built once a test log confirms the hook fires as expected.

## Why

The apworld's 15 "Trophy Challenge" locations (one per stage) and 16 "Trophy" items were removed or
kept item-only in this branch: `croc2.exe` has no trophy concept, so nothing could ever trigger those
locations. The "trophies" people know from PlayStation are achievements, and the PC remaster has a
full achievement system that is easy to detect reliably.

## What the game has

38 achievement IDs (from `remaster/ui/achievements/icons/x260/*.png` and strings in `croc2.exe`).
`allAchievements` is the "unlock everything" one (the PlayStation platinum equivalent), leaving the
37 that Steam shows.

| Group | IDs |
|---|---|
| Bosses (8) | defeatSoveena, defeatKeith, defeatFlavio, defeatLarry, defeatTrappe, defeatMasher, defeatDantePeak, defeatDanteFinal |
| Bosses on hard (10) | hardSoveena, hardKeith, hardFlavio, hardLarry, hardTrappe, hardMasher, hardRodger, hardGoo, hardDantePeak, hardDanteFinal |
| Collecting (11) | firstPot, allPots, firstGoldenGobbo, firstClassicGoldenGobbo, allJigsaws, allEggs, firstHundredLevelCrystals, manyCrystals, maxBonus, useFirstClockwork, useFirstJelly |
| Other (8) | completeLevel, firstPurchase, aceBoating, buildSnowman, highTrampoline, makeGingerSoda, simmonSez, winBurpingContest |
| Meta (1) | allAchievements |

The display names and descriptions aren't in `remaster/localization/achievements.csv` (that only
has UI strings); they come from Steam. Look up the conditions for each before writing logic rules.

## How the game awards them

- `0x14025FF00` `AchievementManager::SetAchievementValue(const std::string* id, int value)`
- `0x14025F900` `AchievementManager::IncrementAchievement(const std::string* id, int value)`
- `id` is an MSVC `std::string`: characters inline unless capacity (`+0x18`) >= 16, then `+0x0` is a
  pointer; size at `+0x10`.
- Both have the same 18-byte prologue (`55 56 57 48 81 EC 40 01 00 00 48 8D AC 24 80 00 00 00`), with
  no RIP-relative instructions, so they are detoured like the other inline hooks
  (`Hooked_SetAchievement` / `Hooked_IncrementAchievement` in `croc2_ap_hook.cpp`).
- `0x140260DE0`, `0x140260F90`, `0x140261060` and `0x140260DD0` were labelled as sound-effect functions
  (`RVA_FN_PLAY_SFX_*`). They are the game's achievement checks that its pickup handlers tail-call
  (`0x260DE0` sets `firstGoldenGobbo`, for example). The mod called them as "sounds" when it swallowed
  key, clockwork and binocular pickups, which could unlock real Steam achievements. Fixed in this
  branch: they're renamed `RVA_FN_ACH_CHECK_*` (documentation only, never called), and the pickup hooks
  use the game's real HUD popups (`0x41530` key, `0x414F0` clockwork).

## Open questions for the test log

1. Does each achievement fire once (set to 1) or repeatedly (increment, or set on every save load)?
   The hook must only claim the location on the first 0 -> unlocked transition per save.
2. Do already-unlocked Steam achievements still call the setter? If they do, the setter call alone is
   a fine signal. If they don't, a player who already owns an achievement could never send its
   location, and detection must also check the underlying game condition.
3. Are the `hard*` ones only reachable on a hard difficulty run?

## Suggested apworld design

- New location category "Achievements", one location per ID, named after the Steam display name.
- Exclude `allAchievements` (it depends on every other one).
- Option `include_hard_achievements` (default off) for the 10 `hard*` bosses.
- Logic: boss achievements require the same as the boss entrance; collecting ones need their
  stage/region rules (e.g. `allJigsaws` needs all four secret levels).
- `trophy_hunt` goal: count achievements earned instead of Trophy items, or keep Trophy items and
  place them on achievement locations. Either works; the first is simpler for players to understand.
- Hook side: in `Hooked_SetAchievement`, map the ID to its location and call a claim function
  backed by a new persisted bitmask (38 bits; the save footer still has spare room after
  `extra_checks`).
