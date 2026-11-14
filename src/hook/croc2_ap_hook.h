#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <mutex>

namespace croc2_ap {

// Authentic RVAs extracted from croc2.exe (x86_64 Steam release, ImageBase 0x140000000)
constexpr uintptr_t RVA_JIGSAW_COUNT_REF      = 0x002187D6;
constexpr uintptr_t RVA_CRYSTAL_COUNT_REF     = 0x00218559;
constexpr uintptr_t RVA_GOBBO_COUNT_REF       = 0x001D1DA1;
constexpr uintptr_t RVA_CHEAT_ACTIVATED_REF   = 0x00218D67;
constexpr uintptr_t RVA_CRYSTAL_PATCHER_TABLE = 0x007F6F30;
constexpr uintptr_t RVA_WAD_PATH_FORMAT       = 0x007F727A;
constexpr uintptr_t RVA_HUD_INVENTORY_TABLE   = 0x0081B400;
constexpr uintptr_t RVA_ACTION_PROMPT_ENUMS   = 0x0081B9A0;

// Exact croc2.exe Function RVAs (disassembled from x64 Steam binary)
constexpr uintptr_t RVA_FN_ADD_HEART_POT         = 0x00028A60;
constexpr uintptr_t RVA_FN_TAKE_HEART_DAMAGE     = 0x00028AA0;
constexpr uintptr_t RVA_FN_HURT_CROC             = 0x00065F20; // strat opcode 155 "stReset_to_checkpoint": heart loss + teleport to checkpoint
constexpr uintptr_t RVA_FN_HURT_CROC_HIT         = 0x00065BD0; // strat opcode 154: the heart loss an enemy hit runs on Croc's own strat
constexpr uintptr_t RVA_FN_CALC_SAVE_INDEX       = 0x0002B9C0;
constexpr uintptr_t RVA_FN_ON_DOOR_TRANSITION    = 0x0002BA00;
constexpr uintptr_t RVA_FN_READ_INPUT            = 0x00040990;
constexpr uintptr_t RVA_FN_HUD_POPUP_COLORED     = 0x00041400;
constexpr uintptr_t RVA_FN_HUD_POPUP_CRYSTALS    = 0x00041420;
constexpr uintptr_t RVA_FN_HUD_POPUP_JIGSAW      = 0x000414F0; // called by opcode 248 (jigsaw piece)
constexpr uintptr_t RVA_FN_HUD_POPUP_OP250       = 0x00041520; // called by opcode 250, which no script runs
constexpr uintptr_t RVA_FN_HUD_POPUP_GOLDEN_GOBBO = 0x00041530; // called by opcode 249 (Golden Gobbo)
constexpr uintptr_t RVA_FN_ADD_INVENTORY_ITEM    = 0x00041600;
constexpr uintptr_t RVA_FN_ADD_COLORED_CRYSTAL   = 0x00041D90;
constexpr uintptr_t RVA_FN_HUD_POPUP_HEARTS      = 0x000440F0;
constexpr uintptr_t RVA_FN_REBUILD_INV_WHEEL     = 0x00045D30;
constexpr uintptr_t RVA_FN_OP_GOLDEN_GOBBO       = 0x00065F90; // opcode 249: Golden Gobbo counter += 1
constexpr uintptr_t RVA_FN_OP_250                = 0x00065FD0; // opcode 250: counter 0xBA9B34 += 1 (no script runs it)
constexpr uintptr_t RVA_FN_OP_REWARD_100         = 0x00065FE0; // opcode 270: wallet += 100 when the reward flag is set
constexpr uintptr_t RVA_FN_OP_JIGSAW_PIECE       = 0x00066010; // opcode 248: jigsaw piece counter += 1
constexpr uintptr_t RVA_FN_GIVE_HEART_POT_NAT    = 0x00066120;
constexpr uintptr_t RVA_FN_GIVE_INV_ITEM_NAT     = 0x00062CD0; // Opcode 172 handler: GiveInventoryItem(type)
constexpr uintptr_t RVA_FN_SET_ACHIEVEMENT       = 0x0025FF00; // AchievementManager::SetAchievementValue(std::string*, int)
constexpr uintptr_t RVA_FN_INCREMENT_ACHIEVEMENT = 0x0025F900; // AchievementManager::IncrementAchievement(std::string*, int)
constexpr uintptr_t RVA_FN_LOAD_MAP              = 0x00070B60;
constexpr uintptr_t RVA_FN_SAVE_GAME_SLOT        = 0x001DCE40;
constexpr uintptr_t RVA_FN_SET_ACTION_STATE      = 0x0022E290; // SetActionState(void* strat, int32_t action_id)
constexpr uintptr_t RVA_FN_STRAT_VM_PUSH_INT     = 0x0005D8A0; // StratVM_PushInt(void* strat, int32_t val)
constexpr uintptr_t RVA_FN_CLIMB_WALL_IDLE       = 0x001FAA90; // Opcode 370: Wall/Vine/Ladder can-climb check
constexpr uintptr_t RVA_CLIMB_WALL_IDLE_SLOT     = 0x007F9FB0;
constexpr uintptr_t RVA_FN_CLIMB_WALL_ATTACH     = 0x001FAC30; // Opcode 371: Wall attach
constexpr uintptr_t RVA_CLIMB_WALL_ATTACH_SLOT   = 0x007F9FB8;
constexpr uintptr_t RVA_FN_CLIMB_WALL_DETACH     = 0x001FAC70; // Opcode 372: Wall/Vine/Ladder climb detach
constexpr uintptr_t RVA_CLIMB_WALL_DETACH_SLOT   = 0x007F9FC0;
constexpr uintptr_t RVA_FN_CLIMB_SURFACE_CHECK   = 0x001FACE0; // Opcode 375: Climbable surface / bars attach check
constexpr uintptr_t RVA_CLIMB_SURFACE_CHECK_SLOT = 0x007F9FD8;
constexpr uintptr_t RVA_FN_CLIMB_STATE_INIT      = 0x001FAD10; // Opcode 376: Wall state init
constexpr uintptr_t RVA_CLIMB_STATE_INIT_SLOT    = 0x007F9FE0;
constexpr uintptr_t RVA_FN_CLIMB_WALL_MOVE       = 0x001FAD40; // Opcode 377: Wall/Vine/Ladder climb movement
constexpr uintptr_t RVA_CLIMB_WALL_MOVE_SLOT     = 0x007F9FE8;
constexpr uintptr_t RVA_FN_TRY_ATTACH_CLIMB      = 0x00025C80;
constexpr uintptr_t RVA_FN_CLIMB_REJECT_FALLBACK = 0x00069C20;
constexpr uintptr_t RVA_CLIMB_DISPATCH_SLOT      = 0x007FA070;
constexpr uintptr_t RVA_CLIMB_SURFACE_TYPE       = 0x00BC8310; // int32_t non-zero when touching climbable surface
constexpr uintptr_t RVA_CLIMB_WALL_STATE_2       = 0x00BC832C; // int32_t cleared when climb disallowed
constexpr uintptr_t RVA_CLIMB_ALLOWED_BYTE       = 0x00BD1361; // uint8_t 1 when allowed to grab wall
constexpr uintptr_t RVA_CLIMB_TOUCH_SURFACE_FLAG = 0x00BD1930; // int32_t non-zero when touching climbable wall/vine
constexpr uintptr_t RVA_CLIMB_ACTIVE_STATE_FLAG  = 0x00BD19A0; // int32_t 1 when climbing wall/vine

// Strat VM pickup opcodes and their dispatch table slots. What each one does was read from its handler and
// from the scripts that run it (docs/level_objects.md, "Every pickup, traced").
constexpr uintptr_t RVA_STRAT_VM_DISPATCH_TABLE    = 0x007F9420;
constexpr uintptr_t RVA_STRAT_VM_HEART_POT_SLOT    = 0x007F9978; // Opcode 171: add a heart pot (Heart Pot objects, Swap Meet Pete)
constexpr uintptr_t RVA_STRAT_VM_INV_ITEM_SLOT     = 0x007F9980; // Opcode 172 <type>: give an inventory item (types below)
constexpr uintptr_t RVA_STRAT_VM_JIGSAW_SLOT       = 0x007F9BE0; // Opcode 248: jigsaw piece (run only by JigsawPiece)
constexpr uintptr_t RVA_STRAT_VM_GOLDEN_GOBBO_SLOT = 0x007F9BE8; // Opcode 249: Golden Gobbo (run only by Golden Gobbo / Special Golden Gobbo)
constexpr uintptr_t RVA_STRAT_VM_OP250_SLOT        = 0x007F9BF0; // Opcode 250: no script runs it
constexpr uintptr_t RVA_STRAT_VM_REWARD_100_SLOT   = 0x007F9C90; // Opcode 270: 100 crystals to the wallet (hub reward characters)

// NOT sound effects: these are the AchievementManager checks the game's pickup handlers tail-call
// (each builds an achievement ID and calls RVA_FN_SET_ACHIEVEMENT). Calling them unlocks real Steam
// achievements, so the mod must never use them as pickup feedback. Kept only as documentation.
constexpr uintptr_t RVA_FN_ACH_CHECK_GOLDEN_GOBBO = 0x00260DE0; // tail of opcode 249; sets "firstGoldenGobbo"
constexpr uintptr_t RVA_FN_ACH_CHECK_JIGSAW      = 0x00260F90; // tail of opcode 248
constexpr uintptr_t RVA_FN_ACH_CHECK_REWARD_100  = 0x00261060; // tail of opcode 270
constexpr uintptr_t RVA_FN_ACH_CHECK_INV_ITEM    = 0x00260DD0; // tail of opcode 172, passed the item type

// Exact croc2.exe Global State RVAs
constexpr uintptr_t RVA_ACTIVE_SAVE_SLOT         = 0x00B4D5A8; // int32_t save slot index (0..4)
constexpr uintptr_t RVA_PAUSE_MENU_ACTIVE        = 0x00B8E018;
constexpr uintptr_t RVA_COLORED_CRYSTAL_PTR      = 0x00B8E028; // Points to int32_t (0x1000 per crystal, 0x5000 = all 5)
constexpr uintptr_t RVA_FN_STRAT_READ_VALUE      = 0x00067EA0; // push game value N (opcode 4 and 9 others)
constexpr uintptr_t PLAYER_GROUND_FLAGS_OFF      = 0x100;      // Croc strat flags word; bit 1 set while on the ground (recorded in-game)
constexpr uint32_t  PLAYER_ON_GROUND_BIT         = 0x2;
constexpr uintptr_t RVA_TRANSITION_STATE         = 0x00B90D30; // 7 = door transition triggered
constexpr uintptr_t RVA_CURRENT_MAP_HEADER       = 0x00B90D70; // Loaded map header pointer
constexpr uintptr_t RVA_ACTIVE_SPAWN_TRIGGER      = 0x00B90D78; // Pointer to active spawn/entrance trigger
constexpr uintptr_t RVA_CLEAN_SAVE_TEMPLATE      = 0x00B90D88; // 0x17A30-byte clean save template copied on New Game
constexpr uintptr_t RVA_SAVE_DATA_TABLE          = 0x00BA9880; // 0x17A30-byte active save table; completion bit at + idx + 0x304
constexpr uintptr_t RVA_RANDO_SAVE_FOOTER_OFF    = 0x00017800; // Embedded randomizer seed/state footer inside save table
constexpr uintptr_t RVA_HEALTH_STRUCT            = 0x00BA9A88; // +0x0 max_hearts, +0x4 curr_hearts, +0x8 bonus_hearts
constexpr uintptr_t RVA_WALLET_CRYSTALS          = 0x00BA9A9C; // Saved wallet white crystals
constexpr uintptr_t RVA_CURR_MAP_TRIBE           = 0x00BA9B04; // int32_t tribe (1..5)
constexpr uintptr_t RVA_CURR_MAP_LEVEL           = 0x00BA9B08; // int32_t level (1=Hub, 2..7=Stages)
constexpr uintptr_t RVA_CURR_MAP_SUBMAP          = 0x00BA9B0C; // int32_t submap (1..999)
constexpr uintptr_t RVA_CURR_MAP_TYPE            = 0x00BA9B10; // int32_t map_type (0=L, 1=B, 2=S, 3=I)
constexpr uintptr_t RVA_STAGE_COLORED_MASK       = 0x00BA9B24; // int32_t colored crystal mask
constexpr uintptr_t RVA_COUNT_GOLDEN_GOBBOS      = 0x00BA9B30; // int32_t Golden Gobbos (opcode 249 adds 1)
constexpr uintptr_t RVA_COUNT_OP250              = 0x00BA9B34; // int32_t counter of opcode 250, which no script runs
constexpr uintptr_t RVA_COUNT_JIGSAW_PIECES      = 0x00BA9B38; // int32_t jigsaw pieces (opcode 248 adds 1)
constexpr uintptr_t RVA_LEVEL_CRYSTALS           = 0x00BA9B3C; // Current in-level white crystals
// Inventory counters by item type, from AddInventoryItem's own switch (0x140041600) and the scripts that give them.
constexpr uintptr_t RVA_INV_BINOCULARS           = 0x00BA9B40; // type 2: binoculars (an object in Sailor Village gives them)
constexpr uintptr_t RVA_INV_KEYS                 = 0x00BA9B44; // type 3: keys (the Key objects)
constexpr uintptr_t RVA_INV_STRAWBERRY_JELLIES   = 0x00BA9B48; // type 5: strawberry jelly (80 crystals at Pete's), max 9
constexpr uintptr_t RVA_INV_ORANGE_JELLIES       = 0x00BA9B4C; // type 7: orange jelly (40 crystals), max 9
constexpr uintptr_t RVA_INV_LIME_JELLIES         = 0x00BA9B50; // type 6: lime jelly (60 crystals), max 9
constexpr uintptr_t RVA_INV_CARRIED              = 0x00BA9B54; // types 12, 13, 14 share it: what Croc is carrying (12 a Gobbo, 14 water)
constexpr uintptr_t RVA_INV_CLOCKWORK            = 0x00BA9B58; // type 8: Clockwork Gobbo (50 crystals), max 9
constexpr uintptr_t RVA_INV_TYPE_4               = 0x00BA9B5C; // type 4: no script gives it
constexpr uintptr_t RVA_INV_WHEELS               = 0x00BA9B60; // type 10: racer wheels (jungle Wheel objects, mine wheel blocks)
constexpr uintptr_t RVA_INV_WHEEL_SLOT_COUNT     = 0x00BA9B64; // Number of active non-zero item types in inventory wheel
constexpr uintptr_t RVA_INV_WHEEL_CUR_ITEM_ID    = 0x00BA9B68; // Currently selected item_type in inventory wheel
constexpr uintptr_t RVA_PREV_MAP_TRIBE           = 0x00BA9B6C; // int32_t prev_tribe
constexpr uintptr_t RVA_PREV_MAP_LEVEL           = 0x00BA9B70; // int32_t prev_level
constexpr uintptr_t RVA_PREV_MAP_SUBMAP          = 0x00BA9B74; // int32_t prev_submap
constexpr uintptr_t RVA_PREV_MAP_TYPE            = 0x00BA9B78; // int32_t prev_map_type
constexpr uintptr_t RVA_REWARD_100_FLAG          = 0x00BA9B7C; // int32_t opcode 270 pays its 100 crystals only when this is set; what sets it isn't known
constexpr uintptr_t RVA_HELD_BUTTONS             = 0x00BC4B54; // PS1 PAD bitmask (0x8000=Tail, 0x4000=Jump/Stomp, 0x0C00=Flip)
constexpr uintptr_t RVA_PREV_30HZ_BUTTONS        = 0x00BC4B58; // Previous 30Hz logic tick held buttons
constexpr uintptr_t RVA_PRESSED_BUTTONS          = 0x00BC4B5C; // 30Hz rising-edge pressed buttons (~prev_30hz & held)
constexpr uintptr_t RVA_STAGE_COLORED_COUNT      = 0x00BC4DAC; // int32_t 0..5 colored crystals collected in current stage
constexpr uintptr_t RVA_INV_WHEEL_STATE          = 0x00BC4E9C; // int32_t 2 = inventory wheel ready to open
constexpr uintptr_t RVA_INV_WHEEL_SEL_INDEX      = 0x00BC4EA4; // int32_t selected slot index in wheel
constexpr uintptr_t RVA_INV_WHEEL_SLOTS_ARRAY    = 0x00BC4EB0; // 0x1C-byte slot entries in live inventory wheel
constexpr uintptr_t RVA_PLAYER_PTR               = 0x00BC7CE8; // Pointer to active PlayerStrat instance
constexpr uintptr_t RVA_STRAT_LIST_HEAD          = 0x00BC7D20; // First live strat; next = *(strat + 8) (interpreter loop at 0x14005E7F0)
constexpr uintptr_t RVA_MAP_FILENAME_STR         = 0x00BC8710;
constexpr uintptr_t RVA_CROC_STRAT_DEF           = 0x00BD1540; // Pointer to Croc's on-foot Strat definition
constexpr uintptr_t RVA_WAD_BUFFER_BASE          = 0x00BD25C0;
constexpr uintptr_t RVA_WAD_BUFFER_LIMIT         = 0x00BD25C8;

// Colored Crystal bitflags per level (all 5 = 0x1F -> activates Golden Gobbo)
constexpr uint8_t CRYSTAL_RED    = 1 << 0; // 0x01
constexpr uint8_t CRYSTAL_BLUE   = 1 << 1; // 0x02
constexpr uint8_t CRYSTAL_GREEN  = 1 << 2; // 0x04
constexpr uint8_t CRYSTAL_YELLOW = 1 << 3; // 0x08
constexpr uint8_t CRYSTAL_PURPLE = 1 << 4; // 0x10
constexpr uint8_t CRYSTAL_ALL_5  = 0x1F;

// Magic numbers for embedded Save-Slot Randomizer Footer at RVA_SAVE_DATA_TABLE + RVA_RANDO_SAVE_FOOTER_OFF
constexpr uint32_t RANDO_SAVE_MAGIC   = 0x43324150u; // "C2AP" - Randomizer Save (locks mod ON for this save)
constexpr uint32_t VANILLA_SAVE_MAGIC = 0x4332564Eu; // "C2VN" - Vanilla Save
constexpr int SOLO_CHECK_POOL_SIZE    = 180;         // 20 stages * 9 checks per stage

#pragma pack(push, 1)
struct Croc2RandoSaveFooter {
    uint32_t magic = 0;               // RANDO_SAVE_MAGIC or VANILLA_SAVE_MAGIC (0 = fresh/deleted save)
    uint32_t version = 1;
    uint64_t seed = 0;                // Unique 64-bit random seed bound to this save
    uint8_t is_rando_save = 0;        // 1 if created with Mod ON (locks Mod Toggle while in this save)
    uint8_t abilities_mask = 0;       // Bits 0..6: tail, stomp, climb, carry, lever, vehicle, plane_gun
    uint8_t max_heart_pots = 5;
    uint8_t jigsaw_pieces = 0;
    uint8_t golden_gobbos = 0;
    uint8_t trophies = 0;
    uint8_t silver_keys = 0;
    uint8_t gold_keys = 0;
    uint8_t door_perm[20] = {0};      // Shuffled target stage index (0..19) for each of the 20 doors
    uint8_t stage_crystals[20] = {0}; // 5-bit Colored Crystal mask per stage (0..19)
    uint8_t checks_claimed[24] = {0}; // 192-bit bitmask of claimed in-game checks (0..179)
    uint8_t white_crystals_claimed[20][13] = {{0}}; // 100-bit per-stage bitmask of individual White Crystal checks (1..100)
    uint8_t red_jellies = 0;
    uint8_t blue_jellies = 0;
    uint8_t green_jellies = 0;
    uint8_t clockwork_gobbos = 0;
    uint8_t binoculars = 0;
    uint32_t ap_items_applied = 0;    // Number of Archipelago ReceivedItems (by index) already applied to this save
    // Added later; zero on older saves. Bits: 0..19 Golden Gobbo per stage, 20..23 Jigsaw per village,
    // 24..43 Pete's Store slot (village*5 + slot), 44..51 bosses. See EXTRA_CHECK_* in croc2_ap_hook.cpp.
    uint8_t extra_checks[8] = {0};
};
#pragma pack(pop)

// Archipelago Text Client Importance Color Classification
enum class ItemImportance : uint8_t {
    PROGRESSION = 0, // Plum / Bright Orchid (#AF99EF -> RGB(195, 145, 255))
    USEFUL      = 1, // Slate Blue (#6D8BE8 -> RGB(110, 165, 255))
    FILLER      = 2, // Cyan (#00EEEE -> RGB(0, 238, 238))
    TRAP        = 3  // Salmon / Coral Red (#FA8072 -> RGB(255, 115, 95))
};

struct OverlayFeedEntry {
    bool is_outgoing = false; // true = Sent (OUT), false = Received (IN)
    std::string item_name;
    ItemImportance importance = ItemImportance::FILLER;
    std::string player_name;
    std::string location_name;
};

struct BottomToastNotification {
    bool is_outgoing = false;
    bool is_self = false;
    std::string sender = "Croc";
    std::string receiver = "Croc";
    std::string item_name;
    ItemImportance importance = ItemImportance::FILLER;
    std::string location;
    uint32_t duration_ms = 5000;
    uint64_t expire_tick = 0;
};

inline std::string get_solo_peer_name(int stage_idx, int check_id, bool is_sender, const std::vector<std::string>& yaml_players = {}) {
    (void)stage_idx;
    (void)is_sender;
    if (!yaml_players.empty()) {
        int idx = (check_id >= 0) ? (check_id % static_cast<int>(yaml_players.size())) : 0;
        return yaml_players[idx];
    }
    return "WillOfTheVinny";
}

// Fire Breakout Trap state (Instant heart loss + flame animation & knockback)
struct FireTrapState {
    bool active = false;
    float timer = 0.0f; // Visual flame effect and knockback recoil duration (2.0s)
};

struct ActiveTimedTrap {
    std::string trap_type;
    float remaining_seconds = 10.0f; // All ill effects last a flat 10 seconds!
};

struct PlayerAbilities {
    bool tail_whack = false;
    bool butt_stomp = false;
    bool climbing_claws = false;
    bool power_gloves = false;
    bool lever_wrench = false;
    bool vehicle_license = false;
    bool airplane_ammo_feed = false;
    bool long_jump = false;
};

struct ServerConnectionConfig {
    std::string server_host = "localhost";
    int server_port = 38281;
    std::string slot_name = "Player1"; // Case-sensitive!
    std::string password = "";
    std::string status_text = "Not Connected (Press F9 to Connect)";
    bool connecting = false;
    bool ap_server_connected = false;
};

struct ModState {
    // Zero-Friction Vanilla Toggle & Save-Lock flags
    bool config_enabled = true;
    bool forced_vanilla_cmdline = false;
    bool client_connected = false;
    // Set once an Archipelago client (launcher / Croc2Client) configures a multiworld slot over the
    // localhost bridge. While set, checks are only reported to the server, never granted locally.
    bool multiworld_session = false;
    bool death_link_enabled = false;
    bool debug_watch = false;               // croc2_mod.ini debug_watch = 1: log detection signals for testing
    bool death_link_kill_pending = false;   // set by a received DeathLink, applied on the game thread
    uint32_t death_link_grace_until = 0;    // GetTickCount() before which a death is ours, not reported
    bool manual_override_disabled = false;
    bool in_active_save = false;
    bool save_locked_to_mod = false; // True when playing a save created with the mod enabled!
    int active_save_slot = 0;
    uint64_t current_save_seed = 0;
    std::string current_seed_hex = "NONE";
    uint16_t tcp_port = 43825;
    int toggle_hotkey_vk = 0x79; // VK_F10
    int menu_hotkey_vk = 0x78;   // VK_F9
    bool overlay_full_visible = false; // Overlay collapsed by default; F10 toggles full console view
    int overlay_toggle_hotkey_vk = 0x79; // VK_F10

    BottomToastNotification active_toast;
    bool toast_active = false;
    std::deque<BottomToastNotification> toast_queue;

    ServerConnectionConfig conn;
    std::string toast_message = "Croc 2 Archipelago Mod Loaded! Press F9 for Server Menu, F10 to Toggle Vanilla.";
    uint64_t toast_expire_tick = 0;
    std::vector<std::string> activity_log;
    std::vector<OverlayFeedEntry> item_feed; // Color-coordinated Archipelago incoming/outgoing item feed
    std::vector<int> ap_received_item_ids;   // Persistent list of AP item IDs received in this session
    int ap_items_applied = 0;                // AP item index applied to the active save (dedupes re-sends)

    // Archipelago Player Names (Discovered from custom YAMLs e.g. WillOfTheVinny or multiworld session)
    std::vector<std::string> custom_yaml_players;
    std::unordered_map<int, std::string> slot_to_player_name;
    std::string yaml_players_dir = "C:\\ProgramData\\Archipelago\\Players";
    int our_slot_id = 1;

    std::string get_default_yaml_receiver() const {
        if (!custom_yaml_players.empty()) {
            return custom_yaml_players[0];
        }
        return "WillOfTheVinny";
    }

    std::string get_player_name_by_slot(int slot_id) const {
        auto it = slot_to_player_name.find(slot_id);
        if (it != slot_to_player_name.end() && !it->second.empty()) {
            return it->second;
        }
        if (!custom_yaml_players.empty()) {
            int idx = (slot_id >= 0 ? slot_id : 0) % static_cast<int>(custom_yaml_players.size());
            return custom_yaml_players[idx];
        }
        return "WillOfTheVinny";
    }

    // Session configuration from Archipelago Slot Data
    int colored_crystals_mode = 1;
    int shuffle_white_crystals = 3;
    bool lock_level_access = true;
    bool shuffle_abilities = true;
    bool shuffle_levers = true;
    bool shuffle_vehicles = true;
    bool shuffle_plane_guns = true;
    int shuffle_pete_store = 1;

    // Solo / Seed-linked check pool (180 logical checks + 2000 individual white crystal checks across all 20 stages)
    uint8_t door_perm[20] = {0};
    int solo_check_items[SOLO_CHECK_POOL_SIZE] = {0};
    int solo_white_crystal_items[2000] = {0};
    uint8_t checks_claimed[24] = {0};
    uint8_t extra_checks[8] = {0};
    uint8_t white_crystals_claimed[20][13] = {{0}};

    // Player runtime state
    std::string current_wad = "T1L1M001";
    std::string last_door_redirect_desc = "Door 1 -> Random (New Save)";
    int current_hearts = 5;
    int max_heart_pots = 5;
    int current_crystals = 0;
    int jigsaw_pieces = 0;
    int golden_gobbos = 0;
    int trophies = 0;
    int silver_keys = 0;
    int gold_keys = 0;
    int clockwork_gobbos = 0;
    int red_jellies = 0;
    int blue_jellies = 0;
    int green_jellies = 0;
    bool binoculars = false;

    PlayerAbilities abilities;
    std::unordered_set<std::string> vip_passes;
    std::unordered_set<std::string> unlocked_doors;
    std::unordered_map<std::string, uint8_t> stage_colored_crystals;
    std::unordered_set<std::string> activated_golden_gobbos;
    std::unordered_map<std::string, std::string> door_overrides;

    FireTrapState fire_trap;
    std::vector<ActiveTimedTrap> timed_traps;
    std::vector<std::string> pending_events_json;

    // Returns true when mod is enabled (or permanently locked on for the loaded Randomizer Save)
    bool is_mod_enabled_toggle() const {
        if (forced_vanilla_cmdline) return false;
        if (save_locked_to_mod) return true;
        return config_enabled && !manual_override_disabled;
    }

    // Returns true whenever the mod is enabled and active in gameplay
    bool is_randomizer_active() const {
        return is_mod_enabled_toggle();
    }

    std::string get_hud_banner() const {
        if (!is_mod_enabled_toggle()) {
            return "Archipelago Randomizer: Disabled (Vanilla Mode)";
        }
        if (conn.ap_server_connected) {
            return "Archipelago Randomizer: Active (" + conn.slot_name + " @ " + conn.server_host +
                   " | Seed #" + current_seed_hex + ")";
        }
        if (client_connected) {
            return "Archipelago Randomizer: Active (Client Bridge Connected)";
        }
        if (save_locked_to_mod) {
            return "Croc 2 Randomizer: Active (Seed #" + current_seed_hex + " | Locked to Save)";
        }
        return "Croc 2 Randomizer: Enabled - Standby (Press F9 for Menu | F10 Vanilla)";
    }

    void add_log(const std::string& msg) {
        activity_log.push_back(msg);
        if (activity_log.size() > 50) {
            activity_log.erase(activity_log.begin());
        }
        toast_message = msg;
    }

    void add_feed_entry(bool is_outgoing,
                        const std::string& item_name,
                        ItemImportance importance,
                        const std::string& player_name,
                        const std::string& location_name) {
        OverlayFeedEntry entry;
        entry.is_outgoing = is_outgoing;
        entry.item_name = item_name;
        entry.importance = importance;
        entry.player_name = player_name.empty() ? conn.slot_name : player_name;
        entry.location_name = location_name;
        item_feed.push_back(entry);
        if (item_feed.size() > 16) {
            item_feed.erase(item_feed.begin());
        }
    }
};

} // namespace croc2_ap
