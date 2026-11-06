#!/usr/bin/env python3
"""
Croc 2 Archipelago Client (`Croc2Client.py`)
Bridges the Archipelago Multiworld Server (WebSockets) with `croc2_ap_hook.dll` (TCP 127.0.0.1:43825).
"""

import argparse
import asyncio
import json
import sys
from typing import Any, Dict, Optional

from worlds.croc2.client_bridge import Croc2HookBridge, DEFAULT_HOOK_HOST, DEFAULT_HOOK_PORT, run_client


async def run_diagnostic(host: str, port: int, action: Optional[str]) -> int:
    bridge = Croc2HookBridge(host=host, port=port)
    if not await bridge.connect():
        print(f"[Croc2Client] Could not connect to croc2_ap_hook.dll at {host}:{port} (Game not running or in Vanilla Mode).")
        return 1

    if action == "toggle":
        resp = await bridge.send_command({"command": "toggle_mod"})
        print("[Croc2Client] Toggle Response:", json.dumps(resp, indent=2))
    elif action == "flip":
        resp = await bridge.send_command({"command": "player_action", "action": "ACTION_FRONT_FLIP"})
        print("[Croc2Client] Front Flip Response:", json.dumps(resp, indent=2))
    else:
        events = await bridge.poll_events()
        print("[Croc2Client] Connected State:", json.dumps(bridge.last_state, indent=2))
        print("[Croc2Client] Pending Events:", json.dumps(events, indent=2))

    await bridge.disconnect()
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Croc 2 Archipelago Client & Hook Connector")
    parser.add_argument("--connect", help="Archipelago server address (e.g. localhost:38281)", default=None)
    parser.add_argument("--password", help="Archipelago server password", default=None)
    parser.add_argument("--hook-host", help="Local hook DLL TCP host", default=DEFAULT_HOOK_HOST)
    parser.add_argument("--hook-port", help="Local hook DLL TCP port", type=int, default=DEFAULT_HOOK_PORT)
    parser.add_argument("--status", action="store_true", help="Query live status from croc2_ap_hook.dll and exit")
    parser.add_argument("--toggle-vanilla", action="store_true", help="Toggle Randomizer/Vanilla mode on running game")
    parser.add_argument("--flip", action="store_true", help="Send ACTION_FRONT_FLIP to extinguish Fire Breakout Trap")

    args, unknown = parser.parse_known_args()

    if args.status:
        return asyncio.run(run_diagnostic(args.hook_host, args.hook_port, None))
    if args.toggle_vanilla:
        return asyncio.run(run_diagnostic(args.hook_host, args.hook_port, "toggle"))
    if args.flip:
        return asyncio.run(run_diagnostic(args.hook_host, args.hook_port, "flip"))

    run_client(*sys.argv[1:])
    return 0


if __name__ == "__main__":
    sys.exit(main())

