"""Region graph construction and Door Shuffle implementation for Croc 2."""

from typing import Dict, List, Tuple
from BaseClasses import Entrance, MultiWorld, Region
from .locations import Croc2Location, LEVEL_MAPPING, LOCATION_TABLE
from .options import ColoredCrystalsMode, Croc2Options, DoorShuffle, ShufflePeteStore, ShuffleWhiteCrystals


def create_croc2_regions(
    multiworld: MultiWorld,
    player: int,
    options: Croc2Options,
    door_pairing: Dict[str, dict],
) -> None:
    """Builds all Village Hubs, Pete's Stores, Stage Regions, Secret Levels, and Boss Arenas."""
    menu_region = Region("Menu", player, multiworld)
    multiworld.regions.append(menu_region)

    regions_by_name: Dict[str, Region] = {"Menu": menu_region}

    # Index locations by region name
    locs_by_region: Dict[str, List[dict]] = {}
    for loc in LOCATION_TABLE:
        locs_by_region.setdefault(loc["region"], []).append(loc)

    def should_include_location(loc_entry: dict) -> bool:
        cat = loc_entry["category"]
        if cat == "Pete Store":
            return options.shuffle_pete_store.value == ShufflePeteStore.option_shuffle_shop_items
        if cat == "Colored Crystals":
            return options.colored_crystals_mode.value == ColoredCrystalsMode.option_shuffled_in_pool
        if cat == "Normal Gobbos":
            return False
        if cat == "White Crystals":
            wc_mode = options.shuffle_white_crystals.value
            if wc_mode == ShuffleWhiteCrystals.option_none:
                return False
            c_idx = loc_entry.get("crystal_index", 0)
            if wc_mode == ShuffleWhiteCrystals.option_milestones_50:
                return c_idx in (50, 100)
            if wc_mode == ShuffleWhiteCrystals.option_milestones_10:
                return c_idx % 10 == 0
            return True
        return True

    def make_region(region_name: str) -> Region:
        reg = Region(region_name, player, multiworld)
        for loc_entry in locs_by_region.get(region_name, []):
            if should_include_location(loc_entry):
                reg.locations.append(
                    Croc2Location(player, loc_entry["name"], loc_entry["id"], reg)
                )
        multiworld.regions.append(reg)
        regions_by_name[region_name] = reg
        return reg

    # Create all Hub regions, Shop regions, Level regions, Secret regions, and Boss regions
    hubs = LEVEL_MAPPING["hubs"]
    for hub in hubs:
        hub_name = hub["name"]
        make_region(hub_name)
        make_region(f"{hub_name} - Pete's Store")
        for lvl in hub["levels"]:
            make_region(f"{hub_name} - {lvl['name']}")
        make_region(f"{hub_name} - {hub['secret_level']['name']}")
        for boss in hub["bosses"]:
            make_region(f"{hub_name} - {boss['name']}")

    # Connect Menu -> Sailor Village
    start_exit = Entrance(player, "Start Game", menu_region)
    menu_region.exits.append(start_exit)
    start_exit.connect(regions_by_name["Sailor Village"])

    # Connect Hubs sequentially: Sailor -> Cossack -> Caveman -> Inca
    for idx in range(len(hubs) - 1):
        curr_hub = hubs[idx]
        next_hub = hubs[idx + 1]
        curr_reg = regions_by_name[curr_hub["name"]]
        next_reg = regions_by_name[next_hub["name"]]
        hub_exit = Entrance(
            player,
            f"{curr_hub['name']} to {next_hub['name']}",
            curr_reg,
        )
        curr_reg.exits.append(hub_exit)
        hub_exit.connect(next_reg)

    # Connect each Hub to its Pete's Store, Secret Level, Bosses, and Stage Doors (supporting Door Shuffle)
    for hub in hubs:
        hub_name = hub["name"]
        hub_reg = regions_by_name[hub_name]

        # Pete's Store entrance
        shop_reg = regions_by_name[f"{hub_name} - Pete's Store"]
        shop_ent = Entrance(player, f"{hub_name} -> Pete's Store", hub_reg)
        hub_reg.exits.append(shop_ent)
        shop_ent.connect(shop_reg)

        # Stage Doors (using door_pairing which maps door_id -> target level dict)
        for lvl in hub["levels"]:
            door_id = lvl["door_id"]
            target_lvl = door_pairing.get(door_id, lvl)
            target_reg_name = f"{target_lvl['hub_name']} - {target_lvl['name']}"
            target_reg = regions_by_name[target_reg_name]
            door_ent = Entrance(
                player,
                f"{hub_name} Door {lvl['door_num']} Entrance",
                hub_reg,
            )
            hub_reg.exits.append(door_ent)
            door_ent.connect(target_reg)

        # Secret Level entrance
        sec = hub["secret_level"]
        sec_reg = regions_by_name[f"{hub_name} - {sec['name']}"]
        sec_ent = Entrance(player, f"{hub_name} -> {sec['name']}", hub_reg)
        hub_reg.exits.append(sec_ent)
        sec_ent.connect(sec_reg)

        # Boss entrances
        for boss in hub["bosses"]:
            boss_reg = regions_by_name[f"{hub_name} - {boss['name']}"]
            boss_ent = Entrance(player, f"{hub_name} -> {boss['name']}", hub_reg)
            hub_reg.exits.append(boss_ent)
            boss_ent.connect(boss_reg)


# Must match is_vehicle_stage_wad() in src/hook/croc2_ap_hook.cpp. The hook refuses entry to these
# without a Vehicle License, so none of them may sit behind Sailor Village Door 1 (open from the start).
VEHICLE_STAGE_WADS = {"T1L3M001", "T1L5M001", "T2L3M001", "T2L6M001", "T3L4M001", "T3L5M001", "T4L3M001"}


def _fix_first_door(world_random, targets: list, vanilla_wad: str) -> None:
    """Swap a non-vanilla, non-vehicle stage into slot 0 if slot 0 is vanilla or a vehicle stage."""
    first = targets[0]["wad"]
    if first != vanilla_wad and first not in VEHICLE_STAGE_WADS:
        return
    ok = [i for i in range(1, len(targets))
          if targets[i]["wad"] != vanilla_wad and targets[i]["wad"] not in VEHICLE_STAGE_WADS]
    if ok:
        j = world_random.choice(ok)
        targets[0], targets[j] = targets[j], targets[0]


def compute_door_pairing(world_random, options: Croc2Options) -> Tuple[Dict[str, dict], Dict[str, str]]:
    """
    Computes the mapping of physical village doors (`T1_D1`..`T4_D5`) to level descriptors and WAD codes.
    Returns `(door_pairing, slot_door_map)`.
    """
    all_doors: List[Tuple[str, dict]] = []
    by_hub: Dict[str, List[Tuple[str, dict]]] = {}

    for hub in LEVEL_MAPPING["hubs"]:
        hub_list = []
        for lvl in hub["levels"]:
            lvl_copy = dict(lvl)
            lvl_copy["hub_name"] = hub["name"]
            lvl_copy["hub_id"] = hub["id"]
            all_doors.append((lvl["door_id"], lvl_copy))
            hub_list.append((lvl["door_id"], lvl_copy))
        by_hub[hub["id"]] = hub_list

    door_pairing: Dict[str, dict] = {}
    slot_door_map: Dict[str, str] = {}

    if options.door_shuffle.value == DoorShuffle.option_off:
        for door_id, lvl_copy in all_doors:
            door_pairing[door_id] = lvl_copy
            slot_door_map[door_id] = lvl_copy["wad"]
    elif options.door_shuffle.value == DoorShuffle.option_within_village:
        for hub_id, hub_doors in by_hub.items():
            targets = [lvl_copy for _, lvl_copy in hub_doors]
            shuffled = list(targets)
            world_random.shuffle(shuffled)
            # Ensure Door 1 in Sailor Village is visibly shuffled to a non-vanilla stage
            if hub_id == "sailor_village" and len(shuffled) > 1:
                _fix_first_door(world_random, shuffled, targets[0]["wad"])
            for (door_id, _), target in zip(hub_doors, shuffled):
                door_pairing[door_id] = target
                slot_door_map[door_id] = target["wad"]
    else:
        # DoorShuffle.option_any_village
        all_door_ids = [d_id for d_id, _ in all_doors]
        all_targets = [lvl_copy for _, lvl_copy in all_doors]
        shuffled_targets = list(all_targets)
        world_random.shuffle(shuffled_targets)
        if len(shuffled_targets) > 1:
            _fix_first_door(world_random, shuffled_targets, all_targets[0]["wad"])
        for door_id, target in zip(all_door_ids, shuffled_targets):
            door_pairing[door_id] = target
            slot_door_map[door_id] = target["wad"]

    return door_pairing, slot_door_map

