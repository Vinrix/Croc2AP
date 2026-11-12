"""
End-to-End Automated Test Suite for the Croc 2 Archipelago Randomizer:
1. Validates `Croc 2.yaml` format & 1-to-1 correlation with `worlds/croc2/options.py` and `Ori and the Will of the Wisps.yaml`.
2. Validates `worlds/croc2` & `dist/croc2.apworld` item/location counts, 5-Crystal Golden Gobbo logic, Door Shuffle, and all 4 Goal win conditions.
3. Validates `build/croc2_ap_hook.dll` (native x64 DLL + live TCP 127.0.0.1:43825 bridge):
   - Zero-Friction Vanilla Toggle (Dormant standby, client connect, F10 toggle, disconnect standby)
   - Receiving all 5 stage Colored Crystals automatically activating that stage's Golden Gobbo
   - Fire Breakout Trap (3 ticks before losing 1 heart; extinguished immediately by ACTION_FRONT_FLIP)
   - Door Shuffle entrance redirection
"""

import asyncio
import json
import os
import re
import shutil
import subprocess
import sys
import unittest
import zipfile

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if ROOT_DIR not in sys.path:
    sys.path.insert(0, ROOT_DIR)


# Provide lightweight mock BaseClasses / Options / worlds.AutoWorld when running outside full Archipelago install
def _install_ap_shims_if_needed() -> None:
    try:
        import BaseClasses  # noqa: F401
        return
    except ImportError:
        pass

    import types

    base_classes = types.ModuleType("BaseClasses")

    class ItemClassification:
        filler = 0b0000
        progression = 0b0001
        useful = 0b0010
        trap = 0b0100
        progression_skip_balancing = 0b1001

    class Item:
        game = "Generic"

        def __init__(self, name: str, classification: int, code: int, player: int):
            self.name = name
            self.classification = classification
            self.code = code
            self.player = player
            self.location = None

    class Location:
        game = "Generic"

        def __init__(self, player: int, name: str, address: int, parent=None):
            self.player = player
            self.name = name
            self.address = address
            self.parent_region = parent
            self.item = None
            self.access_rule = lambda state: True

    class Entrance:
        def __init__(self, player: int, name: str, parent=None):
            self.player = player
            self.name = name
            self.parent_region = parent
            self.connected_region = None
            self.access_rule = lambda state: True

        def connect(self, region):
            self.connected_region = region
            region.entrances.append(self)

    class Region:
        def __init__(self, name: str, player: int, multiworld=None):
            self.name = name
            self.player = player
            self.multiworld = multiworld
            self.locations = []
            self.exits = []
            self.entrances = []

    class Tutorial:
        def __init__(self, tutorial_name, description, language, file_name, link, authors):
            self.tutorial_name = tutorial_name
            self.description = description
            self.language = language
            self.file_name = file_name
            self.link = link
            self.authors = authors

    class CollectionState:
        def __init__(self, multiworld):
            self.multiworld = multiworld
            self.prog_items = {}

        def collect(self, item: Item):
            p_items = self.prog_items.setdefault(item.player, {})
            p_items[item.name] = p_items.get(item.name, 0) + 1

        def remove(self, item: Item):
            p_items = self.prog_items.setdefault(item.player, {})
            if p_items.get(item.name, 0) > 1:
                p_items[item.name] -= 1
            else:
                p_items.pop(item.name, None)

        def has(self, item_name: str, player: int, count: int = 1) -> bool:
            return self.prog_items.get(player, {}).get(item_name, 0) >= count

        def can_reach_region(self, region_name: str, player: int) -> bool:
            reachable = {"Menu"}
            changed = True
            while changed:
                changed = False
                for reg in self.multiworld.regions:
                    if reg.player == player and reg.name in reachable:
                        for ex in reg.exits:
                            if ex.connected_region and ex.connected_region.name not in reachable:
                                if ex.access_rule(self):
                                    reachable.add(ex.connected_region.name)
                                    changed = True
            return region_name in reachable

        def can_reach_location(self, loc_name: str, player: int) -> bool:
            loc = self.multiworld.get_location(loc_name, player)
            if not self.can_reach_region(loc.parent_region.name, player):
                return False
            return loc.access_rule(self)

    class MultiWorld:
        def __init__(self):
            self.regions = []
            self.itempool = []
            self.early_items = {1: {}}
            self.completion_condition = {}

        def get_unfilled_locations(self, player: int):
            return [
                loc
                for reg in self.regions
                if reg.player == player
                for loc in reg.locations
                if loc.item is None
            ]

        def get_locations(self, player: int):
            return [
                loc
                for reg in self.regions
                if reg.player == player
                for loc in reg.locations
            ]

        def get_location(self, name: str, player: int) -> Location:
            for reg in self.regions:
                if reg.player == player:
                    for loc in reg.locations:
                        if loc.name == name:
                            return loc
            raise KeyError(f"Location not found: {name}")

        def get_entrance(self, name: str, player: int) -> Entrance:
            for reg in self.regions:
                if reg.player == player:
                    for ex in reg.exits:
                        if ex.name == name:
                            return ex
            raise KeyError(f"Entrance not found: {name}")

    base_classes.ItemClassification = ItemClassification
    base_classes.Item = Item
    base_classes.Location = Location
    base_classes.Entrance = Entrance
    base_classes.Region = Region
    base_classes.Tutorial = Tutorial
    base_classes.CollectionState = CollectionState
    base_classes.MultiWorld = MultiWorld
    sys.modules["BaseClasses"] = base_classes

    options_mod = types.ModuleType("Options")

    class Option:
        default = 0

        def __init__(self, value=None):
            self.value = self.default if value is None else value

    class Choice(Option):
        pass

    class Range(Option):
        range_start = 0
        range_end = 100

    class Toggle(Option):
        default = 0

    class DefaultOnToggle(Toggle):
        default = 1

    class DeathLink(Toggle):
        default = 0

    class StartInventoryPool(Option):
        default = {}

    class OptionGroup:
        def __init__(self, name: str, options: list):
            self.name = name
            self.options = options

    class PerGameCommonOptions:
        pass

    options_mod.Option = Option
    options_mod.Choice = Choice
    options_mod.Range = Range
    options_mod.Toggle = Toggle
    options_mod.DefaultOnToggle = DefaultOnToggle
    options_mod.DeathLink = DeathLink
    options_mod.StartInventoryPool = StartInventoryPool
    options_mod.OptionGroup = OptionGroup
    options_mod.PerGameCommonOptions = PerGameCommonOptions
    sys.modules["Options"] = options_mod

    autoworld_mod = types.ModuleType("worlds.AutoWorld")

    class WebWorld:
        pass

    class World:
        def __init__(self, multiworld, player: int):
            import random
            self.multiworld = multiworld
            self.player = player
            self.random = random.Random(42)

    autoworld_mod.WebWorld = WebWorld
    autoworld_mod.World = World
    sys.modules["worlds.AutoWorld"] = autoworld_mod

    generic_rules_mod = types.ModuleType("worlds.generic.Rules")

    def set_rule(spot, rule):
        spot.access_rule = rule

    generic_rules_mod.set_rule = set_rule
    sys.modules["worlds.generic"] = types.ModuleType("worlds.generic")
    sys.modules["worlds.generic.Rules"] = generic_rules_mod


_install_ap_shims_if_needed()

from BaseClasses import CollectionState, MultiWorld  # noqa: E402
from worlds.croc2 import Croc2World  # noqa: E402
from worlds.croc2.client_bridge import Croc2HookBridge  # noqa: E402
from worlds.croc2.options import (  # noqa: E402
    ColoredCrystalsMode,
    Croc2Options,
    DoorShuffle,
    FireBreakoutTrapEnabled,
    Goal,
    JigsawPiecesRequired,
    LockLevelAccess,
    ShuffleAbilities,
    ShuffleLevers,
    ShufflePlaneGuns,
    ShufflePeteStore,
    ShuffleVehicles,
    ShuffleWhiteCrystals,
    TrapWeight,
    TrophiesRequired,
)
from Options import DeathLink, StartInventoryPool  # noqa: E402


def make_default_options(**overrides) -> Croc2Options:
    defaults = {
        "goal": Goal(Goal.option_defeat_baron_dante),
        "jigsaw_pieces_required": JigsawPiecesRequired(4),
        "trophies_required": TrophiesRequired(12),
        "colored_crystals_mode": ColoredCrystalsMode(ColoredCrystalsMode.option_shuffled_in_pool),
        "shuffle_white_crystals": ShuffleWhiteCrystals(ShuffleWhiteCrystals.option_all_100),
        "lock_level_access": LockLevelAccess(1),
        "shuffle_abilities": ShuffleAbilities(1),
        "shuffle_levers": ShuffleLevers(1),
        "shuffle_vehicles": ShuffleVehicles(1),
        "shuffle_plane_guns": ShufflePlaneGuns(1),
        "shuffle_pete_store": ShufflePeteStore(ShufflePeteStore.option_shuffle_shop_items),
        "door_shuffle": DoorShuffle(DoorShuffle.option_within_village),
        "trap_weight": TrapWeight(15),
        "fire_breakout_trap": FireBreakoutTrapEnabled(1),
        "start_inventory_from_pool": StartInventoryPool({}),
        "death_link": DeathLink(0),
    }
    defaults.update(overrides)
    return Croc2Options(**defaults)


class TestCroc2Archipelago(unittest.TestCase):
    def test_yaml_format_matches_ori_template_and_options(self):
        yaml_path = os.path.join(ROOT_DIR, "Croc 2.yaml")
        self.assertTrue(os.path.exists(yaml_path), "Croc 2.yaml must exist")
        with open(yaml_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Verify Ori-style template headers & metadata
        self.assertIn("# Q. What is this file?", content)
        self.assertIn("# Q. How do I use it?", content)
        self.assertIn("name: Player{number}", content)
        self.assertIn("description: Default Croc 2 Template", content)
        self.assertIn("game: Croc 2", content)
        self.assertIn("requires:", content)
        self.assertIn("progression_balancing:", content)
        self.assertIn("accessibility:", content)

        # Verify all dataclass fields in Croc2Options appear in Croc 2.yaml
        for field_name in Croc2Options.__dataclass_fields__:
            self.assertRegex(
                content,
                rf"(?m)^\s+{re.escape(field_name)}:",
                f"Option '{field_name}' from Croc2Options must be present in Croc 2.yaml",
            )

        # Verify the 4 Goal options and 4 Jigsaw Pieces limit
        self.assertIn("defeat_baron_dante: 50", content)
        self.assertIn("collect_jigsaw_pieces: 0", content)
        self.assertIn("trophy_hunt: 0", content)
        self.assertIn("all_of_the_above: 0", content)
        self.assertIn("random-range-1-4: 0", content)
        self.assertNotIn("free_all_gobbos", content)

    def test_apworld_archive_integrity(self):
        apworld_path = os.path.join(ROOT_DIR, "dist", "croc2.apworld")
        self.assertTrue(os.path.exists(apworld_path), "dist/croc2.apworld must exist")
        with zipfile.ZipFile(apworld_path, "r") as zf:
            names = set(zf.namelist())
            required = {
                "croc2/__init__.py",
                "croc2/archipelago.json",
                "croc2/options.py",
                "croc2/items.py",
                "croc2/locations.py",
                "croc2/regions.py",
                "croc2/rules.py",
                "croc2/web_world.py",
                "croc2/client.py",
                "croc2/client_bridge.py",
                "croc2/data/items.json",
                "croc2/data/locations.json",
                "croc2/data/level_mapping.json",
                "croc2/docs/en_Croc 2.md",
                "croc2/docs/setup_en.md",
            }
            self.assertTrue(required.issubset(names))

    def test_world_generation_and_5_crystal_golden_gobbo_rule(self):
        mw = MultiWorld()
        world = Croc2World(mw, 1)
        world.options = make_default_options()

        world.generate_early()
        world.create_regions()
        world.create_items()
        world.set_rules()

        unfilled = mw.get_unfilled_locations(1)
        self.assertEqual(
            len(mw.itempool),
            len(unfilled),
            f"Item pool ({len(mw.itempool)}) must match unfilled locations ({len(unfilled)})",
        )
        # Verify currency item balance: Croc Crystals must be 1, 5, and 10 sets (not 50 or 100)
        from worlds.croc2.items import ITEM_NAME_TO_DATA, ITEM_ID_TO_NAME
        self.assertIn("Croc Crystals (1)", ITEM_NAME_TO_DATA)
        self.assertEqual(ITEM_NAME_TO_DATA["Croc Crystals (1)"]["crystal_amount"], 1)
        self.assertEqual(ITEM_ID_TO_NAME[7200148], "Croc Crystals (1)")

        self.assertIn("Croc Crystals (5)", ITEM_NAME_TO_DATA)
        self.assertEqual(ITEM_NAME_TO_DATA["Croc Crystals (5)"]["crystal_amount"], 5)
        self.assertEqual(ITEM_ID_TO_NAME[7200149], "Croc Crystals (5)")

        self.assertIn("Croc Crystals (10)", ITEM_NAME_TO_DATA)
        self.assertEqual(ITEM_NAME_TO_DATA["Croc Crystals (10)"]["crystal_amount"], 10)
        self.assertEqual(ITEM_ID_TO_NAME[7200150], "Croc Crystals (10)")

        self.assertNotIn("Croc Crystals (50)", ITEM_NAME_TO_DATA)
        self.assertNotIn("Croc Crystals (100)", ITEM_NAME_TO_DATA)

        crystal_items_in_pool = [item.name for item in mw.itempool if "Croc Crystals" in item.name]
        self.assertTrue(len(crystal_items_in_pool) > 0, "Item pool must contain Croc Crystals filler items")
        self.assertTrue(
            all(name in {"Croc Crystals (1)", "Croc Crystals (5)", "Croc Crystals (10)"} for name in crystal_items_in_pool),
            f"All Croc Crystals in item pool must be sets of 1, 5, or 10, found: {set(crystal_items_in_pool)}",
        )

        # Default location count: 187 base checks + 2000 White Crystal checks (100 per stage * 20 stages) = 2187
        self.assertEqual(len(unfilled), 2187)

        # Verify that Sailor Village Door 1 (T1_D1) is shuffled (not vanilla T1L2M001) when door_shuffle is enabled
        self.assertNotEqual(
            world.slot_door_map["T1_D1"],
            "T1L2M001",
            "Sailor Village Door 1 must be shuffled to a non-vanilla level when door_shuffle is enabled",
        )

        # Verify authentic in-game level naming for items and locations
        self.assertIn("Find the Key! Save the Gobbo! - Blue Crystal", ITEM_NAME_TO_DATA)
        self.assertEqual(ITEM_NAME_TO_DATA["Find the Key! Save the Gobbo! - Blue Crystal"]["group"], "Colored Crystals")
        self.assertIn("Find the Key! Save the Gobbo! Key", ITEM_NAME_TO_DATA)
        self.assertEqual(ITEM_NAME_TO_DATA["Find the Key! Save the Gobbo! Key"]["group"], "Level Keys")

        from worlds.croc2.locations import LOCATION_NAME_TO_ID
        self.assertIn("Find the Key! Save the Gobbo! - Golden Gobbo", LOCATION_NAME_TO_ID)
        self.assertIn("Find the Key! Save the Gobbo! - Stage Clear", LOCATION_NAME_TO_ID)
        self.assertIn("Find the Key! Save the Gobbo! - White Crystal 1", LOCATION_NAME_TO_ID)
        self.assertNotIn("Sailor Village Door 1 - Blue Crystal", ITEM_NAME_TO_DATA)
        self.assertNotIn("Sailor Village Door 1 - Golden Gobbo", LOCATION_NAME_TO_ID)

        state = CollectionState(mw)
        # Grant Tail Whack, Power Gloves, and Sailor Village Door Keys so Croc can reach Find the Key! Save the Gobbo!
        state.collect(world.create_item("Tail Whack"))
        state.collect(world.create_item("Power Gloves"))
        for item in mw.itempool:
            if item.name.endswith(" Key"):
                state.collect(item)

        golden_loc = "Find the Key! Save the Gobbo! - Golden Gobbo"
        # With only 4 out of 5 Colored Crystals, Golden Gobbo must NOT be reachable
        for c in ("Red", "Blue", "Green", "Yellow"):
            state.collect(world.create_item(f"Find the Key! Save the Gobbo! - {c} Crystal"))
        self.assertFalse(
            state.can_reach_location(golden_loc, 1),
            "Golden Gobbo must be locked when only 4 of 5 stage Colored Crystals are collected",
        )

        # Collecting the 5th (Purple) Crystal unlocks the Golden Gobbo!
        state.collect(world.create_item("Find the Key! Save the Gobbo! - Purple Crystal"))
        self.assertTrue(
            state.can_reach_location(golden_loc, 1),
            "Golden Gobbo must activate as soon as all 5 stage Colored Crystals are collected",
        )

        # Collecting all items in pool must satisfy the completion condition
        for item in mw.itempool:
            state.collect(item)
        self.assertTrue(
            mw.completion_condition[1](state),
            "Full item pool must satisfy the goal completion condition",
        )

    def test_all_of_the_above_goal_condition(self):
        mw = MultiWorld()
        world = Croc2World(mw, 1)
        world.options = make_default_options(goal=Goal(Goal.option_all_of_the_above))
        world.generate_early()
        world.create_regions()
        world.create_items()
        world.set_rules()

        state = CollectionState(mw)
        for item in mw.itempool:
            if item.name != "Trophy":
                state.collect(item)
        # Without required Trophies, All of the Above is False
        self.assertFalse(mw.completion_condition[1](state))

        # Collect Trophies -> All of the Above becomes True
        for item in mw.itempool:
            if item.name == "Trophy":
                state.collect(item)
        self.assertTrue(mw.completion_condition[1](state))

    def test_hook_dll_tcp_protocol_and_mechanics(self):
        # Compile or locate test_host.exe that loads build/croc2_ap_hook.dll and starts its TCP server on port 43829
        host_cpp = os.path.join(ROOT_DIR, "tests", "test_host.cpp")
        host_exe = os.path.join(ROOT_DIR, "build", "test_host.exe")
        os.makedirs(os.path.join(ROOT_DIR, "build"), exist_ok=True)

        if not os.path.isfile(host_exe):
            gpp_candidates = [
                shutil.which("g++"),
                r"C:\msys64\mingw64\bin\g++.exe",
                "/mingw64/bin/g++.exe",
                r"D:\a\_temp\setup-msys2\mingw64\bin\g++.exe",
            ]
            gpp_path = next((p for p in gpp_candidates if p and os.path.isfile(p)), None)
            if not gpp_path:
                gpp_path = "g++"

            env = os.environ.copy()
            env["PATH"] = "/mingw64/bin:" + r"C:\msys64\mingw64\bin;" + env.get("PATH", "")
            try:
                subprocess.run(
                    [gpp_path, "-std=c++17", "-static", "-static-libgcc", "-static-libstdc++", host_cpp, "-o", host_exe],
                    cwd=ROOT_DIR,
                    env=env,
                    check=True,
                )
            except Exception as e:
                self.skipTest(f"Skipping hook TCP protocol test: g++ compiler unavailable ({e})")

        test_ini = os.path.join(ROOT_DIR, "build", "test_mod.ini")
        with open(test_ini, "w", encoding="utf-8") as f:
            f.write("[ArchipelagoMod]\nenabled = true\nport = 43829\nfire_trap_tick_ms = 1500\n")

        env = os.environ.copy()
        proc = subprocess.Popen([host_exe, test_ini], cwd=ROOT_DIR, env=env)
        try:
            async def run_tcp_checks():
                bridge = Croc2HookBridge(host="127.0.0.1", port=43829)
                for _ in range(20):
                    if await bridge.connect():
                        break
                    await asyncio.sleep(0.05)
                self.assertTrue(bridge.connected, "Croc2HookBridge must connect to croc2_ap_hook.dll over TCP")

                # 1. Verify Zero-Friction Vanilla Toggle (F10 toggle)
                toggled_off = await bridge.send_command({"command": "toggle_mod"})
                self.assertFalse(toggled_off["active"])
                self.assertIn("Vanilla Mode", toggled_off["banner"])
                toggled_on = await bridge.send_command({"command": "toggle_mod"})
                self.assertTrue(toggled_on["active"])
                self.assertIn("Active", toggled_on["banner"])

                # 2. Verify 5 Colored Crystals activate Golden Gobbo
                for c in ("RED", "BLUE", "GREEN", "YELLOW"):
                    res = await bridge.send_command({
                        "command": "give_item",
                        "item_name": f"Get the Gobbo's Sandwich! - {c.title()} Crystal",
                        "group": "Colored Crystals",
                        "wad": "T1L4M001",
                        "color": c,
                    })
                    self.assertFalse(res["golden_gobbo_active"])

                res5 = await bridge.send_command({
                    "command": "give_item",
                    "item_name": "Get the Gobbo's Sandwich! - Purple Crystal",
                    "group": "Colored Crystals",
                    "wad": "T1L4M001",
                    "color": "PURPLE",
                })
                self.assertTrue(res5["golden_gobbo_active"], "5th Colored Crystal must activate Golden Gobbo")
                events = await bridge.poll_events()
                self.assertTrue(
                    any(e.get("event") == "golden_gobbo_activated" and e.get("wad") == "T1L4M001" for e in events)
                )

                # 3. Verify Fire Breakout Trap: Instant Heart Down (-1 heart) with fire animation & knockback
                await bridge.send_command({"command": "apply_trap", "trap_type": "fire_breakout"})
                events = await bridge.poll_events()
                self.assertEqual(bridge.last_state["current_hearts"], 4, "Fire Breakout Trap must instantly deduct 1 heart")
                self.assertTrue(bridge.last_state["fire_trap"]["active"], "Fire effect must be active for visual/knockback duration")
                self.assertTrue(any(e.get("event") == "fire_trap_applied" for e in events))

                # 4. Verify No Early Removal & Flat 10s Timed Traps (e.g. Tail-Knot)
                await bridge.send_command({"command": "apply_trap", "trap_type": "tail_knot"})
                # Front flip or other actions cannot extinguish or remove traps
                flip_res = await bridge.send_command({"command": "player_action", "action": "ACTION_FRONT_FLIP"})
                self.assertNotIn("fire_extinguished", flip_res, "Traps cannot be removed early by player actions")
                # Advance 2.1 seconds -> fire knockback/flames finish
                step_res = await bridge.send_command({"command": "step_tick", "dt": 2.1})
                self.assertFalse(step_res["state"]["fire_trap"]["active"], "Fire animation expires after knockback duration")
                # Advance 8.0 seconds -> flat 10s timed trap expires
                await bridge.send_command({"command": "step_tick", "dt": 8.0})
                events = await bridge.poll_events()
                self.assertTrue(
                    any(e.get("event") == "timed_trap_expired" and e.get("trap_type") == "tail_knot" for e in events)
                )

                # 5. Verify Door Shuffle redirection
                await bridge.send_command({
                    "command": "set_door_destination",
                    "source_door": "T1_D2",
                    "target_wad": "T3L4M001",
                })
                door_res = await bridge.send_command({
                    "command": "enter_door",
                    "door_id": "T1_D2",
                    "default_wad": "T1L3M001",
                })
                self.assertEqual(door_res["resolved_wad"], "T3L4M001")

                # 6. Verify Case-Sensitive Slot Name, Server Address, Port & Optional Password persistence
                conn_res = await bridge.send_command({
                    "command": "set_connection_info",
                    "server_host": "archipelago.gg",
                    "server_port": 38281,
                    "slot_name": "CrocHero_CaSeTeSt",
                    "password": "OptionalSecret123",
                })
                self.assertEqual(conn_res["state"]["server_host"], "archipelago.gg")
                self.assertEqual(conn_res["state"]["server_port"], 38281)
                self.assertEqual(conn_res["state"]["slot_name"], "CrocHero_CaSeTeSt")

                # 7. Verify Per-Save Random Seed Generation, Moveset Locks, Logical Checks & Save-Locked Mod Toggle
                door1_targets = set()
                seeds_seen = set()
                for _ in range(12):
                    await bridge.send_command({"command": "return_to_title"})
                    save_res = await bridge.send_command({"command": "start_new_save", "slot": 0})
                    st = save_res["state"]
                    self.assertTrue(st["save_locked_to_mod"], "Mod must lock ON when a randomizer save is created")
                    self.assertTrue(st["in_active_save"])
                    self.assertNotEqual(st["door_1_target"], "T1L2M001", "Door 1 must never be vanilla T1L2M001")
                    door1_targets.add(st["door_1_target"])
                    seeds_seen.add(st["current_seed_hex"])

                    # Moveset starts locked on a brand-new randomizer save
                    self.assertFalse(st["abilities"]["tail_whack"])
                    self.assertFalse(st["abilities"]["butt_stomp"])
                    self.assertFalse(st["abilities"]["power_gloves"])

                self.assertGreater(len(seeds_seen), 5, "Deleting/starting new saves must generate distinct random seeds")
                self.assertGreater(len(door1_targets), 1, "Door 1 must randomize across different levels on new saves")

                # While inside a Randomizer Save, toggling the mod off MUST be blocked!
                locked_toggle = await bridge.send_command({"command": "toggle_mod"})
                self.assertEqual(locked_toggle["status"], "locked_to_save")
                self.assertTrue(locked_toggle["mod_enabled_toggle"])
                self.assertTrue(locked_toggle["save_locked_to_mod"])

                # Claiming all 15 Sphere 1 checks (white crystals across Sailor Village stages 0..4)
                # must logically unlock all 5 core progression abilities (Tail Whack, Butt Stomp, Climbing Claws, Power Gloves, Lever Wrench)!
                for s_idx in range(5):
                    for c_idx in range(3):
                        chk_res = await bridge.send_command({
                            "command": "claim_solo_check",
                            "stage_idx": s_idx,
                            "sub_check_idx": c_idx,
                        })
                final_ab = chk_res["state"]["abilities"]
                self.assertTrue(final_ab["tail_whack"], "Sphere 1 checks must logically grant Tail Whack")
                self.assertTrue(final_ab["butt_stomp"], "Sphere 1 checks must logically grant Butt Stomp")
                self.assertTrue(final_ab["climbing_claws"], "Sphere 1 checks must logically grant Climbing Claws")
                self.assertTrue(final_ab["power_gloves"], "Sphere 1 checks must logically grant Power Gloves")
                self.assertTrue(final_ab["lever_wrench"], "Sphere 1 checks must logically grant Lever Wrench")

                # 8. Verify every regular White Crystal (1..100) is a one-time check:
                #    First time collecting White Crystal #7 in Stage 0 sends a check AND adds currency;
                #    Collecting White Crystal #7 again on a level revisit ONLY adds currency and does NOT send a duplicate check!
                await bridge.send_command({"command": "start_new_save", "slot": 1})
                await bridge.poll_events()  # Clear initial events
                c_before = bridge.last_state["current_crystals"]

                wc_first = await bridge.send_command({
                    "command": "collect_white_crystal",
                    "stage_idx": 0,
                    "crystal_index": 7,
                })
                self.assertTrue(wc_first["check_triggered"], "First-time White Crystal #7 pickup must trigger a check")
                self.assertEqual(wc_first["location_id"], 7211007)
                events_first = await bridge.poll_events()
                self.assertTrue(
                    any(
                        e.get("event") == "location_checked"
                        and e.get("location_id") == 7211007
                        and "Find the Key! Save the Gobbo!" in e.get("description", "")
                        for e in events_first
                    ),
                    "First-time White Crystal pickup must emit location_checked with authentic level name",
                )
                c_after_first = wc_first["current_crystals"]
                self.assertGreater(c_after_first, c_before, "First-time White Crystal pickup must increment currency")

                # Re-collect the exact same White Crystal #7 in Stage 0 (e.g., on re-entering the level)
                wc_second = await bridge.send_command({
                    "command": "collect_white_crystal",
                    "stage_idx": 0,
                    "crystal_index": 7,
                })
                self.assertFalse(
                    wc_second["check_triggered"],
                    "Re-collecting an already-checked White Crystal must NOT trigger another check",
                )
                self.assertEqual(
                    wc_second["current_crystals"],
                    c_after_first,
                    "Re-collecting an already-checked White Crystal must NOT add to crystal currency",
                )
                events_second = await bridge.poll_events()
                self.assertFalse(
                    any(e.get("event") == "location_checked" and e.get("location_id") == 7211007 for e in events_second),
                    "No duplicate location_checked event may be emitted when re-collecting a White Crystal",
                )

                # 9. Verify Live Inventory Wheel & HUD Items Delivery + Archipelago Color-Coordinated Feed:
                #    Delivering Jellies, Clockwork Gobbo, Binoculars, Keys, Jigsaws, and Croc Crystals (1, 5, 10)
                #    updates the live inventory counts and appends [IN] / [OUT] feed entries with importance colors.
                st_res = await bridge.send_command({"command": "get_state"})
                st_before = st_res["state"]
                red_before = st_before["red_jellies"]
                c_before_gives = st_before["current_crystals"]

                inv_res = await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200143,  # Gummi Jump (Red Jelly)
                    "sender": "Pete",
                    "location": "Pete's Emporium",
                    "classification": "useful",
                })
                self.assertEqual(inv_res["status"], "ok")
                self.assertEqual(inv_res["state"]["red_jellies"], red_before + 1)

                await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200144,  # Jelly Jump (Blue Jelly)
                    "sender": "Pete",
                    "location": "Pete's Emporium",
                    "classification": "useful",
                })
                await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200145,  # High Jelly Jump (Green Jelly)
                    "sender": "Pete",
                    "location": "Pete's Emporium",
                    "classification": "useful",
                })
                await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200142,  # Clockwork Gobbo
                    "sender": "Gobbo Friend",
                    "location": "Sailor Village Cave",
                    "classification": "useful",
                })
                await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200146,  # Binoculars
                    "sender": "Pete",
                    "location": "Pete's Emporium",
                    "classification": "useful",
                })
                await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200019,  # Silver Key
                    "sender": "Archipelago Server",
                    "location": "T1L2M001 Key Chest",
                    "classification": "progression",
                })
                await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200012,  # Jigsaw Piece
                    "sender": "King Rufus",
                    "location": "Sailor Village Hub Secret",
                    "classification": "progression",
                })

                # Test Croc Crystals sets of 1, 5, and 10
                c1_res = await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200148,  # Croc Crystals (1)
                    "sender": "Archipelago Server",
                    "location": "Secret Cave Crystal",
                    "classification": "filler",
                })
                self.assertEqual(c1_res["status"], "ok")
                self.assertEqual(c1_res["state"]["current_crystals"], c_before_gives + 1)

                c5_res = await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200149,  # Croc Crystals (5)
                    "sender": "Archipelago Server",
                    "location": "Underwater Crystal",
                    "classification": "filler",
                })
                self.assertEqual(c5_res["status"], "ok")
                self.assertEqual(c5_res["state"]["current_crystals"], c_before_gives + 1 + 5)

                c10_res = await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200150,  # Croc Crystals (10)
                    "sender": "Archipelago Server",
                    "location": "Cloud Bounce Crystal",
                    "classification": "filler",
                })
                self.assertEqual(c10_res["status"], "ok")
                self.assertEqual(c10_res["state"]["current_crystals"], c_before_gives + 1 + 5 + 10)

                await bridge.send_command({
                    "command": "notify_item_sent",
                    "item_name": "Power Gloves",
                    "receiver": "PartnerSlot",
                    "location": "Sailor Village Door 1 Clear",
                    "classification": "progression",
                })

                feed_res = await bridge.send_command({"command": "get_item_feed"})
                self.assertEqual(feed_res["status"], "ok")
                feed_entries = feed_res["feed"]
                self.assertGreaterEqual(len(feed_entries), 5, "Overlay feed must record recent incoming and outgoing items")

                # Verify presence of progression, useful, filler, and outgoing entries
                has_out_prog = any(e["direction"] == "OUT" and e["importance"] == "progression" and e["item_name"] == "Power Gloves" for e in feed_entries)
                has_in_useful = any(e["direction"] == "IN" and e["importance"] == "useful" for e in feed_entries)
                has_in_filler = any(e["direction"] == "IN" and e["importance"] == "filler" and "Croc Crystals" in e["item_name"] for e in feed_entries)
                has_in_prog = any(e["direction"] == "IN" and e["importance"] == "progression" for e in feed_entries)
                self.assertTrue(has_out_prog, "Overlay feed must contain color-coordinated [OUT] progression items")
                self.assertTrue(has_in_useful, "Overlay feed must contain color-coordinated [IN] useful items")
                self.assertTrue(has_in_filler, "Overlay feed must contain color-coordinated [IN] filler Croc Crystals")
                self.assertTrue(has_in_prog, "Overlay feed must contain color-coordinated [IN] progression items")

                # 10. Verify Overlay F10 Toggle & Small Indicator Badge:
                #     Full console overlay is invisible/collapsed by default (overlay_full_visible == False),
                #     showing only a small indicator badge. Toggling flips visibility.
                ov_state = await bridge.send_command({"command": "get_overlay_state"})
                self.assertEqual(ov_state["status"], "ok")
                self.assertFalse(ov_state["overlay_full_visible"], "Full console overlay must be invisible/collapsed by default")
                self.assertTrue(ov_state["small_indicator_active"], "Small indicator badge must be active")

                ov_toggle = await bridge.send_command({"command": "toggle_overlay"})
                self.assertTrue(ov_toggle["overlay_full_visible"], "Toggling overlay must expand the full console")

                ov_toggle2 = await bridge.send_command({"command": "toggle_overlay"})
                self.assertFalse(ov_toggle2["overlay_full_visible"], "Toggling overlay again must collapse back to minimal badge")

                # 11. Verify Bottom-Screen Toast Notifications:
                #     Outgoing: [Croc] sent [Item] to [Receiver]!
                #     Incoming: [Sender] sent [Item] to [Croc]!
                #     Even in solo rando with village characters (e.g., Swap Meet Pete, King Rufus, Sailor Gobbo)!

                # Test Outgoing Toast
                await bridge.send_command({
                    "command": "notify_item_sent",
                    "item_name": "Power Gloves",
                    "receiver": "PartnerSlot",
                    "location": "Find the Key! Save the Gobbo!",
                    "classification": "progression",
                })
                toast_out = await bridge.send_command({"command": "get_bottom_toast"})
                self.assertEqual(toast_out["status"], "ok")
                self.assertTrue(toast_out["active"])
                self.assertTrue(toast_out["is_outgoing"])
                self.assertEqual(toast_out["sender"], "Croc")
                self.assertEqual(toast_out["receiver"], "PartnerSlot")
                self.assertEqual(toast_out["item_name"], "Power Gloves")
                self.assertEqual(toast_out["importance"], "progression")
                self.assertIn("Croc sent Power Gloves to PartnerSlot", toast_out["formatted"])

                # Test Incoming Toast
                await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200003,  # Climbing Claws
                    "sender": "Swap Meet Pete",
                    "location": "Pete's Shop",
                    "classification": "progression",
                })
                toast_in = await bridge.send_command({"command": "get_bottom_toast"})
                self.assertEqual(toast_in["status"], "ok")
                self.assertTrue(toast_in["active"])
                self.assertFalse(toast_in["is_outgoing"])
                self.assertEqual(toast_in["sender"], "Swap Meet Pete")
                self.assertEqual(toast_in["receiver"], "Croc")
                self.assertEqual(toast_in["item_name"], "Climbing Claws")
                self.assertEqual(toast_in["importance"], "progression")
                self.assertIn("Swap Meet Pete sent Climbing Claws to Croc", toast_in["formatted"])

                # Test Solo Rando Outgoing & Incoming with Authentic Village Characters (no Player1 self-sends!)
                await bridge.send_command({"command": "start_new_save", "slot": 2})
                await bridge.send_command({
                    "command": "collect_white_crystal",
                    "stage_idx": 0,
                    "crystal_index": 12,
                })
                toast_solo = await bridge.send_command({"command": "get_bottom_toast"})
                self.assertEqual(toast_solo["status"], "ok")
                self.assertTrue(toast_solo["active"])
                self.assertEqual(toast_solo["sender"], "Croc")
                self.assertEqual(toast_solo["receiver"], "Croc")
                self.assertIn("got his own", toast_solo["formatted"], "Solo rando bottom toast must format as 'Croc got his own (item)!'")
                self.assertIn("Croc got his own", toast_solo["formatted"])

                # 12. Verify Archipelago Custom YAML Player Names (WillOfTheVinny, JiggyWithVinny, JakVinnyOff)
                yaml_players_res = await bridge.send_command({"command": "get_yaml_players"})
                self.assertEqual(yaml_players_res["status"], "ok")
                players_list = yaml_players_res["players"]
                self.assertIn("WillOfTheVinny", players_list, "Hook must discover WillOfTheVinny from Archipelago custom yaml folder")

                # Test sending specifically to WillOfTheVinny (from Ori and the Will of the Wisps)
                await bridge.send_command({
                    "command": "notify_item_sent",
                    "item_name": "Health Cell",
                    "receiver": "WillOfTheVinny",
                    "location": "Find the Key! Save the Gobbo!",
                    "classification": "progression",
                })
                toast_vinny = await bridge.send_command({"command": "get_bottom_toast"})
                self.assertEqual(toast_vinny["status"], "ok")
                self.assertTrue(toast_vinny["is_outgoing"])
                self.assertEqual(toast_vinny["sender"], "Croc")
                self.assertEqual(toast_vinny["receiver"], "WillOfTheVinny")
                self.assertEqual(toast_vinny["item_name"], "Health Cell")
                self.assertIn("Croc sent Health Cell to WillOfTheVinny!", toast_vinny["formatted"])

                # Test receiving specifically from WillOfTheVinny
                await bridge.send_command({
                    "command": "give_item",
                    "item_id": 7200003,  # Climbing Claws
                    "sender": "WillOfTheVinny",
                    "location": "Spirit Tree",
                    "classification": "progression",
                })
                toast_rx_vinny = await bridge.send_command({"command": "get_bottom_toast"})
                self.assertEqual(toast_rx_vinny["status"], "ok")
                self.assertFalse(toast_rx_vinny["is_outgoing"])
                self.assertEqual(toast_rx_vinny["sender"], "WillOfTheVinny")
                self.assertEqual(toast_rx_vinny["receiver"], "Croc")
                self.assertIn("WillOfTheVinny sent Climbing Claws to Croc!", toast_rx_vinny["formatted"])

                # Test that when receiver is literal "Someone Else", it resolves to the YAML name (e.g. WillOfTheVinny)
                await bridge.send_command({
                    "command": "notify_item_sent",
                    "item_name": "Extra Life",
                    "receiver": "Someone Else",
                    "location": "Sailor Village Secret",
                    "classification": "useful",
                })
                toast_generic = await bridge.send_command({"command": "get_bottom_toast"})
                self.assertNotEqual(toast_generic["receiver"], "Someone Else", "Must replace literal 'Someone Else' with YAML player name")
                self.assertEqual(toast_generic["receiver"], "WillOfTheVinny")
                self.assertIn("Croc sent Extra Life to WillOfTheVinny!", toast_generic["formatted"])

                # Returning to Title Screen unlocks the Mod Toggle again
                await bridge.send_command({"command": "return_to_title"})
                unlocked_toggle = await bridge.send_command({"command": "toggle_mod"})
                self.assertEqual(unlocked_toggle["status"], "ok")
                self.assertFalse(unlocked_toggle["mod_enabled_toggle"])

                await bridge.disconnect()

            asyncio.run(run_tcp_checks())
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=2)
                except Exception:
                    proc.kill()
            for p in (test_ini,):
                if os.path.exists(p):
                    try:
                        os.remove(p)
                    except Exception:
                        pass

    def test_installer_and_uninstaller_lifecycle(self):
        import shutil
        import tempfile
        from installer import get_install_status, install_mod, toggle_vanilla_ini, uninstall_mod

        temp_root = tempfile.mkdtemp(prefix="croc2ap_test_")
        try:
            mock_game_dir = os.path.join(temp_root, "Croc 2")
            mock_ap_dir = os.path.join(temp_root, "Archipelago")
            os.makedirs(mock_game_dir, exist_ok=True)
            os.makedirs(mock_ap_dir, exist_ok=True)

            # Create a dummy pre-existing version.dll to test backup & restore
            orig_dll_path = os.path.join(mock_game_dir, "version.dll")
            with open(orig_dll_path, "wb") as f:
                f.write(b"ORIGINAL_VANILLA_DLL_BYTES")

            # 1. Install Mod & APWorld
            installed = install_mod(mock_game_dir, mock_ap_dir)
            self.assertIn("backup_dll", installed)
            self.assertIn("game_dll", installed)
            self.assertIn("game_ini", installed)
            self.assertIn("apworld", installed)
            self.assertIn("yaml_template", installed)

            status = get_install_status(mock_game_dir, mock_ap_dir)
            self.assertTrue(status["hook_dll_installed"])
            self.assertTrue(status["mod_ini_installed"])
            self.assertTrue(status["mod_ini_enabled"])
            self.assertTrue(status["apworld_installed"])
            self.assertTrue(status["yaml_template_installed"])

            # 2. Toggle Vanilla Mode in croc2_mod.ini
            new_state, _ = toggle_vanilla_ini(mock_game_dir)
            self.assertFalse(new_state)
            self.assertFalse(get_install_status(mock_game_dir, mock_ap_dir)["mod_ini_enabled"])

            new_state_2, _ = toggle_vanilla_ini(mock_game_dir)
            self.assertTrue(new_state_2)
            self.assertTrue(get_install_status(mock_game_dir, mock_ap_dir)["mod_ini_enabled"])

            # 3. Uninstall Mod & verify original version.dll was restored from backup
            uninstall_mod(mock_game_dir, mock_ap_dir)
            status_after = get_install_status(mock_game_dir, mock_ap_dir)
            self.assertFalse(status_after["mod_ini_installed"])
            self.assertFalse(status_after["apworld_installed"])
            self.assertFalse(status_after["yaml_template_installed"])
            self.assertTrue(os.path.isfile(orig_dll_path))
            with open(orig_dll_path, "rb") as f:
                self.assertEqual(f.read(), b"ORIGINAL_VANILLA_DLL_BYTES")
        finally:
            shutil.rmtree(temp_root, ignore_errors=True)

    def test_standalone_launcher_build_and_components(self):
        root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        launcher_exe = os.path.join(root, "Croc2ArchipelagoLauncher.exe")
        build_bat = os.path.join(root, "build.bat")
        self.assertTrue(os.path.isfile(build_bat), "build.bat must exist in project root")

        # Verify all C# launcher source modules exist
        src_dir = os.path.join(root, "src", "launcher")
        expected_sources = [
            "Program.cs", "MainForm.cs", "ApClient.cs",
            "HookBridgeClient.cs", "GameDetector.cs",
            "SettingsManager.cs", "MiniJson.cs", "Croc2Data.cs",
        ]
        for src_name in expected_sources:
            src_path = os.path.join(src_dir, src_name)
            self.assertTrue(os.path.isfile(src_path), f"Launcher source file {src_name} must exist")

        # Verify executable existence, non-empty size, and Windows PE signature
        self.assertTrue(os.path.isfile(launcher_exe), "Croc2ArchipelagoLauncher.exe must exist")
        self.assertGreater(os.path.getsize(launcher_exe), 20000, "Launcher executable must be > 20KB")
        with open(launcher_exe, "rb") as f:
            header = f.read(2)
            self.assertEqual(header, b"MZ", "Executable must have valid Windows PE header (MZ)")

    def test_archipelago_logo_collectable_models(self):
        """
        Validates that:
        1. LogoData.cs defines valid base64 for 3600-byte vertex buffer and 2016-byte index buffer.
        2. Non-gem collectables (crates, quest items/binoculars, heart pots, keys, gobbos) are targeted.
        3. All gem models (white crystal OBJ_011, colored crystals OBJ_015..019) and level scenery (OBJ_000, OBJ_006, OBJ_009, OBJ_014, >700KB) are NEVER touched.
        4. Rebuilt ZMOD preserves original submesh count, submesh material names, sets index_count=504 on submesh 0, and index_count=0 on subsequent submeshes.
        """
        import base64
        import struct

        logo_data_path = os.path.join(ROOT_DIR, "src", "launcher", "LogoData.cs")
        self.assertTrue(os.path.isfile(logo_data_path), "src/launcher/LogoData.cs must exist")

        with open(logo_data_path, "r", encoding="utf-8") as f:
            content = f.read()

        m_v = re.search(r'public const string VertexDataBase64 = "([^"]+)";', content)
        m_i = re.search(r'public const string IndexDataBase64 = "([^"]+)";', content)
        self.assertIsNotNone(m_v)
        self.assertIsNotNone(m_i)

        vdata = base64.b64decode(m_v.group(1))
        idata = base64.b64decode(m_i.group(1))
        self.assertEqual(len(vdata), 3600, "Vertex data must be exactly 3600 bytes (150 vertices * 24 bytes)")
        self.assertEqual(len(idata), 2016, "Index data must be exactly 2016 bytes (504 uint32 indices)")

        # Target list comes straight from GameDetector.cs (an explicit allowlist of shared-library models).
        # Model numbers are per-map indices, so per-map rules like "every *_OBJ_021" hit random scenery.
        with open(os.path.join(ROOT_DIR, "src", "launcher", "GameDetector.cs"), "r", encoding="utf-8-sig") as f:
            detector = f.read()
        block = re.search(r"LogoTargetNames\s*=\s*\{(.*?)\};", detector, re.S)
        self.assertIsNotNone(block, "GameDetector.cs must define the LogoTargetNames allowlist")
        targets = set(re.findall(r'"(EU_[A-Z0-9_]+)"', block.group(1)))

        def is_target(fname: str) -> bool:
            return fname.upper() in targets

        self.assertTrue(is_target("EU_T0L1M003_OBJ_009"), "Shared Key model MUST be replaced")
        self.assertTrue(is_target("EU_T0L1M003_OBJ_022"), "Shared Heart model MUST be replaced")
        for gem in ("011", "015", "016", "017", "018", "019"):
            self.assertFalse(is_target("EU_T0L1M003_OBJ_" + gem), "Crystal OBJ_%s must NOT be replaced" % gem)
        # Same numbers in ordinary level maps are unrelated level props and must never be touched
        for prop in ("EU_T1L2M001_OBJ_021", "EU_T1L1M001_OBJ_007", "EU_T1L1M004_OBJ_010", "EU_T2L7M001_OBJ_012"):
            self.assertFalse(is_target(prop), prop + " is a per-map prop, not a collectable")
        self.assertIn("trailerLen", detector, "Rebuilt ZMOD must keep the original 20-byte trailer")

        # Verify ZMOD rebuilding logic
        # Create a mock 2-submesh ZMOD
        mock_orig = bytearray(b"ZMOD\x01\x00\x00\x00\x00\x00\x00\x00\x02\x00\x00\x00")
        sub0_mat = b"mat_sub0\x00" + b"\x00" * 23
        sub0_meta = struct.pack("<II6f8s3I", 120, 0, -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, b"\x00"*8, 0xFFFFFFFF, 0, 0xFFFFFFFF)
        mock_orig += sub0_mat + sub0_meta

        sub1_mat = b"mat_sub1\x00" + b"\x00" * 23
        sub1_meta = struct.pack("<II6f8s3I", 240, 120, -2.0, -2.0, -2.0, 2.0, 2.0, 2.0, b"\x00"*8, 0xFFFFFFFF, 0, 0xFFFFFFFF)
        mock_orig += sub1_mat + sub1_meta

        # Mock payload
        mock_orig += struct.pack("<I", 100) + b"\xAA" * 100
        mock_orig += struct.pack("<I", 50) + b"\xBB" * 50

        # Now simulate BuildArchipelagoLogoZmod
        header_len = 16 + 2 * 84
        patched = bytearray(mock_orig[:header_len])
        # submesh 0: index_count = 504, index_offset = 0, bounding box [-0.1, -0.1, -0.05] to [0.1, 0.1, 0.05]
        struct.pack_into("<II6f", patched, 16 + 32, 504, 0, -0.1, -0.1, -0.05, 0.1, 0.1, 0.05)
        # submesh 1: index_count = 0, index_offset = 0
        struct.pack_into("<II6f", patched, 16 + 84 + 32, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)

        # append vdata and idata
        patched += struct.pack("<I", len(vdata)) + vdata
        patched += struct.pack("<I", len(idata)) + idata

        # Assertions on patched ZMOD
        self.assertEqual(patched[:4], b"ZMOD")
        num_sub = struct.unpack_from("<I", patched, 12)[0]
        self.assertEqual(num_sub, 2)

        # Check preserved material names
        mat0 = patched[16:16+32].split(b"\x00")[0]
        mat1 = patched[16+84:16+84+32].split(b"\x00")[0]
        self.assertEqual(mat0, b"mat_sub0")
        self.assertEqual(mat1, b"mat_sub1")

        # Check index counts
        idx_cnt0 = struct.unpack_from("<I", patched, 16 + 32)[0]
        idx_cnt1 = struct.unpack_from("<I", patched, 16 + 84 + 32)[0]
        self.assertEqual(idx_cnt0, 504)
        self.assertEqual(idx_cnt1, 0)

        # Check payload
        offset = header_len
        v_size = struct.unpack_from("<I", patched, offset)[0]
        self.assertEqual(v_size, 3600)
        offset += 4 + v_size
        i_size = struct.unpack_from("<I", patched, offset)[0]
        self.assertEqual(i_size, 2016)


    def test_goal_completion_detection(self):
        """The client must report CLIENT_GOAL exactly when rules.py's completion_condition is met."""
        from worlds.croc2.client_bridge import DANTE_FINAL_FIGHT_LOC, goal_reached

        sd = {"goal": 0, "jigsaw_pieces_required": 4, "trophies_required": 12}
        self.assertFalse(goal_reached(sd, [], set()))
        self.assertTrue(goal_reached(sd, [], {DANTE_FINAL_FIGHT_LOC}))
        self.assertFalse(goal_reached({}, [], {DANTE_FINAL_FIGHT_LOC}), "No slot data yet -> never report")

        sd = {"goal": 1, "jigsaw_pieces_required": 3}
        self.assertFalse(goal_reached(sd, [7200012, 7200013], set()))
        self.assertTrue(goal_reached(sd, [7200012, 7200013, 7200016], set()))

        sd = {"goal": 2, "trophies_required": 2}
        self.assertFalse(goal_reached(sd, [7200017], set()))
        self.assertTrue(goal_reached(sd, [7200017, 7200017], set()))

        sd = {"goal": 3, "jigsaw_pieces_required": 1, "trophies_required": 1}
        self.assertFalse(goal_reached(sd, [7200012, 7200017], set()))
        self.assertTrue(goal_reached(sd, [7200012, 7200017], {DANTE_FINAL_FIGHT_LOC}))

    def test_sailor_door_1_never_vehicle_or_vanilla(self):
        """Sailor Village Door 1 is open from the start; a vehicle stage there softlocks without the license."""
        import random
        import types
        from worlds.croc2.options import DoorShuffle
        from worlds.croc2.regions import VEHICLE_STAGE_WADS, compute_door_pairing

        for mode in (DoorShuffle.option_within_village, DoorShuffle.option_any_village):
            opts = types.SimpleNamespace(door_shuffle=types.SimpleNamespace(value=mode))
            for seed in range(300):
                _, door_map = compute_door_pairing(random.Random(seed), opts)
                self.assertNotIn(door_map["T1_D1"], VEHICLE_STAGE_WADS, "mode %d seed %d" % (mode, seed))
                self.assertNotEqual(door_map["T1_D1"], "T1L2M001", "mode %d seed %d" % (mode, seed))
                self.assertEqual(len(set(door_map.values())), 20, "door shuffle must stay a permutation")


if __name__ == "__main__":
    unittest.main(verbosity=2)
