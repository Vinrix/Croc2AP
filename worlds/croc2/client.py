"""Archipelago Launcher component registration for the Croc 2 Client."""

from typing import Optional
from worlds.LauncherComponents import Component, Type, components, launch_subprocess


def launch_croc2_client(*args: str) -> None:
    from . import client_bridge
    launch_subprocess(client_bridge.run_client, name="Croc2Client", args=args)


components.append(
    Component(
        "Croc 2 Client",
        func=launch_croc2_client,
        component_type=Type.CLIENT,
        description="Connects Croc 2 (Steam / PC x64) to an Archipelago Multiworld server.",
    )
)

