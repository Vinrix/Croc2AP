"""
Croc 2 Archipelago Client Bridge (`Croc2Context` & TCP JSON-RPC Connector).
Bridges the Archipelago Multiworld Server (WebSockets) with `croc2_ap_hook.dll` (TCP 127.0.0.1:43825).
"""

import asyncio
import json
import logging
from typing import Any, Dict, List, Optional, Set

from .items import ITEM_ID_TO_NAME, ITEM_NAME_TO_DATA
from .locations import LOCATION_ID_TO_NAME

logger = logging.getLogger("Croc2Client")

DEFAULT_HOOK_HOST = "127.0.0.1"
DEFAULT_HOOK_PORT = 43825

# Archipelago NetworkItem.flags bits
AP_FLAG_PROGRESSION = 0b001
AP_FLAG_USEFUL = 0b010
AP_FLAG_TRAP = 0b100


def classification_from_flags(flags: int) -> str:
    """Map Archipelago NetworkItem flags to the Text Client importance colour class."""
    if flags & AP_FLAG_TRAP:
        return "trap"
    if flags & AP_FLAG_PROGRESSION:
        return "progression"
    if flags & AP_FLAG_USEFUL:
        return "useful"
    return "filler"


class Croc2HookBridge:
    """Asynchronous JSON-RPC over localhost TCP client communicating with `croc2_ap_hook.dll`."""

    def __init__(self, host: str = DEFAULT_HOOK_HOST, port: int = DEFAULT_HOOK_PORT) -> None:
        self.host = host
        self.port = port
        self.reader: Optional[asyncio.StreamReader] = None
        self.writer: Optional[asyncio.StreamWriter] = None
        self.connected: bool = False
        self.checked_locations: Set[int] = set()
        self.current_wad: str = "T1L1M001"
        self.last_state: Dict[str, Any] = {}
        # Serialises request/response pairs so concurrent callers never collide on reader.readline()
        self._lock = asyncio.Lock()

    async def connect(self) -> bool:
        try:
            self.reader, self.writer = await asyncio.open_connection(self.host, self.port)
            self.connected = True
            resp = await self.send_command({"command": "client_hello", "version": "0.1.0"})
            return resp is not None
        except OSError:
            self.connected = False
            self.reader = None
            self.writer = None
            return False

    async def disconnect(self) -> None:
        if self.writer is not None:
            try:
                self.writer.close()
                await self.writer.wait_closed()
            except OSError:
                pass
        self.connected = False
        self.reader = None
        self.writer = None

    async def send_command(self, payload: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        async with self._lock:
            if not self.connected or self.writer is None or self.reader is None:
                return None
            try:
                line = json.dumps(payload) + "\n"
                self.writer.write(line.encode("utf-8"))
                await self.writer.drain()
                resp_line = await asyncio.wait_for(self.reader.readline(), timeout=2.0)
                if not resp_line:
                    await self.disconnect()
                    return None
                return json.loads(resp_line.decode("utf-8"))
            except (OSError, asyncio.TimeoutError, json.JSONDecodeError, RuntimeError):
                await self.disconnect()
                return None

    async def configure_slot(self, slot_data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        door_map = slot_data.get("door_map", {})
        for source_door, target_wad in door_map.items():
            await self.send_command({
                "command": "set_door_destination",
                "source_door": source_door,
                "target_wad": target_wad,
            })
        return await self.send_command({
            "command": "configure_session",
            "colored_crystals_mode": slot_data.get("colored_crystals_mode", 1),
            "shuffle_white_crystals": slot_data.get("shuffle_white_crystals", 1),
            "shuffle_abilities": slot_data.get("shuffle_abilities", 1),
            "shuffle_levers": slot_data.get("shuffle_levers", 1),
            "shuffle_vehicles": slot_data.get("shuffle_vehicles", 1),
            "shuffle_plane_guns": slot_data.get("shuffle_plane_guns", 1),
            "shuffle_pete_store": slot_data.get("shuffle_pete_store", 2),
        })

    async def give_item(
        self,
        item_id: int,
        sender: str = "",
        location: str = "",
        classification: str = "",
        item_index: int = -1,
    ) -> Optional[Dict[str, Any]]:
        """Deliver an item into croc2.exe. The DLL writes it into the live inventory wheel / HUD."""
        item_name = ITEM_ID_TO_NAME.get(item_id, f"Unknown ({item_id})")
        item_data = ITEM_NAME_TO_DATA.get(item_name, {})
        return await self.send_command({
            "command": "give_item",
            "item_id": item_id,
            "item_name": item_name,
            "group": item_data.get("group", ""),
            "wad": item_data.get("wad", ""),
            "color": item_data.get("color", ""),
            "sender": sender,
            "location": location,
            "classification": classification or item_data.get("classification", ""),
            "item_index": item_index,
        })

    async def notify_item_sent(
        self,
        item_name: str,
        receiver: str,
        location: str,
        classification: str,
    ) -> Optional[Dict[str, Any]]:
        """Show an outgoing ([OUT]) item in the in-game colour-coded Archipelago feed."""
        return await self.send_command({
            "command": "notify_item_sent",
            "item_name": item_name,
            "receiver": receiver,
            "location": location,
            "classification": classification,
        })

    async def poll_events(self) -> List[Dict[str, Any]]:
        resp = await self.send_command({"command": "poll_events"})
        if not resp:
            return []
        self.last_state = resp.get("state", {})
        events = resp.get("events", [])
        for ev in events:
            if ev.get("event") == "location_checked":
                loc_id = int(ev["location_id"])
                self.checked_locations.add(loc_id)
            elif ev.get("event") == "level_transition":
                self.current_wad = str(ev.get("wad", self.current_wad))
        return events


def run_client(*args: str) -> None:
    """Entrypoint for the Archipelago CommonClient UI when launched from ArchipelagoLauncher."""
    try:
        import CommonClient
        from CommonClient import ClientCommandProcessor, CommonContext, get_base_parser, gui_enabled, server_loop

        class Croc2CommandProcessor(ClientCommandProcessor):
            def _cmd_croc2(self) -> None:
                """Display current Croc 2 Hook DLL connection and inventory status."""
                ctx = self.ctx
                if isinstance(ctx, Croc2Context):
                    logger.info(f"Hook DLL Connected: {ctx.hook.connected} | Current WAD: {ctx.hook.current_wad}")
                    logger.info(f"Hook State: {ctx.hook.last_state}")

            def _cmd_flip(self) -> None:
                """Simulate Croc performing a front flip to extinguish an active Fire Breakout Trap."""
                ctx = self.ctx
                if isinstance(ctx, Croc2Context) and ctx.hook.connected:
                    asyncio.create_task(ctx.hook.send_command({"command": "player_action", "action": "ACTION_FRONT_FLIP"}))
                    logger.info("Sent ACTION_FRONT_FLIP to Croc 2 Hook DLL.")

        class Croc2Context(CommonContext):
            game = "Croc 2"
            items_handling = 0b111
            command_processor = Croc2CommandProcessor

            def __init__(self, server_address: Optional[str], password: Optional[str]) -> None:
                super().__init__(server_address, password)
                self.hook = Croc2HookBridge()
                self.synced_item_index = 0
                self.slot_data: Dict[str, Any] = {}
                self.needs_slot_config = False
                # Outgoing item notifications queued for the in-game feed (drained by croc2_sync_task)
                self.pending_sent_feed: List[Dict[str, str]] = []

            async def server_auth(self, password_requested: bool = False) -> None:
                if password_requested and not self.password:
                    await super().server_auth(password_requested)
                await self.get_username()
                await self.send_connect()

            # ---- name helpers (work across Archipelago versions) ----
            def player_name(self, slot: int) -> str:
                try:
                    if hasattr(self, "slot_info") and slot in self.slot_info:
                        name = getattr(self.slot_info[slot], "name", None)
                        if name:
                            return name
                    if hasattr(self, "player_names") and slot in self.player_names:
                        return self.player_names[slot]
                    return f"Player {slot}"
                except Exception:
                    return f"Player {slot}"

            def item_name_for(self, item_id: int, receiving_slot: int) -> str:
                try:
                    return self.item_names.lookup_in_slot(item_id, receiving_slot)
                except Exception:
                    return ITEM_ID_TO_NAME.get(item_id, f"Item {item_id}")

            def location_name_for(self, loc_id: int, finding_slot: int) -> str:
                try:
                    return self.location_names.lookup_in_slot(loc_id, finding_slot)
                except Exception:
                    return LOCATION_ID_TO_NAME.get(loc_id, f"Location {loc_id}")

            def on_package(self, cmd: str, args: dict) -> None:
                super().on_package(cmd, args)
                if cmd == "Connected":
                    self.slot_data = args.get("slot_data", {}) or {}
                    self.synced_item_index = 0
                    # Configured sequentially by croc2_sync_task (no unguarded concurrent socket use)
                    self.needs_slot_config = True
                elif cmd == "PrintJSON" and args.get("type") == "ItemSend":
                    item = args.get("item")
                    receiving = args.get("receiving")
                    if item is None or receiving is None:
                        return
                    try:
                        finder = int(item.player)
                        item_id = int(item.item)
                        loc_id = int(item.location)
                        flags = int(item.flags)
                    except (AttributeError, TypeError, ValueError):
                        try:
                            finder = int(item[2])
                            item_id = int(item[0])
                            loc_id = int(item[1])
                            flags = int(item[3])
                        except (TypeError, ValueError, IndexError, KeyError):
                            return
                    receiving = int(receiving)
                    # Only show items found in OUR world that belong to someone else here;
                    # items to ourselves are shown as both [OUT] and [IN] by croc2_sync_task.
                    if finder == self.slot and receiving != self.slot:
                        self.pending_sent_feed.append({
                            "item_name": self.item_name_for(item_id, receiving),
                            "receiver": self.player_name(receiving),
                            "location": self.location_name_for(loc_id, finder),
                            "classification": classification_from_flags(flags),
                        })

        async def croc2_sync_task(ctx: Croc2Context) -> None:
            while not ctx.exit_event.is_set():
                if not ctx.hook.connected:
                    if await ctx.hook.connect():
                        logger.info("Connected to Croc 2 Hook DLL on 127.0.0.1:43825 (Randomizer Active)")
                        # Re-send everything so the DLL can rebuild inventory after a game restart
                        ctx.synced_item_index = 0
                        if ctx.slot_data:
                            ctx.needs_slot_config = True
                else:
                    if ctx.needs_slot_config and ctx.slot_data:
                        if await ctx.hook.configure_slot(ctx.slot_data) is not None:
                            ctx.needs_slot_config = False
                            # Sync other player names to the hook DLL
                            names = []
                            if hasattr(ctx, "player_names") and ctx.player_names:
                                names = [n for s, n in ctx.player_names.items() if s != ctx.slot and n]
                            elif hasattr(ctx, "slot_info") and ctx.slot_info:
                                names = [getattr(info, "name", "") for s, info in ctx.slot_info.items() if s != ctx.slot]
                                names = [n for n in names if n]
                            if names:
                                await ctx.hook.send_command({"command": "set_yaml_players", "players": names})

                    # Deliver newly received items. Only advance the index once the DLL confirms receipt,
                    # so nothing is lost if the game is restarting or the socket hiccups.
                    while ctx.hook.connected and ctx.synced_item_index < len(ctx.items_received):
                        net_item = ctx.items_received[ctx.synced_item_index]
                        sender_slot = int(net_item.player)
                        sender = ctx.player_name(sender_slot)
                        location = ctx.location_name_for(int(net_item.location), sender_slot)
                        if int(net_item.location) < 0:
                            location = "Starting Inventory" if int(net_item.location) == -2 else "Server"
                        classification = classification_from_flags(int(net_item.flags))

                        if sender_slot == ctx.slot and int(net_item.location) >= 0:
                            # Single-player / self-found item: show the [OUT] side as well
                            await ctx.hook.notify_item_sent(
                                ctx.item_name_for(int(net_item.item), ctx.slot),
                                ctx.player_name(ctx.slot),
                                location,
                                classification,
                            )

                        resp = await ctx.hook.give_item(
                            int(net_item.item),
                            sender=sender,
                            location=location,
                            classification=classification,
                            item_index=ctx.synced_item_index,
                        )
                        if resp is None:
                            break
                        ctx.synced_item_index += 1

                    # Outgoing items to other players
                    while ctx.hook.connected and ctx.pending_sent_feed:
                        entry = ctx.pending_sent_feed[0]
                        if await ctx.hook.notify_item_sent(**entry) is None:
                            break
                        ctx.pending_sent_feed.pop(0)

                    # Poll checked locations and events from DLL
                    await ctx.hook.poll_events()
                    new_checks = ctx.hook.checked_locations - ctx.checked_locations
                    if new_checks and ctx.server and ctx.slot:
                        await ctx.send_msgs([{"cmd": "LocationChecks", "locations": sorted(new_checks)}])

                await asyncio.sleep(0.25)

        async def main(parsed_args) -> None:
            ctx = Croc2Context(parsed_args.connect, parsed_args.password)
            ctx.server_task = asyncio.create_task(server_loop(ctx), name="server loop")
            sync_task = asyncio.create_task(croc2_sync_task(ctx), name="croc2 sync")
            if gui_enabled:
                ctx.run_gui()
            ctx.run_cli()
            await ctx.exit_event.wait()
            sync_task.cancel()
            await ctx.shutdown()

        import colorama
        colorama.init()
        parser = get_base_parser(description="Croc 2 Archipelago Client")
        parsed = parser.parse_args(args)
        asyncio.run(main(parsed))
    except ImportError:
        print("Archipelago CommonClient environment not detected. Run Croc2Client.py --standalone-test to test hook connection.")
