# Croc 2 Multiworld Setup Guide

## Required Software
- **Croc 2** (Steam / PC x64 release at `C:\Program Files (x86)\Steam\steamapps\common\Croc 2`)
- **Archipelago** (v0.6.3 or newer)
- **`croc2.apworld`** placed in `C:\ProgramData\Archipelago\custom_worlds\`
- **`version.dll` (`croc2_ap_hook.dll`)** and **`croc2_mod.ini`** placed in your `Croc 2` installation folder

## Zero-Friction Vanilla Mode
You never have to uninstall or rename files to play vanilla Croc 2:
1. **Automatic Standby**: When `Croc 2 Client` is not connected, the hook DLL remains dormant and Croc 2 plays 100% vanilla.
2. **In-Game Hotkey**: Press `F10` at any time to toggle between **Archipelago Randomizer Mode** and **Vanilla Mode**.
3. **Config File**: Set `enabled = false` in `croc2_mod.ini` or pass `-vanilla` in Steam Launch Options.

