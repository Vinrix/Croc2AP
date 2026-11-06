"""Options for the Croc 2: Kingdom of the Gobbos Archipelago Randomizer."""

from dataclasses import dataclass
from Options import (
    Choice,
    DeathLink,
    DefaultOnToggle,
    OptionGroup,
    PerGameCommonOptions,
    Range,
    StartInventoryPool,
    Toggle,
)


class Goal(Choice):
    """
    Set the win condition required to complete the seed.

    - **defeat_baron_dante**: Open the portal to Dante's Sanctuary and defeat Baron Dante in Dante's Final Fight.
    - **collect_jigsaw_pieces**: Collect the required number of Village Hub Jigsaw Pieces (`jigsaw_pieces_required`).
    - **trophy_hunt**: Collect the required number of Trophies from races, challenges, and secret levels (`trophies_required`).
    - **all_of_the_above**: 100% Mastery — Defeat Baron Dante **AND** collect the required Jigsaw Pieces **AND** collect the required Trophies.
    """
    display_name = "Goal"
    rich_text_doc = True
    option_defeat_baron_dante = 0
    option_collect_jigsaw_pieces = 1
    option_trophy_hunt = 2
    option_all_of_the_above = 3
    default = 0


class JigsawPiecesRequired(Range):
    """
    Number of Jigsaw Pieces required to open the portal to Baron Dante's Castle and satisfy Jigsaw win conditions.
    There are 4 total Jigsaw Pieces in the game (1 per Village Hub: Sailor, Cossack, Caveman, and Inca).
    """
    display_name = "Jigsaw Pieces Required"
    rich_text_doc = True
    range_start = 1
    range_end = 4
    default = 4


class TrophiesRequired(Range):
    """
    Number of Trophies required when the Goal is set to **trophy_hunt** or **all_of_the_above**.
    There are 16 Trophies available in the pool.
    """
    display_name = "Trophies Required"
    rich_text_doc = True
    range_start = 1
    range_end = 16
    default = 12


class ColoredCrystalsMode(Choice):
    """
    Configure how the 5 Colored Crystals per level (Red, Blue, Green, Yellow, Purple) are handled.

    - **vanilla**: Colored Crystals remain in their vanilla stage positions and are not shuffled into the multiworld.
    - **shuffled_in_pool**: Each of the 5 Colored Crystals per stage is an individual item and location check in the multiworld pool (e.g., *Sailor Village Door 3 - Blue Crystal*). Once all 5 crystals for a stage are received, that stage's **Golden Gobbo** is automatically activated!
    - **auto_grant_on_enter**: All 5 Colored Crystals for a stage are automatically granted whenever Croc enters that stage.
    """
    display_name = "Colored Crystals Mode"
    rich_text_doc = True
    option_vanilla = 0
    option_shuffled_in_pool = 1
    option_auto_grant_on_enter = 2
    default = 1


class ShuffleWhiteCrystals(Choice):
    """
    Configure how regular White Crystals in each level are randomized:

    - **none**: Regular White Crystals only award crystal currency and do not act as location checks.
    - **milestones_50**: 2 checks per level (at 50 and 100 crystals collected, 40 checks total).
    - **milestones_10**: 10 checks per level (every 10 crystals collected: 10, 20, 30, ..., 100; 200 checks total).
    - **all_100**: Every individual White Crystal (1 to 100 per stage) is a unique location check (2,000 checks total).
    """
    display_name = "White Crystals Checks"
    rich_text_doc = True
    option_none = 0
    option_milestones_50 = 1
    option_milestones_10 = 2
    option_all_100 = 3
    default = 3


class LockLevelAccess(DefaultOnToggle):
    """
    Lock stage doors in each Village Hub behind stage-specific Level Unlock Keys in the item pool.
    Croc starts with access to Sailor Village Door 1 so early checks are always reachable.
    """
    display_name = "Lock Level Access"
    rich_text_doc = True


class ShuffleAbilities(DefaultOnToggle):
    """
    Shuffle Croc's core movement and action abilities into the multiworld pool:
    - **Butt Stomp** (aerial stomp / pound attack)
    - **Tail Whack** (360-degree tail spin attack)
    - **Climbing Claws** (ability to climb ropes, ladders, monkey bars, and climbable walls)
    - **Power Gloves** (ability to pick up and carry crates, pots, snowballs, and wheels when walking up to them)
    """
    display_name = "Shuffle Abilities"
    rich_text_doc = True


class ShuffleLevers(DefaultOnToggle):
    """
    Require the **Lever Wrench** item from the multiworld before Croc can switch levers and mechanical triggers.
    """
    display_name = "Shuffle Levers"
    rich_text_doc = True


class ShuffleVehicles(DefaultOnToggle):
    """
    Require the **Vehicle License** item from the multiworld to drive vehicles (Minecarts, Speedboats, Race Cars, Hot Air Balloons, Hang Gliders, and Airplanes).
    """
    display_name = "Shuffle Vehicles"
    rich_text_doc = True


class ShufflePlaneGuns(DefaultOnToggle):
    """
    Require the **Airplane Ammo Feed** item from the multiworld to shoot projectiles while flying in airplane stages.
    """
    display_name = "Shuffle Plane Guns"
    rich_text_doc = True


class ShufflePeteStore(Choice):
    """
    Configure how Swap Meet Pete's General Store is randomized across the 4 Village Hubs.

    - **vanilla**: Pete's store is open by default and sells vanilla items for crystals.
    - **shuffle_shop_items**: Requires each Village's **Pete's VIP Pass** AND replaces Pete's shop inventory (Heart Pots, Clockwork Gobbos, Gummi Jumps, Jelly Jumps) with multiworld location checks.
    """
    display_name = "Swap Meet Pete Shuffle"
    rich_text_doc = True
    option_vanilla = 0
    option_shuffle_shop_items = 1
    default = 1


class DoorShuffle(Choice):
    """
    Randomize which stage door leads to which level across the Village Hubs.

    - **off**: All stage doors lead to their vanilla levels.
    - **within_village**: Stage doors are shuffled amongst levels within the same Village Hub.
    - **any_village**: Stage doors can lead to any normal level across any of the 4 Village Hubs.
    """
    display_name = "Door Shuffle"
    rich_text_doc = True
    option_off = 0
    option_within_village = 1
    option_any_village = 2
    default = 1


class TrapWeight(Range):
    """
    Percentage chance (0 to 100) that filler items in the pool are replaced with Traps.
    Traps include:
    - **Fire Breakout Trap** (3 burning ticks; extinguished immediately by performing a **front flip**)
    - **Tail-Knot Trap** (ties Croc's tail, disabling tail attacks for 10s or until a ground stomp)
    - **Lead Boots Trap** (weighs down Croc, suppressing jumps for 8 seconds)
    - **Ice Physics Trap** (zero surface friction for 15 seconds)
    - **Reverse Controls Trap** (inverted directional movement for 10 seconds)
    - **Darkness Trap** (shrouds visibility with darkness for 10 seconds)
    - **Pete's Crystal Tax Trap** (Swap Meet Pete levies a 20 crystal tax from your wallet!)
    - **Dantini Ambush Trap** (spawns 3 Dantinis around Croc)
    - **Speed Distortion Trap** (random hyper-speed or slow-motion for 12 seconds)
    - **Health Drain Trap** (deducts 1 heart)
    """
    display_name = "Trap Weight"
    rich_text_doc = True
    range_start = 0
    range_end = 100
    default = 15


class FireBreakoutTrapEnabled(DefaultOnToggle):
    """
    Include the **Fire Breakout Trap** (inspired by *Burning Down the Forest*) when traps are enabled.
    Ignites Croc's tail for 3 ticks before losing 1 heart. Performing a **front flip** pats out the flames immediately without taking damage!
    """
    display_name = "Enable Fire Breakout Trap"
    rich_text_doc = True


option_groups = [
    OptionGroup("Seed Settings", [
        Goal,
        JigsawPiecesRequired,
        TrophiesRequired,
        DeathLink,
    ]),
    OptionGroup("Ability & Access Locks", [
        LockLevelAccess,
        ShuffleAbilities,
        ShuffleLevers,
        ShuffleVehicles,
        ShufflePlaneGuns,
        ShufflePeteStore,
    ]),
    OptionGroup("Item & Location Pool", [
        ColoredCrystalsMode,
        ShuffleWhiteCrystals,
    ]),
    OptionGroup("World & Door Changes", [
        DoorShuffle,
    ]),
    OptionGroup("Traps", [
        TrapWeight,
        FireBreakoutTrapEnabled,
    ]),
]


@dataclass
class Croc2Options(PerGameCommonOptions):
    goal: Goal
    jigsaw_pieces_required: JigsawPiecesRequired
    trophies_required: TrophiesRequired
    colored_crystals_mode: ColoredCrystalsMode
    shuffle_white_crystals: ShuffleWhiteCrystals
    lock_level_access: LockLevelAccess
    shuffle_abilities: ShuffleAbilities
    shuffle_levers: ShuffleLevers
    shuffle_vehicles: ShuffleVehicles
    shuffle_plane_guns: ShufflePlaneGuns
    shuffle_pete_store: ShufflePeteStore
    door_shuffle: DoorShuffle
    trap_weight: TrapWeight
    fire_breakout_trap: FireBreakoutTrapEnabled
    start_inventory_from_pool: StartInventoryPool
    death_link: DeathLink

