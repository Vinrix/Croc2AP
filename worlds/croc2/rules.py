"""Logical access rules and goal completion conditions for the Croc 2 Archipelago World."""

from typing import List
from BaseClasses import CollectionState, MultiWorld
from worlds.generic.Rules import set_rule
from .locations import LEVEL_MAPPING, LOCATION_NAME_TO_DATA
from .options import ColoredCrystalsMode, Croc2Options, Goal, ShufflePeteStore
from .regions import VEHICLE_STAGE_WADS


def has_ability(state: CollectionState, player: int, options: Croc2Options, ability_name: str) -> bool:
    """Checks whether the player has unlocked a specific movement or interaction ability."""
    if ability_name in ("Tail Whack", "Butt Stomp", "Climbing Claws", "Power Gloves"):
        return not options.shuffle_abilities.value or state.has(ability_name, player)
    if ability_name == "Lever Wrench":
        return not options.shuffle_levers.value or state.has("Lever Wrench", player)
    if ability_name == "Vehicle License":
        return not options.shuffle_vehicles.value or state.has("Vehicle License", player)
    if ability_name == "Airplane Ammo Feed":
        return not options.shuffle_plane_guns.value or state.has("Airplane Ammo Feed", player)
    return state.has(ability_name, player)


def has_all_requirements(
    state: CollectionState,
    player: int,
    options: Croc2Options,
    reqs: List[str],
) -> bool:
    return all(has_ability(state, player, options, r) for r in reqs)


def can_activate_golden_gobbo(
    state: CollectionState,
    player: int,
    options: Croc2Options,
    level_prefix: str,
) -> bool:
    """
    Golden Gobbos are locked behind having all 5 Colored Crystals (Red, Blue, Green, Yellow, Purple)
    for that specific level. Once all 5 gems for the stage are received, the Golden Gobbo is activated.
    """
    if options.colored_crystals_mode.value != ColoredCrystalsMode.option_shuffled_in_pool:
        return True
    colors = ("Red", "Blue", "Green", "Yellow", "Purple")
    return all(state.has(f"{level_prefix} - {c} Crystal", player) for c in colors)


REGION_TO_WAD = {
    f"{hub['name']} - {lvl['name']}": lvl["wad"] for hub in LEVEL_MAPPING["hubs"] for lvl in hub["levels"]
}


def set_croc2_rules(multiworld: MultiWorld, player: int, options: Croc2Options) -> None:
    """Applies entrance and location rules across all regions in the Croc 2 world."""
    hubs = LEVEL_MAPPING["hubs"]

    # 1. Hub-to-Hub progression rules (requires defeating boss of previous hub)
    for idx in range(len(hubs) - 1):
        curr_hub = hubs[idx]
        next_hub = hubs[idx + 1]
        ent_name = f"{curr_hub['name']} to {next_hub['name']}"
        boss_reqs = curr_hub["bosses"][0]["requires"]
        set_rule(
            multiworld.get_entrance(ent_name, player),
            lambda state, reqs=boss_reqs: has_all_requirements(state, player, options, reqs),
        )

    # 2. Per-Hub Entrances (Pete's Store, Stage Doors, Secret Level, Bosses)
    for hub_idx, hub in enumerate(hubs):
        hub_name = hub["name"]
        vip_pass = hub["vip_pass"]

        # Swap Meet Pete's General Store access
        shop_ent = multiworld.get_entrance(f"{hub_name} -> Pete's Store", player)
        if options.shuffle_pete_store.value != ShufflePeteStore.option_vanilla:
            set_rule(shop_ent, lambda state, vp=vip_pass: state.has(vp, player))

        # Stage Door locks (Door 1 in Sailor Village is open by default to guarantee sphere 1 access)
        for lvl in hub["levels"]:
            door_ent = multiworld.get_entrance(
                f"{hub_name} Door {lvl['door_num']} Entrance", player
            )
            needs_key = bool(options.lock_level_access.value) and not (
                hub["id"] == "sailor_village" and lvl["door_num"] == 1
            )
            key_name = f"{lvl['name']} Key"
            # The hook refuses entry to vehicle stages without the Vehicle License, so the door
            # leading to one (wherever Door Shuffle put it) needs the license in logic too.
            target_wad = REGION_TO_WAD.get(door_ent.connected_region.name if door_ent.connected_region else "")
            needs_license = target_wad in VEHICLE_STAGE_WADS
            set_rule(
                door_ent,
                lambda state, kn=key_name, nk=needs_key, nl=needs_license: (
                    (not nk or state.has(kn, player))
                    and (not nl or has_ability(state, player, options, "Vehicle License"))
                ),
            )

        # Secret Level entrance (requires Golden Gobbos + secret stage abilities)
        sec = hub["secret_level"]
        sec_ent = multiworld.get_entrance(f"{hub_name} -> {sec['name']}", player)
        sec_reqs = sec["requires"]
        req_golden_gobbos = (hub_idx + 1) * 3
        set_rule(
            sec_ent,
            lambda state, reqs=sec_reqs, gg=req_golden_gobbos: (
                state.has("Golden Gobbo", player, gg)
                and has_all_requirements(state, player, options, reqs)
            ),
        )

        # Boss Entrances
        for boss in hub["bosses"]:
            boss_ent = multiworld.get_entrance(f"{hub_name} -> {boss['name']}", player)
            boss_reqs = boss["requires"]
            if boss["wad"] == "T4B2M001":
                # Dante's Final Fight requires the configured Jigsaw Pieces
                req_jigsaws = options.jigsaw_pieces_required.value
                set_rule(
                    boss_ent,
                    lambda state, reqs=boss_reqs, jp=req_jigsaws: (
                        state.has("Jigsaw Piece", player, jp)
                        and has_all_requirements(state, player, options, reqs)
                    ),
                )
            else:
                set_rule(
                    boss_ent,
                    lambda state, reqs=boss_reqs: has_all_requirements(state, player, options, reqs),
                )

    # 3. Location-Specific Rules (Ability requirements + 5 Colored Crystal Golden Gobbo rule)
    first_door_ent = multiworld.get_entrance("Sailor Village Door 1 Entrance", player)
    first_door_region_name = (
        first_door_ent.connected_region.name if first_door_ent.connected_region else ""
    )

    for loc in multiworld.get_locations(player):
        loc_data = LOCATION_NAME_TO_DATA.get(loc.name)
        if not loc_data:
            continue
        reqs = loc_data.get("requires", [])
        cat = loc_data.get("category", "")

        if cat == "Golden Gobbos":
            level_prefix = loc_data["level_prefix"]
            set_rule(
                loc,
                lambda state, reqs=reqs, lp=level_prefix: (
                    has_all_requirements(state, player, options, reqs)
                    and can_activate_golden_gobbo(state, player, options, lp)
                ),
            )
        elif reqs:
            # Leave the first two Colored Crystals in whatever stage is behind Sailor Village Door 1
            # accessible without abilities so Sphere 1 is always open even with Door Shuffle enabled.
            if (
                loc.parent_region
                and loc.parent_region.name == first_door_region_name
                and (loc.name.endswith(" - Red Crystal") or loc.name.endswith(" - Blue Crystal"))
            ):
                continue
            set_rule(
                loc,
                lambda state, reqs=reqs: has_all_requirements(state, player, options, reqs),
            )

    # 4. Goal / Win Conditions
    def can_defeat_dante(state: CollectionState) -> bool:
        return state.can_reach_location("Inca Village - Defeat Dante's Final Fight", player)

    def has_required_jigsaws(state: CollectionState) -> bool:
        return state.has("Jigsaw Piece", player, options.jigsaw_pieces_required.value)

    def has_required_trophies(state: CollectionState) -> bool:
        return state.has("Trophy", player, options.trophies_required.value)

    if options.goal.value == Goal.option_defeat_baron_dante:
        multiworld.completion_condition[player] = lambda state: can_defeat_dante(state)
    elif options.goal.value == Goal.option_collect_jigsaw_pieces:
        multiworld.completion_condition[player] = lambda state: has_required_jigsaws(state)
    elif options.goal.value == Goal.option_trophy_hunt:
        multiworld.completion_condition[player] = lambda state: has_required_trophies(state)
    elif options.goal.value == Goal.option_all_of_the_above:
        multiworld.completion_condition[player] = lambda state: (
            can_defeat_dante(state)
            and has_required_jigsaws(state)
            and has_required_trophies(state)
        )

