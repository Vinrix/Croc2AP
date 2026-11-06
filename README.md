#  Croc 2 Archipelago Multiworld Randomizer & Standalone Launcher

[![Release](https://img.shields.io/badge/Release-v1.0.0-brightgreen.svg)](https://github.com/Vinrix/Croc2AP/releases)
[![Archipelago](https://img.shields.io/badge/Archipelago-v0.6.0+-blue.svg)](https://archipelago.gg/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20(Steam%2FPC)-orange.svg)](https://store.steampowered.com/)
[![CI/CD](https://img.shields.io/badge/CI%2FCD-GitHub%20Actions-blueviolet.svg)](https://github.com/Vinrix/Croc2AP/actions)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

An end-to-end **Archipelago Multiworld Randomizer** and **Self-Contained Standalone Launcher** for **Croc 2: Kingdom of the Gobbos** (PC / Steam).

Featuring memory-hooked item injection, door locking logic, interactive player traps, in-game bottom toast HUD notifications, a visual YAML generator, and a built-in **GitHub Auto-Updater**.

---

##  Key Features

### Standalone Multi-Tab Launcher (`Croc2ArchipelagoLauncher.exe`)
- **Tab 1: Play & Connect**:
  - Direct connection to Archipelago multiworld rooms (Host, Port, Slot Name, Password).
  - One-click launch modes:
    - ** Launch Vanilla**: Boots 100% unmodded Croc 2 with mod hooks cleanly bypassed.
    - ** Launch Solo Rando**: Boots standalone randomizer offline using local `croc2_mod.ini` rules.
    - ** Play Archipelago**: Automatically launches Croc 2 and bridges with the Archipelago room.
    - ** Attach Running**: Connects the launcher and room to an already running instance of Croc 2.
  - Real-time connection badges and check/item counters.
  - Live color-coded Activity Log displaying item sends, receives, and self-checks:
    - *"Vinny got his own Clockwork Gobbo!"* (Solo rando / self-checks)
    - *"Vinny sent Stage Key to Player2 (Inca Village Door 1)!"*
  - ** Built-In Auto-Updater**: One-click check to update the launcher, `.apworld`, and hook DLLs directly from GitHub Releases!

- **Tab 2: Randomizer Settings & YAML Generator**:
  - Configure win conditions, crystal density, shop shuffle, abilities, door locks, and traps.
  - ** One-Click Export**: Saves `<SlotName>.yaml` directly into `C:\ProgramData\Archipelago\Players\` and synchronizes `croc2_mod.ini` for instant multiworld generation!

---

###  In-Game Experience & Mechanics
- **Non-Intrusive Bottom Toast HUD**:
  - Displays stylish item received/sent alerts directly at the bottom of the screen.
  - No disruptive screen freezes or awkward menu overlays.
- **Stage Door Locking & Stage Keys**:
  - Hub stage doors require their respective **Stage Keys** found across the multiworld.
  - Walking up to a locked door displays `🔒 Door Locked: [Level Name] Key Required!`.
- **Swap Meet Pete's General Store Shuffle**:
  - Defaulting to full shop item shuffle, Pete's store becomes an essential hub for multiworld checks!
  - Supports buying other players' progression items with Croc's crystals.
- **Cleaned Item & Location Pools**:
  - Normal Gobbos excluded to preserve stage balance and level logic flow.
  - Colored Crystal HUD collection clamped to prevent phantom pickups.
- **8 Interactive Traps (Flat 10-Second Ill Effects; No Early Removal)**:
  1. **🔥 Fire Breakout Trap**: Instant -1 heart loss with flame animation and knockback recoil!
  2. **🪢 Tail-Knot Trap**: Croc's tail gets tied in a knot, disabling Tail Whack for a flat 10 seconds (cannot be untied early).
  3. **⚓ Lead Boots Trap**: Heavy iron boots completely disable jumping and stomping for a flat 10 seconds.
  4. **🧊 Ice Physics Trap**: Zero ground friction / ice sliding for a flat 10 seconds.
  5. **🔄 Reverse Controls Trap**: Inverts all directional inputs for a flat 10 seconds.
  6. **🌑 Darkness Trap**: Shrouds Croc's vision in a localized black shadow fog for a flat 10 seconds.
  7. **😈 Dantini Ambush**: Spawns an aggressive pack of hostile Dantinis for a flat 10 seconds.
  8. **💰 Pete's Crystal Tax**: Swap Meet Pete levies a portion of Croc's held crystals immediately.

---

##  Quickstart & Installation

### Option A: Automatic One-Click Setup
1. Download the latest **`Croc2Archipelago-vX.X.X.zip`** from [GitHub Releases](https://github.com/Vinrix/Croc2AP/releases).
2. Extract the archive into a folder of your choice.
3. Right-click **`Install_Croc2AP.bat`** and select **Run as Administrator**.
   - Automatically installs `version.dll` and `croc2_mod.ini` into your Croc 2 Steam folder.
   - Automatically installs `croc2.apworld` into `C:\ProgramData\Archipelago\custom_worlds\`.
   - Automatically installs the YAML template into `C:\ProgramData\Archipelago\Players\Templates\`.
4. Double-click **`Croc2ArchipelagoLauncher.exe`** to start!

### Option B: Using the Standalone Launcher
1. Place **`Croc2ArchipelagoLauncher.exe`** in your Croc 2 game directory (or any preferred folder).
2. Open the launcher. It automatically detects `croc2.exe` from default Steam library paths.
3. Switch to the **⚙️ Randomizer Settings & YAML Export** tab, customize your seed rules, and click **💾 Save & Export YAML to Archipelago Players**.
4. Generate your multiworld seed using Archipelago (`Generate.py` or the Archipelago Web Host).
5. Switch to **🎮 Play & Connect**, enter your room host/port/slot, and click **🌐 Play Archipelago**!

---

## 🔄 Built-In Auto-Updater & Release Workflow

### How Players Update
1. In the Launcher, simply click the **🔄 Check for Updates** button in either tab.
2. If a new release is available on GitHub:
   - The launcher displays the new version number and release notes.
   - Clicking **Yes** automatically downloads the new launcher, updated `croc2.apworld`, and `version.dll`.
   - The launcher seamlessly restarts with `--updated` and displays a completion toast!

### How Maintainers Publish Releases
To push changes to GitHub and make them immediately available to all launcher users:

```cmd
:: Quick interactive release (runs automated tests, builds DLLs, builds launcher, tags, and pushes)
publish_release.bat

:: Or specify version and custom release notes via command line:
python publish_release.py --version 1.0.1 -m "Added new features and bug fixes"

:: Or bump patch/minor automatically:
python publish_release.py --bump patch -m "Patch release"
```

The release automation script (`publish_release.py`):
1. Updates the version constant in `src/launcher/UpdateManager.cs`.
2. Compiles `croc2_ap_hook.dll` & `version.dll` via MinGW-w64.
3. Compiles `Croc2ArchipelagoLauncher.exe` via C# 5 compiler (`csc.exe`).
4. Packs `worlds/croc2` into `dist/croc2.apworld`.
5. Runs the automated test suite (`tests/test_croc2_ap.py`).
6. Packages `dist/Croc2Archipelago-v{version}.zip`.
7. Commits to git, tags `v{version}`, and pushes to GitHub.
8. GitHub Actions CI/CD automatically creates the GitHub Release with all downloadable assets!

---

##  Repository Structure

```
Croc2AP/
├── .github/
│   └── workflows/
│       └── build_and_release.yml   # GitHub Actions CI/CD automated release pipeline
├── build/
│   ├── croc2_ap_hook.dll          # MinGW-w64 compiled hook DLL
│   └── version.dll                # Proxy DLL loaded automatically by croc2.exe
├── dist/
│   └── croc2.apworld              # Archipelago World bundle
├── src/
│   ├── hook/
│   │   ├── croc2_ap_hook.cpp      # In-game memory hooks, HUD toasts & TCP server
│   │   ├── croc2_ap_hook.h        # Hook signatures, structs, and game addresses
│   │   └── version.def            # Windows proxy DLL export definitions
│   └── launcher/
│       ├── MainForm.cs            # Standalone Multi-Tab GUI & YAML Generator
│       ├── ApClient.cs            # Archipelago WebSocket client protocol
│       ├── HookBridgeClient.cs    # Local TCP bridge to in-game version.dll
│       ├── UpdateManager.cs       # GitHub Releases API check & auto-updater
│       ├── SettingsManager.cs     # Settings & YAML options serialization
│       ├── GameDetector.cs        # Steam directory auto-discovery
│       ├── Croc2Data.cs           # ID to name / importance mapping table
│       └── MiniJson.cs            # Lightweight JSON serializer
├── templates/
│   └── Croc 2.yaml                # Default Archipelago player options YAML
├── tests/
│   └── test_croc2_ap.py           # Automated test suite (7/7 tests)
├── worlds/
│   └── croc2/                     # Archipelago World implementation package
│       ├── __init__.py            # Main World definition class
│       ├── items.py               # Item definitions & classifications
│       ├── locations.py           # Location checks & village mappings
│       ├── options.py             # Dataclass options for seed generation
│       ├── regions.py             # Level access graph & door pairing logic
│       └── rules.py               # Archipelago logic rules
├── build.bat                      # Standalone launcher compiler script
├── Croc2ArchipelagoLauncher.exe   # Compiled standalone launcher executable
├── croc2_mod.ini                  # Local configuration & rando settings
├── installer.py                   # Python mod installer / uninstaller / status tool
├── Install_Croc2AP.bat            # One-click Windows admin installer
├── Toggle_Vanilla_Mode.bat        # One-click vanilla mode toggler
├── Uninstall_Croc2AP.bat          # One-click clean uninstaller
├── publish_release.py             # Release automation & GitHub sync script
├── publish_release.bat            # One-click release publisher launcher
└── README.md                      # Comprehensive documentation
```

---

##  Configuration (`croc2_mod.ini`)

Located in the Croc 2 game directory alongside `croc2.exe`:

```ini
[ArchipelagoMod]
enabled = true              ; Set to false to instantly force 100% vanilla Croc 2
port = 43825                ; Local TCP port for launcher communication
toggle_hotkey_vk = 0x79     ; F10: Toggle mod hooks in-game
menu_hotkey_vk = 0x78       ; F9: Show HUD toast status
fire_trap_tick_ms = 1500    ; Fire trap timer tick interval

[ServerConnection]
server_host = archipelago.gg
server_port = 38281
slot_name = WillOfTheVinny
password =

[RandomizerSettings]
lock_level_access = true    ; Require Stage Keys to enter village doors
shuffle_white_crystals = 3  ; 0=none, 1=milestones_50, 2=milestones_10, 3=all_100
shuffle_pete_store = 1      ; 0=vanilla, 1=shuffle_shop_items (default)
shuffle_abilities = true    ; Tail Whack, Butt Stomp, Power Gloves, etc.
shuffle_levers = true       ; Village gates and bridge levers
shuffle_vehicles = true     ; Boat, Minecart, Submarine, Balloon, Hang Glider
shuffle_plane_guns = true   ; Plane turret weapons
door_shuffle = 1            ; 0=off, 1=within_village, 2=any_village
goal = 0                    ; 0=Dante, 1=Jigsaws, 2=Trophies, 3=Mastery
jigsaw_pieces_required = 4  ; Jigsaw count needed to unlock Dante's portal
trophies_required = 12      ; Trophy count needed for win condition
trap_weight = 15            ; Percentage weight of traps in item pool
fire_breakout_trap = true   ; Enable interactive Fire Breakout trap
death_link = false          ; Share deaths across multiworld
```

---

##  Testing & Verification

Run the automated verification suite anytime:

```cmd
C:\msys64\usr\bin\python.exe -m unittest tests/test_croc2_ap.py
```

Verifies:
- Item & Location ID parity between `.apworld` and C++ hook DLL.
- Door pairing algorithm & Stage Key access rules.
- White crystal granularity and shop item shuffle modes.
- INI reader/writer synchronization.
- Archipelago world generation logic and reachability.

---

##  License

This project is licensed under the [MIT License](LICENSE).
Croc 2: Kingdom of the Gobbos is copyright Argonaut Software & Fox Interactive. This mod is an independent open-source reverse-engineering project created for educational and preservation purposes.

