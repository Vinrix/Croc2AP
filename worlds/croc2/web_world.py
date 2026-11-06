"""WebHost configuration and documentation links for the Croc 2 Archipelago World."""

from BaseClasses import Tutorial
from worlds.AutoWorld import WebWorld
from .options import option_groups


class Croc2WebWorld(WebWorld):
    theme = "jungle"
    option_groups = option_groups
    rich_text_options_doc = True

    tutorials = [
        Tutorial(
            "Multiworld Setup Guide",
            "A step-by-step guide to installing and playing the Croc 2 Archipelago Randomizer with Zero-Friction Vanilla Toggle.",
            "English",
            "setup_en.md",
            "setup/en",
            ["Croc2AP"],
        )
    ]

