"""Main World definition for Croc 2: Kingdom of the Gobbos in Archipelago."""

from typing import Any, Dict, List
from BaseClasses import Item, ItemClassification
from worlds.AutoWorld import World

from .items import (
    CLASSIFICATION_MAP,
    Croc2Item,
    FILLER_ITEMS,
    ITEM_NAME_GROUPS,
    ITEM_NAME_TO_DATA,
    ITEM_NAME_TO_ID,
    ITEM_TABLE,
    TRAP_ITEMS,
)
from .locations import (
    LEVEL_MAPPING,
    LOCATION_NAME_GROUPS,
    LOCATION_NAME_TO_ID,
)
from .options import ColoredCrystalsMode, Croc2Options, ShufflePeteStore
from .regions import compute_door_pairing, create_croc2_regions
from .rules import set_croc2_rules
from .web_world import Croc2WebWorld

try:
    from . import client  # Register Croc 2 Client in Archipelago Launcher
except Exception:
    pass


class Croc2World(World):
    """
    Croc 2: Kingdom of the Gobbos is a 3D action-platformer by Argonaut Software.
    Join Croc across Sailor, Cossack, Caveman, and Inca Villages as he searches for his
    missing parents, rescues Gobbos, collects Colored Crystals to awaken Golden Gobbos,
    rides vehicles, and foils the plans of the newly resurrected Baron Dante!
    """

    game: str = "Croc 2"
    web = Croc2WebWorld()

    options_dataclass = Croc2Options
    options: Croc2Options

    item_name_to_id = ITEM_NAME_TO_ID
    location_name_to_id = LOCATION_NAME_TO_ID
    item_name_groups = ITEM_NAME_GROUPS
    location_name_groups = LOCATION_NAME_GROUPS

    origin_region_name = "Menu"

    def __init__(self, multiworld, player: int):
        super().__init__(multiworld, player)
        self.door_pairing: Dict[str, dict] = {}
        self.slot_door_map: Dict[str, str] = {}

    def generate_early(self) -> None:
        self.door_pairing, self.slot_door_map = compute_door_pairing(
            self.random, self.options
        )
        # Ensure at least one core ability is placed early if abilities are shuffled
        if self.options.shuffle_abilities.value:
            self.multiworld.early_items[self.player]["Tail Whack"] = 1
            self.multiworld.early_items[self.player]["Power Gloves"] = 1

    def create_regions(self) -> None:
        create_croc2_regions(
            self.multiworld,
            self.player,
            self.options,
            self.door_pairing,
        )

    def create_item(self, name: str) -> Item:
        data = ITEM_NAME_TO_DATA[name]
        classification = CLASSIFICATION_MAP[data["classification"]]
        return Croc2Item(name, classification, data["id"], self.player)

    def get_filler_item_name(self) -> str:
        if self.options.trap_weight.value > 0:
            if self.random.randint(1, 100) <= self.options.trap_weight.value:
                available_traps = list(TRAP_ITEMS)
                if not self.options.fire_breakout_trap.value:
                    available_traps = [
                        t for t in available_traps if t != "Fire Breakout Trap"
                    ]
                if available_traps:
                    return self.random.choice(available_traps)
        return self.random.choice(FILLER_ITEMS)

    def create_items(self) -> None:
        pool: List[Item] = []

        # 1. Core Abilities
        if self.options.shuffle_abilities.value:
            for ab in ("Tail Whack", "Butt Stomp", "Climbing Claws", "Power Gloves"):
                pool.append(self.create_item(ab))

        if self.options.shuffle_levers.value:
            pool.append(self.create_item("Lever Wrench"))

        if self.options.shuffle_vehicles.value:
            pool.append(self.create_item("Vehicle License"))

        if self.options.shuffle_plane_guns.value:
            pool.append(self.create_item("Airplane Ammo Feed"))

        # 2. Swap Meet Pete's VIP Passes
        if self.options.shuffle_pete_store.value != ShufflePeteStore.option_vanilla:
            for hub in LEVEL_MAPPING["hubs"]:
                pool.append(self.create_item(hub["vip_pass"]))

        # 3. Level Access Keys (Sailor Village Door 1 is open from start)
        if self.options.lock_level_access.value:
            for hub in LEVEL_MAPPING["hubs"]:
                for lvl in hub["levels"]:
                    if hub["id"] == "sailor_village" and lvl["door_num"] == 1:
                        continue
                    key_name = f"{lvl['name']} Key"
                    pool.append(self.create_item(key_name))

        # 4. Jigsaw Pieces (4 total: 1 per Village Hub), Trophies (16), and Golden Gobbos (20)
        for _ in range(4):
            pool.append(self.create_item("Jigsaw Piece"))
        for _ in range(16):
            pool.append(self.create_item("Trophy"))
        for _ in range(20):
            pool.append(self.create_item("Golden Gobbo"))

        # 5. Colored Crystals (100 total: 5 per level across 20 stages)
        if (
            self.options.colored_crystals_mode.value
            == ColoredCrystalsMode.option_shuffled_in_pool
        ):
            for entry in ITEM_TABLE:
                if entry.get("group") == "Colored Crystals":
                    pool.append(self.create_item(entry["name"]))

        # 6. Useful Items (Heart Pots, Keys, Pete Shop Items) matching remaining locations
        useful_candidates: List[str] = []
        for entry in ITEM_TABLE:
            if entry["classification"] == "useful":
                useful_candidates.extend([entry["name"]] * entry.get("default_count", 1))

        unfilled_count = len(self.multiworld.get_unfilled_locations(self.player))
        for item_name in useful_candidates:
            if len(pool) < unfilled_count:
                pool.append(self.create_item(item_name))

        # 7. Fill remaining slots with Filler (Normal Crystals) and Traps according to trap_weight
        while len(pool) < unfilled_count:
            pool.append(self.create_item(self.get_filler_item_name()))

        # Trim if minimal location pool is smaller than base items (only filler/useful trimmed)
        if len(pool) > unfilled_count:
            pool = pool[:unfilled_count]

        self.multiworld.itempool.extend(pool)

    def set_rules(self) -> None:
        set_croc2_rules(self.multiworld, self.player, self.options)

    def fill_slot_data(self) -> Dict[str, Any]:
        return {
            "goal": self.options.goal.value,
            "jigsaw_pieces_required": self.options.jigsaw_pieces_required.value,
            "trophies_required": self.options.trophies_required.value,
            "colored_crystals_mode": self.options.colored_crystals_mode.value,
            "shuffle_white_crystals": int(self.options.shuffle_white_crystals.value),
            "lock_level_access": int(self.options.lock_level_access.value),
            "shuffle_abilities": int(self.options.shuffle_abilities.value),
            "shuffle_levers": int(self.options.shuffle_levers.value),
            "shuffle_vehicles": int(self.options.shuffle_vehicles.value),
            "shuffle_plane_guns": int(self.options.shuffle_plane_guns.value),
            "shuffle_pete_store": self.options.shuffle_pete_store.value,
            "door_shuffle": self.options.door_shuffle.value,
            "trap_weight": self.options.trap_weight.value,
            "fire_breakout_trap": int(self.options.fire_breakout_trap.value),
            "death_link": int(self.options.death_link.value),
            "door_map": self.slot_door_map,
        }

