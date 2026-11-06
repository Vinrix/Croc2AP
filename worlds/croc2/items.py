"""Item definitions and lookup tables for the Croc 2 Archipelago World."""

import json
import pkgutil
from typing import Dict, List, Set
from BaseClasses import Item, ItemClassification


class Croc2Item(Item):
    game: str = "Croc 2"


def _load_items_data() -> List[dict]:
    raw = pkgutil.get_data(__package__ or "worlds.croc2", "data/items.json")
    if raw is None:
        raise RuntimeError("Failed to load worlds/croc2/data/items.json")
    return json.loads(raw.decode("utf-8"))


ITEM_TABLE: List[dict] = _load_items_data()

CLASSIFICATION_MAP: Dict[str, ItemClassification] = {
    "progression": ItemClassification.progression,
    "useful": ItemClassification.useful,
    "filler": ItemClassification.filler,
    "trap": ItemClassification.trap,
}

ITEM_NAME_TO_ID: Dict[str, int] = {entry["name"]: entry["id"] for entry in ITEM_TABLE}
ITEM_ID_TO_NAME: Dict[int, str] = {entry["id"]: entry["name"] for entry in ITEM_TABLE}
ITEM_NAME_TO_DATA: Dict[str, dict] = {entry["name"]: entry for entry in ITEM_TABLE}

ITEM_NAME_GROUPS: Dict[str, Set[str]] = {}
for _entry in ITEM_TABLE:
    _group = _entry.get("group", "Miscellaneous")
    ITEM_NAME_GROUPS.setdefault(_group, set()).add(_entry["name"])

TRAP_ITEMS: List[str] = [
    entry["name"] for entry in ITEM_TABLE if entry["classification"] == "trap"
]

FILLER_ITEMS: List[str] = [
    entry["name"] for entry in ITEM_TABLE if entry["classification"] == "filler"
]

