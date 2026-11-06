"""Location definitions and lookup tables for the Croc 2 Archipelago World."""

import json
import pkgutil
from typing import Dict, List, Set
from BaseClasses import Location


class Croc2Location(Location):
    game: str = "Croc 2"


def _load_locations_data() -> List[dict]:
    raw = pkgutil.get_data(__package__ or "worlds.croc2", "data/locations.json")
    if raw is None:
        raise RuntimeError("Failed to load worlds/croc2/data/locations.json")
    return json.loads(raw.decode("utf-8"))


def _load_level_mapping() -> dict:
    raw = pkgutil.get_data(__package__ or "worlds.croc2", "data/level_mapping.json")
    if raw is None:
        raise RuntimeError("Failed to load worlds/croc2/data/level_mapping.json")
    return json.loads(raw.decode("utf-8"))


LEVEL_MAPPING: dict = _load_level_mapping()


def _build_full_location_table() -> List[dict]:
    base_locs = _load_locations_data()
    # Dynamically append the 2,000 individual White Crystal locations (1..100 across all 20 stages)
    # IDs: 7211001 .. 7213000 (stage_idx * 100 + crystal_idx)
    stage_idx = 0
    for hub in LEVEL_MAPPING["hubs"]:
        hub_name = hub["name"]
        for lvl in hub["levels"]:
            door_num = lvl["door_num"]
            region_name = f"{hub_name} - {lvl['name']}"
            wad = lvl["wad"]
            door_id = lvl["door_id"]
            reqs = list(lvl.get("requires", []))
            for c_idx in range(1, 101):
                loc_id = 7211000 + stage_idx * 100 + c_idx
                # The first 25 White Crystals in any stage are reachable right from the entrance without abilities
                c_reqs = [] if c_idx <= 25 else reqs
                base_locs.append({
                    "id": loc_id,
                    "name": f"{lvl['name']} - White Crystal {c_idx}",
                    "region": region_name,
                    "category": "White Crystals",
                    "wad": wad,
                    "door_id": door_id,
                    "crystal_index": c_idx,
                    "stage_index": stage_idx,
                    "requires": c_reqs,
                })
            stage_idx += 1
    return base_locs


LOCATION_TABLE: List[dict] = _build_full_location_table()

LOCATION_NAME_TO_ID: Dict[str, int] = {entry["name"]: entry["id"] for entry in LOCATION_TABLE}
LOCATION_ID_TO_NAME: Dict[int, str] = {entry["id"]: entry["name"] for entry in LOCATION_TABLE}
LOCATION_NAME_TO_DATA: Dict[str, dict] = {entry["name"]: entry for entry in LOCATION_TABLE}

LOCATION_NAME_GROUPS: Dict[str, Set[str]] = {}
for _entry in LOCATION_TABLE:
    _cat = _entry.get("category", "Miscellaneous")
    LOCATION_NAME_GROUPS.setdefault(_cat, set()).add(_entry["name"])

