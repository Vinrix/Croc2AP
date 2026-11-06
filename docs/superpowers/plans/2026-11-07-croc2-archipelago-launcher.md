# Croc 2 Archipelago Standalone Launcher Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

## Overview
Build a self-contained Windows desktop launcher for Croc 2 (`Croc2ArchipelagoLauncher.exe`) matching the architecture and design of the Jak 3 launcher (`Jak3ArchipelagoLauncher.exe`). The launcher provides 3 distinct launch modes:
1. **Launch Vanilla**: Disables mod hooks in `croc2_mod.ini` or passes `-vanilla` and launches unmodded Croc 2.
2. **Launch with Rando (Solo)**: Enables the mod for standalone/offline randomizer play.
3. **Play Archipelago**: Collects server room connection details (Host, Port, Slot Name, Password), connects directly to the Archipelago room over WebSocket, launches `croc2.exe` (or attaches to a running instance), connects to the in-game hook DLL (`croc2_ap_hook.dll` on `127.0.0.1:43825`), relays checks and items bi-directionally, and displays everything in an in-launcher color-coded Activity Log while preserving all in-game toast notifications.

## Architecture & Technology Stack
- **Language / Framework**: C# on .NET Framework 4.5+ (using `C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe`).
- **Dependencies**: Zero external NuGet packages. Uses standard `System.dll`, `System.Core.dll`, `System.Drawing.dll`, `System.Windows.Forms.dll`, and `System.Web.Extensions.dll`.
- **Game Interop**: Communicates with `croc2_ap_hook.dll` via localhost TCP JSON-RPC (`127.0.0.1:43825`).
- **Archipelago Interop**: Native async WebSocket client (`System.Net.WebSockets.ClientWebSocket`) communicating with Archipelago server.
- **In-Game Toasts**: 100% preserved. When the launcher syncs checks or items with `croc2_ap_hook.dll`, the hook DLL triggers its in-game bottom toasts:
  - `Croc sent [item] to [receiver]!`
  - `[sender] sent [item] to Croc!`
  - `Croc got his own [item]!`

---

## File Structure & Proposed Source Modules

```
c:\Users\Werne\Croc2AP\
├── build.bat                                    # One-click compiler script producing Croc2ArchipelagoLauncher.exe
├── Croc2ArchipelagoLauncher.exe                 # Output standalone launcher executable
├── src\
│   └── launcher\
│       ├── Program.cs                           # WinForms application entry point
│       ├── MainForm.cs                          # Main UI Form (Connection, Paths, 3 Launch buttons, Badges, Log)
│       ├── ApClient.cs                          # Archipelago WebSocket client protocol
│       ├── HookBridgeClient.cs                  # Localhost TCP JSON client communicating with croc2_ap_hook.dll
│       ├── GameDetector.cs                      # Game path detection, croc2.exe verification, ini/vanilla toggling
│       ├── SettingsManager.cs                   # Persists host, port, slot, password, remember settings
│       └── MiniJson.cs                          # Lightweight JSON helper using JavaScriptSerializer
└── tests\
    └── test_croc2_ap.py                         # Updated test assertions verifying launcher build & functionality
```

---

## Tasks

### Task 1: Scaffolding, Data Structures & Settings Manager
- [ ] 1.1 Create `src/launcher/MiniJson.cs` providing simple object serialization and deserialization via `System.Web.Script.Serialization.JavaScriptSerializer`.
- [ ] 1.2 Create `src/launcher/SettingsManager.cs` to load and save `launcher_settings.json` (Host, Port, SlotName, Password, RememberSettings, CustomGamePath).
- [ ] 1.3 Create `src/launcher/GameDetector.cs` to locate `croc2.exe` in standard Steam directories (`C:\Program Files (x86)\Steam\steamapps\common\Croc 2`), check for `version.dll` and `croc2_mod.ini`, and configure vanilla vs rando mode prior to launch.

### Task 2: Archipelago WebSocket Protocol Client (`ApClient.cs`)
- [ ] 2.1 Implement `ApClient.cs` using `System.Net.WebSockets.ClientWebSocket`.
- [ ] 2.2 Handle `RoomInfo` packet and send `Connect` with game name `"Croc 2"`, slot name, tags, and password.
- [ ] 2.3 Handle `Connected` packet, extracting `slot_data` (door permutations, crystal mode, abilities).
- [ ] 2.4 Handle `ReceivedItems` packet, queuing new incoming network items.
- [ ] 2.5 Handle `PrintJSON` packet for item sends (`ItemSend`) to record sender, receiver, item name, and importance.
- [ ] 2.6 Expose events: `OnStatusChanged`, `OnLog`, `OnItemReceived`, `OnItemSent`, and `OnDisconnected`.

### Task 3: Hook DLL TCP Client & Synchronization Bridge (`HookBridgeClient.cs`)
- [ ] 3.1 Implement `HookBridgeClient.cs` using `System.Net.Sockets.TcpClient` connecting to `127.0.0.1:43825`.
- [ ] 3.2 Implement methods: `ConnectAsync`, `Disconnect`, `SendCommandAsync`.
- [ ] 3.3 Implement `ConfigureSlotAsync` to push slot data (door destinations, crystal mode, abilities).
- [ ] 3.4 Implement `GiveItemAsync` to deliver received items to Croc 2 (triggering in-game toast `[Sender] sent [Item] to Croc!` or `Croc got his own [Item]!`).
- [ ] 3.5 Implement `NotifyItemSentAsync` to notify Croc 2 of outgoing items (triggering in-game toast `Croc sent [Item] to [Receiver]!`).
- [ ] 3.6 Implement `PollEventsAsync` to query collected location checks from Croc 2 and relay them to Archipelago (`LocationChecks`).

### Task 4: Main UI Form (`MainForm.cs`) & Application Entry (`Program.cs`)
- [ ] 4.1 Implement `src/launcher/Program.cs` with `Application.EnableVisualStyles()`, `Application.SetCompatibleTextRenderingDefault(false)`, and `Application.Run(new MainForm())`.
- [ ] 4.2 Implement `src/launcher/MainForm.cs` layout:
  - **Archipelago Room Details**: Host (default: `archipelago.gg`), Port (default: `38281`), Slot Name, Password, Remember Settings.
  - **Croc 2 Game Path Configuration**: Install Directory textBox, Browse button, status badge for `croc2.exe` and `version.dll`.
  - **Launch & Mode Controls**:
    - `_btnLaunchVanilla` ("🎮 Launch Vanilla"): launches `croc2.exe` with vanilla mode active (`-vanilla` / `enabled = false`).
    - `_btnLaunchSoloRando` ("🎲 Launch with Rando (Solo)"): launches `croc2.exe` with randomizer enabled for offline seed play.
    - `_btnLaunchAp` ("🌐 Play Archipelago (Connect & Launch)"): connects to Archipelago room, launches `croc2.exe`, attaches to Hook TCP, and starts synchronization.
    - `_btnAttach` ("🔗 Attach to Running Game"): connects to an already open `croc2.exe` instance.
    - `_btnDisconnect` ("⏹ Disconnect"): disconnects from Archipelago and Hook.
    - `_btnSyncItems` ("🔄 Sync / Replay Items"): re-delivers all Archipelago items.
  - **Status Badges**: AP Server, Game Process, Hook TCP, Checks Sent count, Items Received count.
  - **Activity Log**: Dark-themed `RichTextBox` with color-coded entries:
    - Gold/Yellow: Player names (`Croc`, `WillOfTheVinny`)
    - Green: Connected / Success / Checks sent
    - Orchid/Purple: Progression items
    - Slate Blue: Useful items
    - Cyan: Filler items
    - Red/Salmon: Traps / Errors / Disconnects
    - Clear Log button.
- [ ] 4.3 Implement background polling timer (150ms interval) to bridge AP and Hook states smoothly.

### Task 5: Compilation Script (`build.bat`) & Automated Integration Testing
- [ ] 5.1 Create `build.bat` compiling all `src/launcher/*.cs` via `C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe` into `Croc2ArchipelagoLauncher.exe`.
- [ ] 5.2 Execute `build.bat` and verify that `Croc2ArchipelagoLauncher.exe` is produced with 0 compiler errors.
- [ ] 5.3 Add unit/integration assertions in `tests/test_croc2_ap.py` validating that the launcher executable exists, can parse CLI parameters, and passes automated sanity checks.
- [ ] 5.4 Run full test suite (`python -m unittest tests/test_croc2_ap.py`) to confirm GREEN status.
- [ ] 5.5 Update `installer.py` to also install `Croc2ArchipelagoLauncher.exe` into the Croc 2 Steam folder and provide desktop shortcut options.

