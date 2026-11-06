#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>

#include "croc2_ap_hook.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <thread>

namespace croc2_ap {

static ModState g_state;
static std::mutex g_mutex;
static std::atomic<bool> g_running{false};
static std::atomic<bool> g_ui_running{false};
static std::atomic<bool> g_ws_running{false};
static std::thread g_server_thread;
static std::thread g_ui_thread;
static std::thread g_ws_thread;
static SOCKET g_listen_socket = INVALID_SOCKET;
static std::string g_ini_file_path = "croc2_mod.ini";

// UI Window Handles
static HWND g_overlay_hwnd = nullptr;
static HWND g_toast_hwnd = nullptr;
static HWND g_menu_hwnd = nullptr;
static HWND g_edit_host = nullptr;
static HWND g_edit_port = nullptr;
static HWND g_edit_slot = nullptr;
static HWND g_edit_pass = nullptr;
static HWND g_btn_connect = nullptr;
static HWND g_btn_disconnect = nullptr;
static HWND g_btn_toggle_mod = nullptr;
static HWND g_list_log = nullptr;
static bool g_menu_visible = false;

constexpr int IDC_EDIT_HOST      = 1001;
constexpr int IDC_EDIT_PORT      = 1002;
constexpr int IDC_EDIT_SLOT      = 1003;
constexpr int IDC_EDIT_PASS      = 1004;
constexpr int IDC_BTN_CONNECT    = 1005;
constexpr int IDC_BTN_DISCONNECT = 1006;
constexpr int IDC_BTN_TOGGLE_MOD = 1007;
constexpr int IDC_BTN_CLOSE_MENU = 1008;
constexpr int IDC_LIST_LOG       = 1009;

// Forward declarations
static void start_ap_websocket_connection();
static void stop_ap_websocket_connection();
static void save_config_ini(const char* ini_path);
static void load_yaml_player_names();
static void refresh_menu_ui();
static void init_default_door_shuffle_locked();
static void register_door_override_locked(const std::string& door_or_wad, const std::string& target_wad);
static void install_croc2_engine_hooks();
static uint64_t generate_fresh_entropy_seed();
static void generate_new_save_seed_locked(uint64_t seed);
static void sync_state_to_save_footer_locked();
static void restore_state_from_save_footer_locked(const Croc2RandoSaveFooter& footer);
static void claim_solo_check_locked(int stage_idx, int sub_check_idx, const std::string& check_desc);
static void apply_ap_item_by_id_locked(int item_id, const std::string& sender_name = "", const std::string& location_name = "", bool record_persistent = true);
static void get_ap_item_info(int item_id, std::string& out_name, ItemImportance& out_imp);
static void trigger_bottom_toast_locked(bool is_outgoing, const std::string& item_name, ItemImportance imp, const std::string& sender, const std::string& receiver, const std::string& location, uint32_t duration_ms = 4500);
static void send_ws_location_check_locked(int location_id);
static void reset_pending_native_grants();
static std::string json_escape(const std::string& s);

// ============================================================================
// Live croc2.exe Memory & Inline Hook State
// ============================================================================
struct MapCoords {
    int32_t tribe = 1;
    int32_t level = 1;
    int32_t submap = 1;
    int32_t map_type = 0;
};

static uintptr_t g_exe_base = 0;
static bool g_hooks_installed = false;
static bool g_in_redirected_stage = false;
static bool g_transition_already_redirected = false;
static MapCoords g_saved_hub{1, 1, 1, 0};
static MapCoords g_saved_orig_door{1, 2, 1, 0};
static MapCoords g_saved_rand_stage{1, 4, 1, 0};
static std::atomic<int> g_pending_heart_damage{0};
static std::atomic<int> g_pending_heart_pots{0};
static std::atomic<int> g_pending_keys{0};
static std::atomic<int> g_pending_clockwork{0};
static std::atomic<int> g_pending_red_jellies{0};
static std::atomic<int> g_pending_blue_jellies{0};
static std::atomic<int> g_pending_green_jellies{0};
static std::atomic<int> g_pending_binoculars{0};
static std::atomic<int> g_pending_jigsaws{0};
static std::atomic<int> g_pending_golden_gobbos{0};
static std::atomic<int> g_pending_wallet_crystals{0};
static std::atomic<int> g_pending_colored_gems{0};
static std::atomic<bool> g_need_full_inventory_sync{false};
static DWORD g_last_jump_press_tick = 0;
static uint32_t g_prev_raw_buttons = 0;

using FnOnDoorTransition = void(__fastcall*)();
using FnLoadMap = void*(__fastcall*)(int32_t tribe, int32_t level, int32_t submap, int32_t map_type);
using FnReadInput = uint32_t(__fastcall*)(void* input_ctx);
using FnTakeHeartDamage = void(__fastcall*)(void* health_struct);
using FnAddHeartPot = void(__fastcall*)(void* health_struct);
using FnSaveGameSlot = int64_t(__fastcall*)(int32_t slot_idx, void* save_table, void* ctx);
using FnAddInventoryItem = int32_t(__fastcall*)(int32_t item_type, int32_t show_popup, int32_t delta, int32_t set_flag, int32_t set_val);
using FnRebuildInvWheel = void(__fastcall*)();
using FnGiveNativeNoArg = void(__fastcall*)();
using FnAddColoredCrystal = void(__fastcall*)(int32_t color_1_to_5);

static FnOnDoorTransition g_orig_on_door_transition = nullptr;
static FnLoadMap g_orig_load_map = nullptr;
static FnReadInput g_orig_read_input = nullptr;
static FnSaveGameSlot g_orig_save_game_slot = nullptr;

// Canonical table of all 20 Village Stage Doors, their vanilla WADs, exact Archipelago IDs, and authentic in-game names
struct DoorWadEntry {
    const char* door_id;
    const char* vanilla_wad;
    const char* default_shuffled_wad;
    int stage_clear_loc_id;
    const char* level_name;
    const char* hub_name;
    int door_num;
};

static constexpr DoorWadEntry kStageDoors[20] = {
    // Tribe 1: Sailor Village (L2..L6)
    {"T1_D1", "T1L2M001", "T1L4M001", 7210006, "Find the Key! Save the Gobbo!", "Sailor Village", 1},
    {"T1_D2", "T1L3M001", "T1L6M001", 7210018, "Find 5 Lost Treasure Chests", "Sailor Village", 2},
    {"T1_D3", "T1L4M001", "T1L5M001", 7210030, "Get the Gobbo's Sandwich!", "Sailor Village", 3},
    {"T1_D4", "T1L5M001", "T1L2M001", 7210042, "Croc Vs Dantini Boat Race", "Sailor Village", 4},
    {"T1_D5", "T1L6M001", "T1L3M001", 7210055, "Save the Bird from the Thief", "Sailor Village", 5},
    // Tribe 2: Cossack Village (L2, L3, L5, L6, L7)
    {"T2_D1", "T2L2M001", "T2L5M001", 7210077, "Save the Ice Trapped Gobbos!", "Cossack Village", 1},
    {"T2_D2", "T2L3M001", "T2L6M001", 7210089, "Chase the Choo Choo Train", "Cossack Village", 2},
    {"T2_D3", "T2L5M001", "T2L7M001", 7210102, "It's just Snowball Madness!", "Cossack Village", 3},
    {"T2_D4", "T2L6M001", "T2L2M001", 7210115, "Hang Glider Valley", "Cossack Village", 4},
    {"T2_D5", "T2L7M001", "T2L3M001", 7210128, "KaBooom! It's Roger Red Ant", "Cossack Village", 5},
    // Tribe 3: Caveman Village (L2, L3, L4, L5, L7)
    {"T3_D1", "T3L2M001", "T3L4M001", 7210149, "Find the Wheels in the Jungle!", "Caveman Village", 1},
    {"T3_D2", "T3L3M001", "T3L5M001", 7210161, "Find the Wheels in the Mine!", "Caveman Village", 2},
    {"T3_D3", "T3L4M001", "T3L7M001", 7210173, "Race Day at Goldrock", "Caveman Village", 3},
    {"T3_D4", "T3L5M001", "T3L2M001", 7210186, "Climb The Devil's Tower", "Caveman Village", 4},
    {"T3_D5", "T3L7M001", "T3L3M001", 7210199, "Save 50 Trapped Gobbos!", "Caveman Village", 5},
    // Tribe 4: Inca Village (L2, L3, L5, L6, T0L1M003)
    {"T4_D1", "T4L2M001", "T4L5M001", 7210221, "Save 30 Gobbo babies!", "Inca Village", 1},
    {"T4_D2", "T4L3M001", "T4L6M001", 7210233, "Up the Waterfall", "Inca Village", 2},
    {"T4_D3", "T4L5M001", "T4L2M001", 7210246, "Bride of the Dungeon of Defright", "Inca Village", 3},
    {"T4_D4", "T4L6M001", "T4L3M001", 7210258, "Goo Man Chu's Tower", "Inca Village", 4},
    {"T4_D5", "T0L1M003", "T4L5M001", 7210271, "Burning Down the Forest", "Inca Village", 5}
};

static int find_stage_index_by_wad(const std::string& wad) {
    if (wad.size() < 4) return -1;
    for (int i = 0; i < 20; ++i) {
        if (wad == kStageDoors[i].vanilla_wad) return i;
    }
    // Also match submaps of the same stage (e.g., T1L4M002 -> T1L4M001)
    std::string prefix = wad.substr(0, 4);
    for (int i = 0; i < 20; ++i) {
        if (std::strncmp(kStageDoors[i].vanilla_wad, prefix.c_str(), 4) == 0) {
            return i;
        }
    }
    return -1;
}

static std::string coords_to_wad(const MapCoords& m) {
    char type_ch = 'L';
    if (m.map_type == 1) type_ch = 'B';
    else if (m.map_type == 2) type_ch = 'S';
    else if (m.map_type == 3) type_ch = 'I';
    char buf[32] = {0};
    std::snprintf(buf, sizeof(buf), "T%1d%c%1dM%03d", m.tribe, type_ch, m.level, m.submap);
    return std::string(buf);
}

static bool wad_to_coords(const std::string& wad, MapCoords& out) {
    if (wad.size() < 8 || wad[0] != 'T' || wad[4] != 'M') return false;
    out.tribe = wad[1] - '0';
    char tc = wad[2];
    if (tc == 'L') out.map_type = 0;
    else if (tc == 'B') out.map_type = 1;
    else if (tc == 'S') out.map_type = 2;
    else if (tc == 'I') out.map_type = 3;
    else return false;
    out.level = wad[3] - '0';
    out.submap = std::atoi(wad.c_str() + 5);
    return (out.tribe >= 0 && out.tribe <= 5 && out.level >= 0 && out.level <= 9 && out.submap >= 0);
}

static void register_door_override_locked(const std::string& door_or_wad, const std::string& target_wad) {
    if (door_or_wad.empty() || target_wad.empty()) return;
    g_state.door_overrides[door_or_wad] = target_wad;
    int target_idx = find_stage_index_by_wad(target_wad);
    for (int i = 0; i < 20; ++i) {
        const auto& entry = kStageDoors[i];
        if (door_or_wad == entry.door_id || door_or_wad == entry.vanilla_wad) {
            g_state.door_overrides[entry.door_id] = target_wad;
            g_state.door_overrides[entry.vanilla_wad] = target_wad;
            if (target_idx >= 0) {
                g_state.door_perm[i] = static_cast<uint8_t>(target_idx);
            }
            if (i == 0) {
                std::string target_name = (target_idx >= 0) ? kStageDoors[target_idx].level_name : target_wad;
                g_state.last_door_redirect_desc = std::string("Door 1 (") + entry.level_name + ") -> " + target_name + " (Seed #" + g_state.current_seed_hex + ")";
            }
            break;
        }
    }
}

static uint64_t generate_fresh_entropy_seed() {
    static std::atomic<uint64_t> s_counter{0x9E3779B97F4A7C15ULL};
    std::random_device rd;
    uint64_t r1 = (static_cast<uint64_t>(rd()) << 32) ^ static_cast<uint64_t>(rd());
    LARGE_INTEGER qpc{};
    QueryPerformanceCounter(&qpc);
    uint64_t t = static_cast<uint64_t>(GetTickCount64());
    uint64_t c = s_counter.fetch_add(0xBF58476D1CE4E5B9ULL);
    uint64_t z = r1 ^ static_cast<uint64_t>(qpc.QuadPart) ^ (t << 17) ^ c;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z = z ^ (z >> 31);
    return (z == 0) ? 0xC20C20A912345678ULL : z;
}

static std::string format_seed_hex(uint64_t seed) {
    char buf[24] = {0};
    std::snprintf(buf, sizeof(buf), "%08X", static_cast<uint32_t>((seed ^ (seed >> 32)) & 0xFFFFFFFFu));
    return std::string(buf);
}

static uint8_t pack_abilities_mask(const PlayerAbilities& a) {
    uint8_t m = 0;
    if (a.tail_whack)         m |= (1 << 0);
    if (a.butt_stomp)         m |= (1 << 1);
    if (a.climbing_claws)     m |= (1 << 2);
    if (a.power_gloves)       m |= (1 << 3);
    if (a.lever_wrench)       m |= (1 << 4);
    if (a.vehicle_license)    m |= (1 << 5);
    if (a.airplane_ammo_feed) m |= (1 << 6);
    return m;
}

static void unpack_abilities_mask(uint8_t m, PlayerAbilities& a) {
    a.tail_whack         = (m & (1 << 0)) != 0;
    a.butt_stomp         = (m & (1 << 1)) != 0;
    a.climbing_claws     = (m & (1 << 2)) != 0;
    a.power_gloves       = (m & (1 << 3)) != 0;
    a.lever_wrench       = (m & (1 << 4)) != 0;
    a.vehicle_license    = (m & (1 << 5)) != 0;
    a.airplane_ammo_feed = (m & (1 << 6)) != 0;
}

static void build_logical_item_pool_for_seed_locked(uint64_t seed) {
    std::mt19937_64 rng(seed ^ 0xA5A5F0F05A5A0F0FULL);

    // 1. Shuffle doors within each of the 4 Villages using a true random derangement (perm[i] != i)
    // so every new save randomizes Door 1 (and all other doors) to genuinely random levels!
    for (int v = 0; v < 4; ++v) {
        int base = v * 5;
        int idx[5] = {base + 0, base + 1, base + 2, base + 3, base + 4};
        for (int attempt = 0; attempt < 32; ++attempt) {
            std::shuffle(std::begin(idx), std::end(idx), rng);
            bool is_derangement = true;
            for (int k = 0; k < 5; ++k) {
                if (idx[k] == base + k) {
                    is_derangement = false;
                    break;
                }
            }
            if (is_derangement) break;
        }
        // Guarantee no fixed points even if random shuffle hit a rare streak
        for (int k = 0; k < 5; ++k) {
            if (idx[k] == base + k) {
                std::swap(idx[k], idx[(k + 1) % 5]);
            }
        }
        for (int k = 0; k < 5; ++k) {
            int door_i = base + k;
            g_state.door_perm[door_i] = static_cast<uint8_t>(idx[k]);
            g_state.door_overrides[kStageDoors[door_i].door_id] = kStageDoors[idx[k]].vanilla_wad;
            g_state.door_overrides[kStageDoors[door_i].vanilla_wad] = kStageDoors[idx[k]].vanilla_wad;
        }
    }

    int target_idx0 = g_state.door_perm[0] < 20 ? g_state.door_perm[0] : 0;
    g_state.last_door_redirect_desc =
        std::string("Door 1 (") + kStageDoors[0].level_name + ") -> " +
        kStageDoors[target_idx0].level_name + " (Seed #" + g_state.current_seed_hex + ")";

    // 2. Build a logically solvable 180-check item/move/trap pool (9 checks per stage across 20 stages):
    //    Sub-checks 0..2: 15, 30, 45 White Crystals collected in stage (always reachable without abilities!)
    //    Sub-checks 3..7: Colored Crystal slots 1..5 in stage
    //    Sub-check  8:    Stage Completion
    //
    // Sphere 1 (Early Logical Reach): White crystal checks (0..2) across the 5 Sailor Village stages (stages 0..4)
    // = 15 early checks. We place core progression abilities (Tail Whack, Butt Stomp, Climbing Claws, Power Gloves,
    // Lever Wrench) + early Heart Pots + Colored Crystals + a Fire Breakout Trap here so the player can NEVER get softlocked!
    std::vector<int> sphere1_slots;
    std::vector<int> remaining_slots;
    for (int s = 0; s < 20; ++s) {
        for (int c = 0; c < 9; ++c) {
            int check_id = s * 9 + c;
            if (s < 5 && c < 3) {
                sphere1_slots.push_back(check_id); // 15 guaranteed-reachable Sphere 1 checks in Sailor Village
            } else {
                remaining_slots.push_back(check_id); // 165 checks across the rest of the game
            }
        }
    }

    std::shuffle(sphere1_slots.begin(), sphere1_slots.end(), rng);
    std::shuffle(remaining_slots.begin(), remaining_slots.end(), rng);

    // Sphere 1 items (15 items):
    // 5 core abilities guaranteed in early checks:
    // Tail Whack (7200001), Butt Stomp (7200002), Climbing Claws (7200003),
    // Power Gloves (7200004), Lever Wrench (7200005)
    // Plus 4 Inventory Wheel items, 1 Heart Pot, 3 Colored Crystals, 2 Traps:
    const int sphere1_items[15] = {
        7200001, 7200002, 7200003, 7200004, 7200005,
        7200142, 7200143, 7200144, 7200145, 7200021,
        7200042, 7200043, 7200044,
        7200151, 7200155
    };
    for (size_t i = 0; i < 15 && i < sphere1_slots.size(); ++i) {
        g_state.solo_check_items[sphere1_slots[i]] = sphere1_items[i];
    }

    // Remaining 165 items:
    // - Vehicle License (7200006), Airplane Ammo Feed (7200007), Binoculars (7200146)
    // - 4 Village VIP Passes (7200008..7200011)
    // - 4 Jigsaw Pieces (7200012..7200015)
    // - 16 Trophies (7200017)
    // - 97 remaining Colored Crystals (7200045..7200141)
    // - 8 Heart Pots (7200021)
    // - 6 Silver Keys (7200019) + 4 Gold Keys (7200020)
    // - 4 Clockwork Gobbos (7200142), 4 Red Jellies (7200143), 4 Blue Jellies (7200144), 4 Green Jellies (7200145)
    // - Remaining Traps (Fire Breakout 7200151, Ice Physics 7200152, Reverse Controls 7200153, Dantini Ambush 7200154, Speed 7200155)
    std::vector<int> pool;
    pool.reserve(165);
    pool.push_back(7200006); // Vehicle License
    pool.push_back(7200007); // Airplane Ammo Feed
    pool.push_back(7200146); // Binoculars
    for (int id = 7200008; id <= 7200015; ++id) pool.push_back(id); // 4 VIP Passes + 4 Jigsaw Pieces
    for (int i = 0; i < 16; ++i) pool.push_back(7200017); // 16 Trophies
    for (int id = 7200045; id <= 7200141; ++id) pool.push_back(id); // 97 Colored Crystals
    for (int i = 0; i < 8; ++i) pool.push_back(7200021); // 8 Heart Pots
    for (int i = 0; i < 6; ++i) pool.push_back(7200019); // 6 Silver Keys
    for (int i = 0; i < 4; ++i) pool.push_back(7200020); // 4 Gold Keys
    for (int i = 0; i < 4; ++i) {
        pool.push_back(7200142); // Clockwork Gobbo
        pool.push_back(7200143); // Gummi Jump (Red Jelly)
        pool.push_back(7200144); // Jelly Jump (Blue Jelly)
        pool.push_back(7200145); // High Jelly Jump (Green Jelly)
    }
    const int trap_cycle[5] = {7200151, 7200152, 7200153, 7200154, 7200155};
    while (pool.size() < remaining_slots.size()) {
        pool.push_back(trap_cycle[pool.size() % 5]);
    }
    std::shuffle(pool.begin(), pool.end(), rng);
    for (size_t i = 0; i < remaining_slots.size(); ++i) {
        g_state.solo_check_items[remaining_slots[i]] = pool[i];
    }

    // 3. Build the 2,000-entry Individual White Crystal check pool (100 crystals per stage * 20 stages):
    //    Sphere 1 White Crystals (first 20 White Crystals in Sailor Village stages 0..4 = 100 early slots)
    //    include core abilities, inventory wheel items (Red/Blue/Green Jellies, Clockwork Gobbos, Binoculars,
    //    Keys, Jigsaw Pieces), colored crystals, heart pots, and traps so every individual White Crystal collected
    //    for the first time immediately populates the inventory wheel and moveset!
    std::vector<int> wc_sphere1_slots;
    std::vector<int> wc_remaining_slots;
    for (int s = 0; s < 20; ++s) {
        for (int c = 0; c < 100; ++c) {
            int wc_idx = s * 100 + c;
            if (s < 5 && c < 20) {
                wc_sphere1_slots.push_back(wc_idx);
            } else {
                wc_remaining_slots.push_back(wc_idx);
            }
        }
    }
    std::shuffle(wc_sphere1_slots.begin(), wc_sphere1_slots.end(), rng);
    std::shuffle(wc_remaining_slots.begin(), wc_remaining_slots.end(), rng);

    std::vector<int> wc_sphere1_items = {
        7200001, 7200002, 7200003, 7200004, 7200005, // Core abilities
        7200142, 7200142, 7200142,                   // 3 Clockwork Gobbos (Inventory Wheel)
        7200143, 7200143, 7200143,                   // 3 Gummi Jump Red Jellies (Inventory Wheel)
        7200144, 7200144, 7200144,                   // 3 Jelly Jump Blue Jellies (Inventory Wheel)
        7200145, 7200145, 7200145,                   // 3 High Jelly Jump Green Jellies (Inventory Wheel)
        7200146,                                     // Binoculars (Inventory Wheel)
        7200019, 7200019, 7200020,                   // 2 Silver Keys + 1 Gold Key (Inventory Wheel & HUD)
        7200012, 7200018,                            // 1 Jigsaw Piece + 1 Golden Gobbo (Inventory Wheel)
        7200021, 7200021,                            // 2 Heart Pots
        7200042, 7200043, 7200044, 7200045, 7200046, // Sailor Door 1 Colored Crystals
        7200151, 7200152, 7200155                    // Early traps
    };
    const int early_wheel_cycle[6] = {7200143, 7200144, 7200145, 7200142, 7200148, 7200149};
    while (wc_sphere1_items.size() < wc_sphere1_slots.size()) {
        wc_sphere1_items.push_back(early_wheel_cycle[wc_sphere1_items.size() % 6]);
    }
    for (size_t i = 0; i < wc_sphere1_slots.size(); ++i) {
        g_state.solo_white_crystal_items[wc_sphere1_slots[i]] = wc_sphere1_items[i];
    }

    std::vector<int> wc_pool;
    wc_pool.reserve(wc_remaining_slots.size());
    wc_pool.push_back(7200006); // Vehicle License
    wc_pool.push_back(7200007); // Airplane Ammo Feed
    for (int id = 7200008; id <= 7200015; ++id) wc_pool.push_back(id); // VIP Passes + Jigsaw Pieces
    for (int i = 0; i < 16; ++i) wc_pool.push_back(7200017); // 16 Trophies
    for (int id = 7200047; id <= 7200141; ++id) wc_pool.push_back(id); // 95 Colored Crystals
    for (int i = 0; i < 8; ++i) wc_pool.push_back(7200021); // 8 Heart Pots
    for (int i = 0; i < 6; ++i) wc_pool.push_back(7200019); // 6 Silver Keys
    for (int i = 0; i < 4; ++i) wc_pool.push_back(7200020); // 4 Gold Keys
    for (int i = 0; i < 10; ++i) {
        wc_pool.push_back(7200142); // Clockwork Gobbo
        wc_pool.push_back(7200143); // Red Jelly
        wc_pool.push_back(7200144); // Blue Jelly
        wc_pool.push_back(7200145); // Green Jelly
    }
    for (int i = 0; i < 45; ++i) wc_pool.push_back(trap_cycle[i % 5]); // 45 Traps
    const int filler_cycle[3] = {7200148, 7200149, 7200150};
    while (wc_pool.size() < wc_remaining_slots.size()) {
        wc_pool.push_back(filler_cycle[wc_pool.size() % 3]);
    }
    std::shuffle(wc_pool.begin(), wc_pool.end(), rng);
    for (size_t i = 0; i < wc_remaining_slots.size(); ++i) {
        g_state.solo_white_crystal_items[wc_remaining_slots[i]] = wc_pool[i];
    }
}

static void sync_state_to_save_footer_locked() {
    if (!g_exe_base) return;
    auto* footer = reinterpret_cast<Croc2RandoSaveFooter*>(
        g_exe_base + RVA_SAVE_DATA_TABLE + RVA_RANDO_SAVE_FOOTER_OFF
    );
    if (!g_state.in_active_save) return;

    if (g_state.save_locked_to_mod) {
        footer->magic = RANDO_SAVE_MAGIC;
        footer->version = 1;
        footer->seed = g_state.current_save_seed;
        footer->is_rando_save = 1;
        footer->abilities_mask = pack_abilities_mask(g_state.abilities);
        footer->max_heart_pots = static_cast<uint8_t>(g_state.max_heart_pots);
        footer->jigsaw_pieces = static_cast<uint8_t>(g_state.jigsaw_pieces);
        footer->golden_gobbos = static_cast<uint8_t>(g_state.golden_gobbos);
        footer->trophies = static_cast<uint8_t>(g_state.trophies);
        footer->silver_keys = static_cast<uint8_t>(g_state.silver_keys);
        footer->gold_keys = static_cast<uint8_t>(g_state.gold_keys);
        std::memcpy(footer->door_perm, g_state.door_perm, 20);
        for (int i = 0; i < 20; ++i) {
            footer->stage_crystals[i] = g_state.stage_colored_crystals[kStageDoors[i].vanilla_wad];
        }
        std::memcpy(footer->checks_claimed, g_state.checks_claimed, 24);
        std::memcpy(footer->white_crystals_claimed, g_state.white_crystals_claimed, sizeof(g_state.white_crystals_claimed));
        footer->red_jellies = static_cast<uint8_t>(g_state.red_jellies);
        footer->blue_jellies = static_cast<uint8_t>(g_state.blue_jellies);
        footer->green_jellies = static_cast<uint8_t>(g_state.green_jellies);
        footer->clockwork_gobbos = static_cast<uint8_t>(g_state.clockwork_gobbos);
        footer->binoculars = g_state.binoculars ? 1 : 0;
        footer->ap_items_applied = static_cast<uint32_t>(g_state.ap_items_applied);
    } else {
        footer->magic = VANILLA_SAVE_MAGIC;
        footer->version = 1;
        footer->is_rando_save = 0;
    }
}

static void restore_state_from_save_footer_locked(const Croc2RandoSaveFooter& footer) {
    g_state.current_save_seed = footer.seed;
    g_state.current_seed_hex = format_seed_hex(footer.seed);
    g_state.in_active_save = true;
    g_state.save_locked_to_mod = true;
    g_state.manual_override_disabled = false;

    // Rebuild the deterministic check pool for this save's seed first
    build_logical_item_pool_for_seed_locked(footer.seed);

    // Restore exact saved door permutation and unlocked progression
    std::memcpy(g_state.door_perm, footer.door_perm, 20);
    for (int i = 0; i < 20; ++i) {
        uint8_t dst_idx = footer.door_perm[i] < 20 ? footer.door_perm[i] : static_cast<uint8_t>(i);
        g_state.door_overrides[kStageDoors[i].door_id] = kStageDoors[dst_idx].vanilla_wad;
        g_state.door_overrides[kStageDoors[i].vanilla_wad] = kStageDoors[dst_idx].vanilla_wad;
    }
    int target_idx0 = g_state.door_perm[0] < 20 ? g_state.door_perm[0] : 0;
    g_state.last_door_redirect_desc =
        std::string("Door 1 (") + kStageDoors[0].level_name + ") -> " +
        kStageDoors[target_idx0].level_name + " (Seed #" + g_state.current_seed_hex + ")";

    unpack_abilities_mask(footer.abilities_mask, g_state.abilities);
    g_state.max_heart_pots = footer.max_heart_pots >= 5 ? footer.max_heart_pots : 5;
    g_state.jigsaw_pieces = footer.jigsaw_pieces;
    g_state.golden_gobbos = footer.golden_gobbos;
    g_state.trophies = footer.trophies;
    g_state.silver_keys = footer.silver_keys;
    g_state.gold_keys = footer.gold_keys;
    g_state.red_jellies = footer.red_jellies;
    g_state.blue_jellies = footer.blue_jellies;
    g_state.green_jellies = footer.green_jellies;
    g_state.clockwork_gobbos = footer.clockwork_gobbos;
    g_state.binoculars = (footer.binoculars != 0);

    g_state.stage_colored_crystals.clear();
    g_state.activated_golden_gobbos.clear();
    for (int i = 0; i < 20; ++i) {
        std::string wad = kStageDoors[i].vanilla_wad;
        g_state.stage_colored_crystals[wad] = footer.stage_crystals[i];
        if (footer.stage_crystals[i] == CRYSTAL_ALL_5) {
            g_state.activated_golden_gobbos.insert(wad);
        }
    }
    std::memcpy(g_state.checks_claimed, footer.checks_claimed, 24);
    std::memcpy(g_state.white_crystals_claimed, footer.white_crystals_claimed, sizeof(g_state.white_crystals_claimed));

    // Apply any Archipelago items received this session that this save hasn't seen yet
    // (e.g. delivered on the Title Screen before the save was loaded).
    reset_pending_native_grants();
    g_state.ap_items_applied = static_cast<int>(footer.ap_items_applied);
    const int session_count = static_cast<int>(g_state.ap_received_item_ids.size());
    if (g_state.ap_items_applied < session_count) {
        std::vector<int> session_items = g_state.ap_received_item_ids;
        for (int i = g_state.ap_items_applied; i < session_count; ++i) {
            apply_ap_item_by_id_locked(session_items[i], "Archipelago", "Sync", false);
        }
        g_state.ap_items_applied = session_count;
    }

    g_need_full_inventory_sync.store(true);
    g_state.add_log("Loaded Randomizer Save (Seed #" + g_state.current_seed_hex + ") - Mod Locked ON!");
}

static void generate_new_save_seed_locked(uint64_t seed) {
    if (seed == 0) {
        seed = generate_fresh_entropy_seed();
    }
    g_state.current_save_seed = seed;
    g_state.current_seed_hex = format_seed_hex(seed);

    // Reset progression for a brand-new Randomizer Save:
    // Start with all randomized moves LOCKED so the player unlocks them across the seed!
    g_state.abilities = PlayerAbilities{};
    if (!g_state.shuffle_abilities) {
        g_state.abilities.tail_whack = true;
        g_state.abilities.butt_stomp = true;
        g_state.abilities.climbing_claws = true;
        g_state.abilities.power_gloves = true;
    }
    if (!g_state.shuffle_levers) g_state.abilities.lever_wrench = true;
    if (!g_state.shuffle_vehicles) g_state.abilities.vehicle_license = true;
    if (!g_state.shuffle_plane_guns) g_state.abilities.airplane_ammo_feed = true;

    g_state.max_heart_pots = 5;
    g_state.current_hearts = 5;
    g_state.jigsaw_pieces = 0;
    g_state.golden_gobbos = 0;
    g_state.trophies = 0;
    g_state.silver_keys = 0;
    g_state.gold_keys = 0;
    g_state.clockwork_gobbos = 0;
    g_state.red_jellies = 0;
    g_state.blue_jellies = 0;
    g_state.green_jellies = 0;
    g_state.binoculars = false;
    g_state.stage_colored_crystals.clear();
    g_state.activated_golden_gobbos.clear();
    std::memset(g_state.checks_claimed, 0, sizeof(g_state.checks_claimed));
    std::memset(g_state.white_crystals_claimed, 0, sizeof(g_state.white_crystals_claimed));

    build_logical_item_pool_for_seed_locked(seed);

    // Re-apply any Archipelago items that were already delivered while on the Title Screen!
    if (!g_state.ap_received_item_ids.empty()) {
        reset_pending_native_grants(); // Avoid double-granting items queued before the save existed
        std::vector<int> saved_ap_items = g_state.ap_received_item_ids;
        for (int ap_id : saved_ap_items) {
            apply_ap_item_by_id_locked(ap_id, "Archipelago", "Start Inventory / Sync", false);
        }
    }
    g_state.ap_items_applied = static_cast<int>(g_state.ap_received_item_ids.size());

    g_need_full_inventory_sync.store(true);
    sync_state_to_save_footer_locked();
}

static void trigger_bottom_toast_locked(bool is_outgoing,
                                       const std::string& item_name,
                                       ItemImportance imp,
                                       const std::string& sender,
                                       const std::string& receiver,
                                       const std::string& location,
                                       uint32_t duration_ms) {
    BottomToastNotification toast;
    toast.is_outgoing = is_outgoing;
    toast.item_name = item_name;
    toast.importance = imp;

    std::string s = sender;
    std::string r = receiver;

    bool is_self = (s == "Croc" && r == "Croc") || (s.empty() && r == "Croc") || (s == "Croc" && r.empty());
    if (is_self) {
        s = "Croc";
        r = "Croc";
    } else {
        if (s.empty() || s == "Player1" || s == "Someone Else") {
            s = is_outgoing ? "Croc" : g_state.get_default_yaml_receiver();
        }
        if (r.empty() || r == "Player1" || r == "Someone Else") {
            r = is_outgoing ? g_state.get_default_yaml_receiver() : "Croc";
        }
        if (s == r) {
            is_self = true;
        }
    }

    toast.is_self = is_self;
    toast.sender = s;
    toast.receiver = r;
    toast.location = location;
    toast.expire_tick = static_cast<uint64_t>(GetTickCount()) + duration_ms;

    g_state.active_toast = toast;
    g_state.toast_active = true;

    if (g_toast_hwnd) {
        ShowWindow(g_toast_hwnd, SW_SHOWNOACTIVATE);
        InvalidateRect(g_toast_hwnd, nullptr, FALSE);
    }
}

static void claim_solo_check_locked(int stage_idx, int sub_check_idx, const std::string& check_desc) {
    if (stage_idx < 0 || stage_idx >= 20 || sub_check_idx < 0 || sub_check_idx >= 9) return;
    int check_id = stage_idx * 9 + sub_check_idx;
    int byte_idx = check_id / 8;
    uint8_t bit_mask = static_cast<uint8_t>(1u << (check_id % 8));

    if ((g_state.checks_claimed[byte_idx] & bit_mask) != 0) {
        return; // Already claimed in this save!
    }
    g_state.checks_claimed[byte_idx] |= bit_mask;

    // Map Stage Clear (sub_check_idx == 8) and Colored Crystals 1..5 (sub_check_idx 3..7)
    // to their exact Archipelago location IDs from worlds/croc2/data/locations.json!
    int location_id = 7300001 + check_id;
    if (sub_check_idx == 8) {
        location_id = kStageDoors[stage_idx].stage_clear_loc_id;
    } else if (sub_check_idx >= 3 && sub_check_idx <= 7) {
        location_id = kStageDoors[stage_idx].stage_clear_loc_id + (sub_check_idx - 2);
    }

    g_state.pending_events_json.push_back(
        "{\"event\":\"location_checked\",\"location_id\":" + std::to_string(location_id) +
        ",\"check_id\":" + std::to_string(check_id) +
        ",\"description\":\"" + json_escape(check_desc) + "\"}"
    );

    const bool is_multiworld = g_state.conn.ap_server_connected;
    if (g_state.conn.ap_server_connected) {
        send_ws_location_check_locked(location_id);
        g_state.add_log("Sent Check: " + check_desc + " (#" + std::to_string(location_id) + ")");
    } else if (!is_multiworld) {
        // Solo / Standalone Randomizer mode: immediately grant the seed's randomized item/move/trap placed at this check!
        int item_id = g_state.solo_check_items[check_id];
        if (item_id > 0) {
            std::string item_name;
            ItemImportance imp = ItemImportance::PROGRESSION;
            get_ap_item_info(item_id, item_name, imp);

            // Toast in solo play: Croc got his own [Item]!
            trigger_bottom_toast_locked(false, item_name, imp, "Croc", "Croc", check_desc);

            g_state.add_log("Check [" + check_desc + "] -> " + item_name);
            apply_ap_item_by_id_locked(item_id, "Croc", check_desc, false);
        }
    }

    sync_state_to_save_footer_locked();
    if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
    if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
}

static bool claim_white_crystal_check_locked(int stage_idx, int crystal_num_1_to_100) {
    if (stage_idx < 0 || stage_idx >= 20 || crystal_num_1_to_100 < 1 || crystal_num_1_to_100 > 100) {
        return false;
    }
    int c_zero = crystal_num_1_to_100 - 1;
    int byte_idx = c_zero / 8;
    uint8_t bit_mask = static_cast<uint8_t>(1u << (c_zero % 8));

    if (g_state.shuffle_white_crystals == 0) {
        // Vanilla mode: each crystal collected directly increments crystal currency
        g_state.current_crystals += 1;
        return false;
    }
    if (g_state.shuffle_white_crystals == 1) { // milestones_50
        if (crystal_num_1_to_100 != 50 && crystal_num_1_to_100 != 100) {
            g_state.current_crystals += 1;
            return false;
        }
    } else if (g_state.shuffle_white_crystals == 2) { // milestones_10
        if ((crystal_num_1_to_100 % 10) != 0) {
            g_state.current_crystals += 1;
            return false;
        }
    }

    // Check if this specific White Crystal (1..100) in this stage has ALREADY been used as a check!
    if ((g_state.white_crystals_claimed[stage_idx][byte_idx] & bit_mask) != 0) {
        // Already collected once in this save: cannot be re-acquired for currency and does NOT send out more checks!
        return false;
    }

    // First-time White Crystal pickup: adds to currency and marks check as claimed
    g_state.current_crystals += 1;
    g_state.white_crystals_claimed[stage_idx][byte_idx] |= bit_mask;

    int location_id = 7211000 + stage_idx * 100 + crystal_num_1_to_100;
    std::string desc = std::string(kStageDoors[stage_idx].level_name) +
                       " - White Crystal #" + std::to_string(crystal_num_1_to_100);
    g_state.pending_events_json.push_back(
        "{\"event\":\"location_checked\",\"location_id\":" + std::to_string(location_id) +
        ",\"stage_idx\":" + std::to_string(stage_idx) +
        ",\"crystal_index\":" + std::to_string(crystal_num_1_to_100) +
        ",\"description\":\"" + json_escape(desc) + "\"}"
    );

    const bool is_multiworld = g_state.conn.ap_server_connected;
    if (g_state.conn.ap_server_connected) {
        send_ws_location_check_locked(location_id);
        g_state.add_log("Sent Check: " + desc + " (#" + std::to_string(location_id) + ")");
    } else if (!is_multiworld) {
        // Solo / Standalone Randomizer mode: grant the seed's randomized item for this White Crystal check!
        int pool_idx = stage_idx * 100 + c_zero;
        int item_id = g_state.solo_white_crystal_items[pool_idx];
        if (item_id > 0) {
            std::string item_name;
            ItemImportance imp = ItemImportance::PROGRESSION;
            get_ap_item_info(item_id, item_name, imp);

            // Toast in solo play: Croc got his own [Item]!
            trigger_bottom_toast_locked(false, item_name, imp, "Croc", "Croc", desc);

            g_state.add_log("Check [" + desc + "] -> " + item_name);
            apply_ap_item_by_id_locked(item_id, "Croc", desc, false);
        }
    }

    sync_state_to_save_footer_locked();
    if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
    if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
    return true;
}

static void init_default_door_shuffle_locked() {
    generate_new_save_seed_locked(generate_fresh_entropy_seed());
}

// Minimal JSON string/int helper for lightweight dependency-free DLL parsing
static std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos) return "";
    size_t first_quote = json.find('"', colon + 1);
    if (first_quote == std::string::npos) return "";
    size_t second_quote = first_quote + 1;
    while (second_quote < json.size()) {
        if (json[second_quote] == '"' && json[second_quote - 1] != '\\') break;
        second_quote++;
    }
    if (second_quote >= json.size()) return "";
    return json.substr(first_quote + 1, second_quote - first_quote - 1);
}

static int extract_json_int(const std::string& json, const std::string& key, int default_val = 0) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return default_val;
    size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos) return default_val;
    size_t start = colon + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;
    if (start >= json.size()) return default_val;
    return std::atoi(json.c_str() + start);
}

static float extract_json_float(const std::string& json, const std::string& key, float default_val = 0.0f) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return default_val;
    size_t colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos) return default_val;
    size_t start = colon + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t')) start++;
    if (start >= json.size()) return default_val;
    return static_cast<float>(std::atof(json.c_str() + start));
}

static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

static uint8_t color_to_bit(const std::string& color) {
    if (color == "RED") return CRYSTAL_RED;
    if (color == "BLUE") return CRYSTAL_BLUE;
    if (color == "GREEN") return CRYSTAL_GREEN;
    if (color == "YELLOW") return CRYSTAL_YELLOW;
    if (color == "PURPLE") return CRYSTAL_PURPLE;
    return 0;
}

static std::string resolve_default_ini_path() {
    char exe_path[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    std::string p(exe_path);
    size_t slash = p.find_last_of("\\/");
    if (slash != std::string::npos) {
        return p.substr(0, slash + 1) + "croc2_mod.ini";
    }
    return "croc2_mod.ini";
}

static void load_config_ini(const char* ini_path) {
    if (ini_path && ini_path[0] != '\0') {
        g_ini_file_path = ini_path;
    } else {
        g_ini_file_path = resolve_default_ini_path();
    }

    std::ifstream file(g_ini_file_path);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        auto trim = [](std::string& s) {
            while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n' || s.back() == '\t')) s.pop_back();
            size_t i = 0;
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
            s = s.substr(i);
        };
        trim(key);
        trim(val);
        if (key == "enabled") {
            g_state.config_enabled = (val == "true" || val == "1");
        } else if (key == "port") {
            g_state.tcp_port = static_cast<uint16_t>(std::atoi(val.c_str()));
        } else if (key == "toggle_hotkey_vk") {
            g_state.toggle_hotkey_vk = static_cast<int>(std::strtol(val.c_str(), nullptr, 0));
        } else if (key == "menu_hotkey_vk") {
            g_state.menu_hotkey_vk = static_cast<int>(std::strtol(val.c_str(), nullptr, 0));
        } else if (key == "fire_trap_knockback_ms" || key == "fire_trap_tick_ms") {
            int ms = std::atoi(val.c_str());
            if (ms > 0) g_state.fire_trap.timer = static_cast<float>(ms) / 1000.0f;
        } else if (key == "server_host" && !val.empty()) {
            g_state.conn.server_host = val;
        } else if (key == "server_port" && !val.empty()) {
            int p = std::atoi(val.c_str());
            if (p > 0) g_state.conn.server_port = p;
        } else if (key == "slot_name" && !val.empty()) {
            g_state.conn.slot_name = val; // Preserves exact case!
        } else if (key == "password") {
            g_state.conn.password = val;
        } else if (key == "lock_level_access") {
            g_state.lock_level_access = (val == "true" || val == "1");
        } else if (key == "shuffle_white_crystals") {
            g_state.shuffle_white_crystals = std::atoi(val.c_str());
        } else if (key == "shuffle_pete_store") {
            g_state.shuffle_pete_store = std::atoi(val.c_str());
        }
    }
}

static void scan_yaml_directory(const std::string& dir_path, std::vector<std::string>& out_names) {
    if (dir_path.empty()) return;
    std::string search_pattern = dir_path + "\\*.yaml";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string file_path = dir_path + "\\" + fd.cFileName;
            std::ifstream file(file_path);
            if (file.is_open()) {
                std::string line;
                int lines_checked = 0;
                while (std::getline(file, line) && lines_checked < 60) {
                    lines_checked++;
                    size_t pos = line.find("name:");
                    if (pos != std::string::npos) {
                        bool only_spaces = true;
                        for (size_t i = 0; i < pos; ++i) {
                            if (line[i] != ' ' && line[i] != '\t') { only_spaces = false; break; }
                        }
                        if (only_spaces) {
                            std::string val = line.substr(pos + 5);
                            size_t first = val.find_first_not_of(" \t\"'");
                            if (first != std::string::npos) {
                                size_t last = val.find_last_not_of(" \t\r\n\"'");
                                std::string player = val.substr(first, (last - first + 1));
                                size_t comment_pos = player.find('#');
                                if (comment_pos != std::string::npos) {
                                    player = player.substr(0, comment_pos);
                                    size_t l2 = player.find_last_not_of(" \t\r\n\"'");
                                    if (l2 != std::string::npos) player = player.substr(0, l2 + 1);
                                    else player.clear();
                                }
                                if (!player.empty() &&
                                    player.find('{') == std::string::npos &&
                                    player != "Player" &&
                                    player != "Player1" &&
                                    player.find("Template") == std::string::npos) {
                                    if (std::find(out_names.begin(), out_names.end(), player) == out_names.end()) {
                                        out_names.push_back(player);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
}

static void load_yaml_player_names() {
    std::vector<std::string> names;
    scan_yaml_directory(g_state.yaml_players_dir, names);
    scan_yaml_directory("C:\\ProgramData\\Archipelago\\edited yamls", names);

    // If WillOfTheVinny was found, place it at index 0 as the primary default receiver
    auto it = std::find(names.begin(), names.end(), "WillOfTheVinny");
    if (it != names.end()) {
        std::rotate(names.begin(), it, it + 1);
    } else {
        names.insert(names.begin(), "WillOfTheVinny");
    }

    g_state.custom_yaml_players = names;
}

static void save_config_ini(const char* ini_path) {
    std::string target = (ini_path && ini_path[0] != '\0') ? ini_path : g_ini_file_path;
    std::ofstream out(target, std::ios::trunc);
    if (!out.is_open()) return;
    out << "[ArchipelagoMod]\n"
        << "; Master enable switch. Set to false to start in Vanilla Mode (can still be toggled anytime in-game with F10).\n"
        << "enabled = " << (g_state.config_enabled ? "true" : "false") << "\n\n"
        << "; Localhost TCP port used by Croc2Client.py to communicate with croc2_ap_hook.dll\n"
        << "port = " << g_state.tcp_port << "\n\n"
        << "; Virtual-Key code for the in-game Vanilla/Randomizer toggle hotkey (0x79 = F10)\n"
        << "toggle_hotkey_vk = 0x" << std::hex << g_state.toggle_hotkey_vk << std::dec << "\n\n"
        << "; Virtual-Key code to open/close the in-game Croc-themed Archipelago Connection Menu (0x78 = F9)\n"
        << "menu_hotkey_vk = 0x" << std::hex << g_state.menu_hotkey_vk << std::dec << "\n\n"
        << "; Fire Breakout Trap visual flame effect and knockback recoil in milliseconds (instant -1 heart damage)\n"
        << "fire_trap_knockback_ms = 2000\n\n"
        << "[ServerConnection]\n"
        << "; Default Archipelago Server connection details (editable in-game by pressing F9)\n"
        << "server_host = " << g_state.conn.server_host << "\n"
        << "server_port = " << g_state.conn.server_port << "\n"
        << "; Case-sensitive player slot name\n"
        << "slot_name = " << g_state.conn.slot_name << "\n"
        << "; Optional room password\n"
        << "password = " << g_state.conn.password << "\n";
}

static void check_cmdline_flags() {
    const char* cmdline = GetCommandLineA();
    if (!cmdline) return;
    std::string s(cmdline);
    if (s.find("-vanilla") != std::string::npos || s.find("--no-mods") != std::string::npos) {
        g_state.forced_vanilla_cmdline = true;
    }
}

static std::string importance_to_string(ItemImportance imp) {
    switch (imp) {
        case ItemImportance::PROGRESSION: return "progression";
        case ItemImportance::USEFUL:      return "useful";
        case ItemImportance::TRAP:        return "trap";
        default:                          return "filler";
    }
}

static ItemImportance parse_importance_string(const std::string& cls, const std::string& item_name) {
    if (cls == "progression") return ItemImportance::PROGRESSION;
    if (cls == "useful")      return ItemImportance::USEFUL;
    if (cls == "trap")        return ItemImportance::TRAP;
    if (cls == "filler")      return ItemImportance::FILLER;

    if (item_name.find("Trap") != std::string::npos ||
        item_name == "fire_breakout" || item_name == "ice_physics" ||
        item_name == "reverse_controls" || item_name == "dantini_ambush" ||
        item_name == "speed_distortion" || item_name == "health_drain") {
        return ItemImportance::TRAP;
    }
    if (item_name == "Heart Pot" || item_name == "Binoculars") {
        return ItemImportance::USEFUL;
    }
    if (item_name == "Croc Crystals (1)" || item_name == "Croc Crystals (5)" ||
        item_name == "Croc Crystals (10)" || item_name == "Extra Life") {
        return ItemImportance::FILLER;
    }
    return ItemImportance::PROGRESSION;
}

static void get_ap_item_info(int item_id, std::string& out_name, ItemImportance& out_imp) {
    out_imp = ItemImportance::PROGRESSION;
    switch (item_id) {
        case 7200001: out_name = "Tail Whack"; return;
        case 7200002: out_name = "Butt Stomp"; return;
        case 7200003: out_name = "Climbing Claws"; return;
        case 7200004: out_name = "Power Gloves"; return;
        case 7200005: out_name = "Lever Wrench"; return;
        case 7200006: out_name = "Vehicle License"; return;
        case 7200007: out_name = "Airplane Ammo Feed"; return;
        case 7200008: out_name = "Sailor Village VIP Pass"; return;
        case 7200009: out_name = "Cossack Village VIP Pass"; return;
        case 7200010: out_name = "Caveman Village VIP Pass"; return;
        case 7200011: out_name = "Inca Village VIP Pass"; return;
        case 7200012:
        case 7200013:
        case 7200014:
        case 7200015:
        case 7200016: out_name = "Jigsaw Piece"; return;
        case 7200017: out_name = "Trophy"; return;
        case 7200018: out_name = "Golden Gobbo"; return;
        case 7200019: out_name = "Silver Key"; return;
        case 7200020: out_name = "Gold Key"; return;
        case 7200021: out_name = "Heart Pot"; out_imp = ItemImportance::USEFUL; return;
        case 7200142: out_name = "Clockwork Gobbo"; out_imp = ItemImportance::USEFUL; return;
        case 7200143: out_name = "Gummi Jump (Red Jelly)"; out_imp = ItemImportance::USEFUL; return;
        case 7200144: out_name = "Jelly Jump (Blue Jelly)"; out_imp = ItemImportance::USEFUL; return;
        case 7200145: out_name = "High Jelly Jump (Green Jelly)"; out_imp = ItemImportance::USEFUL; return;
        case 7200146: out_name = "Binoculars"; out_imp = ItemImportance::USEFUL; return;
        case 7200147: out_name = "Extra Life"; out_imp = ItemImportance::USEFUL; return;
        case 7200148: out_name = "Croc Crystals (1)"; out_imp = ItemImportance::FILLER; return;
        case 7200149: out_name = "Croc Crystals (5)"; out_imp = ItemImportance::FILLER; return;
        case 7200150: out_name = "Croc Crystals (10)"; out_imp = ItemImportance::FILLER; return;
        case 7200151: out_name = "Fire Breakout Trap"; out_imp = ItemImportance::TRAP; return;
        case 7200152: out_name = "Ice Physics Trap"; out_imp = ItemImportance::TRAP; return;
        case 7200153: out_name = "Reverse Controls Trap"; out_imp = ItemImportance::TRAP; return;
        case 7200154: out_name = "Dantini Ambush Trap"; out_imp = ItemImportance::TRAP; return;
        case 7200155: out_name = "Speed Distortion Trap"; out_imp = ItemImportance::TRAP; return;
        case 7200156: out_name = "Health Drain Trap"; out_imp = ItemImportance::TRAP; return;
        case 7200157: out_name = "Tail-Knot Trap"; out_imp = ItemImportance::TRAP; return;
        case 7200158: out_name = "Lead Boots Trap"; out_imp = ItemImportance::TRAP; return;
        case 7200159: out_name = "Darkness Trap"; out_imp = ItemImportance::TRAP; return;
        case 7200160: out_name = "Pete's Crystal Tax Trap"; out_imp = ItemImportance::TRAP; return;
        default:
            if (item_id >= 7200022 && item_id <= 7200041) {
                int door_idx = item_id - 7200022;
                out_name = std::string(kStageDoors[door_idx].level_name) + " Key";
                return;
            }
            if (item_id >= 7200042 && item_id <= 7200141) {
                int gem_offset = item_id - 7200042;
                int stage_idx = gem_offset / 5;
                int color_idx = gem_offset % 5;
                const char* colors[5] = {"Red", "Blue", "Green", "Yellow", "Purple"};
                std::string stage_name = (stage_idx >= 0 && stage_idx < 20)
                    ? std::string(kStageDoors[stage_idx].level_name)
                    : ("Stage " + std::to_string(stage_idx + 1));
                out_name = stage_name + " - " + colors[color_idx] + " Crystal";
                return;
            }
            out_name = "Archipelago Item #" + std::to_string(item_id);
            out_imp = ItemImportance::FILLER;
            return;
    }
}

static void apply_ap_item_by_id_locked(int item_id,
                                       const std::string& sender_name,
                                       const std::string& location_name,
                                       bool record_persistent) {
    if (record_persistent && item_id > 0) {
        g_state.ap_received_item_ids.push_back(item_id);
    }

    std::string item_name;
    ItemImportance imp = ItemImportance::PROGRESSION;
    get_ap_item_info(item_id, item_name, imp);

    // ID range mapped from worlds/croc2/data/items.json (7200001..7200156)
    switch (item_id) {
        case 7200001: g_state.abilities.tail_whack = true; g_state.add_log("Unlocked Ability: Tail Whack!"); break;
        case 7200002: g_state.abilities.butt_stomp = true; g_state.add_log("Unlocked Ability: Butt Stomp!"); break;
        case 7200003: g_state.abilities.climbing_claws = true; g_state.add_log("Unlocked Ability: Climbing Claws!"); break;
        case 7200004: g_state.abilities.power_gloves = true; g_state.add_log("Unlocked Ability: Power Gloves (Carry)!"); break;
        case 7200005: g_state.abilities.lever_wrench = true; g_state.add_log("Unlocked Ability: Lever Wrench!"); break;
        case 7200006: g_state.abilities.vehicle_license = true; g_state.add_log("Unlocked Ability: Vehicle License!"); break;
        case 7200007: g_state.abilities.airplane_ammo_feed = true; g_state.add_log("Unlocked Ability: Airplane Ammo Feed!"); break;
        case 7200008: g_state.vip_passes.insert("Sailor Village"); g_state.add_log("Received Pete's VIP Pass (Sailor Village)!"); break;
        case 7200009: g_state.vip_passes.insert("Cossack Village"); g_state.add_log("Received Pete's VIP Pass (Cossack Village)!"); break;
        case 7200010: g_state.vip_passes.insert("Caveman Village"); g_state.add_log("Received Pete's VIP Pass (Caveman Village)!"); break;
        case 7200011: g_state.vip_passes.insert("Inca Village"); g_state.add_log("Received Pete's VIP Pass (Inca Village)!"); break;
        case 7200012:
        case 7200013:
        case 7200014:
        case 7200015:
        case 7200016:
            g_state.jigsaw_pieces += 1;
            g_pending_jigsaws.fetch_add(1);
            g_state.add_log("Received Jigsaw Piece (" + std::to_string(g_state.jigsaw_pieces) + "/4)!");
            break;
        case 7200017:
            g_state.trophies += 1;
            g_pending_golden_gobbos.fetch_add(1);
            g_state.add_log("Received Trophy (" + std::to_string(g_state.trophies) + "/16)!");
            break;
        case 7200018:
            g_state.golden_gobbos += 1;
            g_pending_golden_gobbos.fetch_add(1);
            g_state.add_log("Received Golden Gobbo (" + std::to_string(g_state.golden_gobbos) + "/20)!");
            break;
        case 7200019:
            g_state.silver_keys += 1;
            g_pending_keys.fetch_add(1);
            g_state.add_log("Received Silver Key!");
            break;
        case 7200020:
            g_state.gold_keys += 1;
            g_pending_keys.fetch_add(1);
            g_state.add_log("Received Gold Key!");
            break;
        case 7200021:
            g_state.max_heart_pots += 1;
            g_state.current_hearts += 1;
            g_pending_heart_pots.fetch_add(1);
            g_state.add_log("Received Heart Pot (+1 Max Heart)!");
            break;
        case 7200142:
            g_state.clockwork_gobbos += 1;
            g_pending_clockwork.fetch_add(1);
            g_state.add_log("Received Clockwork Gobbo (" + std::to_string(g_state.clockwork_gobbos) + ")!");
            break;
        case 7200143:
            g_state.red_jellies += 1;
            g_pending_red_jellies.fetch_add(1);
            g_state.add_log("Received Gummi Jump (Red Jelly)!");
            break;
        case 7200144:
            g_state.blue_jellies += 1;
            g_pending_blue_jellies.fetch_add(1);
            g_state.add_log("Received Jelly Jump (Blue Jelly)!");
            break;
        case 7200145:
            g_state.green_jellies += 1;
            g_pending_green_jellies.fetch_add(1);
            g_state.add_log("Received High Jelly Jump (Green Jelly)!");
            break;
        case 7200146:
            g_state.binoculars = true;
            g_pending_binoculars.fetch_add(1);
            g_state.add_log("Received Binoculars!");
            break;
        case 7200147:
            g_state.current_hearts = g_state.max_heart_pots;
            g_pending_heart_pots.fetch_add(1);
            g_state.add_log("Received Extra Life (Full Hearts Restored)!");
            break;
        case 7200148:
            g_state.current_crystals += 1;
            g_pending_wallet_crystals.fetch_add(1);
            g_state.add_log("Received Croc Crystals (+1)!");
            break;
        case 7200149:
            g_state.current_crystals += 5;
            g_pending_wallet_crystals.fetch_add(5);
            g_state.add_log("Received Croc Crystals (+5)!");
            break;
        case 7200150:
            g_state.current_crystals += 10;
            g_pending_wallet_crystals.fetch_add(10);
            g_state.add_log("Received Croc Crystals (+10)!");
            break;
        case 7200151: // Fire Breakout Trap: INSTANT heart down + flame animation & knockback
            if (g_state.current_hearts > 0) g_state.current_hearts -= 1;
            g_pending_heart_damage.fetch_add(1);
            g_state.fire_trap.active = true;
            g_state.fire_trap.timer = 2.0f;
            g_state.add_log("🔥 TRAP! Fire Breakout: Instant Heart Lost with Fire Knockback!");
            break;
        case 7200152: // Ice Physics
            g_state.timed_traps.push_back({"ice_physics", 10.0f});
            g_state.add_log("TRAP! Ice Physics active for 10s!");
            break;
        case 7200153: // Reverse Controls
            g_state.timed_traps.push_back({"reverse_controls", 10.0f});
            g_state.add_log("TRAP! Reverse Controls active for 10s!");
            break;
        case 7200154: // Dantini Ambush
            g_state.timed_traps.push_back({"dantini_ambush", 10.0f});
            g_state.add_log("TRAP! Dantini Ambush active for 10s!");
            break;
        case 7200155: // Speed Distortion
            g_state.timed_traps.push_back({"speed_distortion", 10.0f});
            g_state.add_log("TRAP! Speed Distortion active for 10s!");
            break;
        case 7200156: // Health Drain
            if (g_state.current_hearts > 0) g_state.current_hearts -= 1;
            g_pending_heart_damage.fetch_add(1);
            g_state.add_log("TRAP! Health Drain (-1 Heart)!");
            break;
        case 7200157: // Tail-Knot (no untie, flat 10s)
            g_state.timed_traps.push_back({"tail_knot", 10.0f});
            g_state.add_log("TRAP! Tail-Knot: Tail is tied for 10s! Cannot be untied early!");
            break;
        case 7200158: // Lead Boots (flat 10s)
            g_state.timed_traps.push_back({"lead_boots", 10.0f});
            g_state.add_log("TRAP! Lead Boots: Heavy boots disable jumping for 10s!");
            break;
        case 7200159: // Darkness (flat 10s)
            g_state.timed_traps.push_back({"darkness", 10.0f});
            g_state.add_log("TRAP! Darkness: Shadows obscure vision for 10s!");
            break;
        case 7200160: {
            int tax = 20;
            if (g_state.current_crystals < tax) tax = g_state.current_crystals;
            g_state.current_crystals -= tax;
            g_pending_wallet_crystals.fetch_sub(tax);
            g_state.add_log("TRAP! Pete's Crystal Tax: Pete took " + std::to_string(tax) + " crystals!");
            break;
        }
        default:
            if (item_id >= 7200022 && item_id <= 7200041) {
                int door_idx = item_id - 7200022;
                if (door_idx >= 0 && door_idx < 20) {
                    g_state.unlocked_doors.insert(kStageDoors[door_idx].vanilla_wad);
                    g_state.unlocked_doors.insert(kStageDoors[door_idx].door_id);
                }
                g_pending_keys.fetch_add(1);
                g_state.add_log("Received Key: " + std::string(kStageDoors[door_idx].level_name) + "!");
            } else if (item_id >= 7200042 && item_id <= 7200141) {
                int gem_offset = item_id - 7200042;
                int stage_idx = gem_offset / 5;
                int color_idx = gem_offset % 5;
                const char* colors[5] = {"Red", "Blue", "Green", "Yellow", "Purple"};
                std::string stage_key = (stage_idx >= 0 && stage_idx < 20)
                    ? std::string(kStageDoors[stage_idx].vanilla_wad)
                    : ("STAGE_" + std::to_string(stage_idx + 1));
                std::string stage_name = (stage_idx >= 0 && stage_idx < 20)
                    ? std::string(kStageDoors[stage_idx].level_name)
                    : stage_key;
                uint8_t prev = g_state.stage_colored_crystals[stage_key];
                uint8_t next = static_cast<uint8_t>(prev | (1 << color_idx));
                g_state.stage_colored_crystals[stage_key] = next;
                if (find_stage_index_by_wad(g_state.current_wad) == stage_idx) {
                    g_pending_colored_gems.fetch_add(1);
                }
                g_state.add_log(std::string("Received ") + colors[color_idx] + " Crystal (" + stage_name + ")!");
                if (next == CRYSTAL_ALL_5 && g_state.activated_golden_gobbos.find(stage_key) == g_state.activated_golden_gobbos.end()) {
                    g_state.activated_golden_gobbos.insert(stage_key);
                    g_state.add_log("ALL 5 CRYSTALS! Golden Gobbo Activated for " + stage_name + "!");
                }
            } else {
                g_state.current_crystals += 5;
                g_pending_wallet_crystals.fetch_add(5);
                g_state.add_log("Received Archipelago Item (#" + std::to_string(item_id) + ")!");
            }
            break;
    }

    g_need_full_inventory_sync.store(true);

    std::string sender = sender_name;
    const bool is_multiworld = g_state.conn.ap_server_connected || g_state.client_connected;
    if (sender.empty() || sender == g_state.conn.slot_name || sender == "Player1" || sender == "Someone Else") {
        sender = get_solo_peer_name(0, item_id, true, g_state.custom_yaml_players);
    }
    std::string receiver = "Croc";

    // Trigger bottom toast for incoming item: [Sender] sent [Item] to [Croc]!
    // For direct grants/receives (give_item / multiworld items), this displays immediately.
    // For solo check pickups (record_persistent == false), the check function already displays the outgoing toast!
    if (record_persistent) {
        trigger_bottom_toast_locked(false, item_name, imp, sender, receiver, location_name);
    }

    if (!location_name.empty()) {
        if (!is_multiworld) {
            // In Solo / Single-game mode, show both the outgoing check and incoming item in the Archipelago feed!
            g_state.add_feed_entry(true, item_name, imp, sender, location_name);
        }
        g_state.add_feed_entry(false, item_name, imp, sender, location_name);
    }
}

static void step_internal_locked(float dt_seconds) {
    if (!g_state.is_mod_enabled_toggle()) {
        return;
    }

    // Advance Fire Breakout Trap flame visual effect and knockback timer (instant heart loss already dealt)
    if (g_state.fire_trap.active) {
        g_state.fire_trap.timer -= dt_seconds;
        if (g_state.fire_trap.timer <= 0.0f) {
            g_state.fire_trap.active = false;
            g_state.fire_trap.timer = 0.0f;
            g_state.pending_events_json.push_back(
                "{\"event\":\"fire_trap_resolved\",\"reason\":\"effect_finished\"}"
            );
        }
    }

    // Advance other timed traps (ice_physics, reverse_controls, speed_distortion)
    for (auto it = g_state.timed_traps.begin(); it != g_state.timed_traps.end();) {
        it->remaining_seconds -= dt_seconds;
        if (it->remaining_seconds <= 0.0f) {
            g_state.pending_events_json.push_back(
                "{\"event\":\"timed_trap_expired\",\"trap_type\":\"" + it->trap_type + "\"}"
            );
            it = g_state.timed_traps.erase(it);
        } else {
            ++it;
        }
    }
}

static std::string build_state_json_locked() {
    std::ostringstream ss;
    ss << "{"
       << "\"randomizer_active\":" << (g_state.is_randomizer_active() ? "true" : "false") << ","
       << "\"mod_enabled_toggle\":" << (g_state.is_mod_enabled_toggle() ? "true" : "false") << ","
       << "\"save_locked_to_mod\":" << (g_state.save_locked_to_mod ? "true" : "false") << ","
       << "\"in_active_save\":" << (g_state.in_active_save ? "true" : "false") << ","
       << "\"current_seed_hex\":\"" << json_escape(g_state.current_seed_hex) << "\","
       << "\"door_1_target\":\"" << json_escape(g_state.door_overrides["T1_D1"]) << "\","
       << "\"hud_banner\":\"" << json_escape(g_state.get_hud_banner()) << "\","
       << "\"server_host\":\"" << json_escape(g_state.conn.server_host) << "\","
       << "\"server_port\":" << g_state.conn.server_port << ","
       << "\"slot_name\":\"" << json_escape(g_state.conn.slot_name) << "\","
       << "\"ap_server_connected\":" << (g_state.conn.ap_server_connected ? "true" : "false") << ","
       << "\"current_wad\":\"" << g_state.current_wad << "\","
       << "\"current_hearts\":" << g_state.current_hearts << ","
       << "\"max_heart_pots\":" << g_state.max_heart_pots << ","
       << "\"current_crystals\":" << g_state.current_crystals << ","
       << "\"jigsaw_pieces\":" << g_state.jigsaw_pieces << ","
       << "\"golden_gobbos\":" << g_state.golden_gobbos << ","
       << "\"trophies\":" << g_state.trophies << ","
       << "\"silver_keys\":" << g_state.silver_keys << ","
       << "\"gold_keys\":" << g_state.gold_keys << ","
       << "\"clockwork_gobbos\":" << g_state.clockwork_gobbos << ","
       << "\"red_jellies\":" << g_state.red_jellies << ","
       << "\"blue_jellies\":" << g_state.blue_jellies << ","
       << "\"green_jellies\":" << g_state.green_jellies << ","
       << "\"binoculars\":" << (g_state.binoculars ? "true" : "false") << ","
       << "\"item_feed_count\":" << g_state.item_feed.size() << ","
       << "\"abilities\":{"
       << "\"tail_whack\":" << (g_state.abilities.tail_whack ? "true" : "false") << ","
       << "\"butt_stomp\":" << (g_state.abilities.butt_stomp ? "true" : "false") << ","
       << "\"climbing_claws\":" << (g_state.abilities.climbing_claws ? "true" : "false") << ","
       << "\"power_gloves\":" << (g_state.abilities.power_gloves ? "true" : "false") << ","
       << "\"lever_wrench\":" << (g_state.abilities.lever_wrench ? "true" : "false") << ","
       << "\"vehicle_license\":" << (g_state.abilities.vehicle_license ? "true" : "false") << ","
       << "\"airplane_ammo_feed\":" << (g_state.abilities.airplane_ammo_feed ? "true" : "false")
       << "},"
       << "\"fire_trap\":{"
       << "\"active\":" << (g_state.fire_trap.active ? "true" : "false")
       << "}"
       << "}";
    return ss.str();
}

static std::string handle_command_locked(const std::string& req) {
    std::string cmd = extract_json_string(req, "command");

    if (cmd == "client_hello") {
        g_state.client_connected = true;
        g_state.add_log("Croc2Client connected via Localhost Bridge!");
        return "{\"status\":\"ok\",\"mode\":\"" + json_escape(g_state.get_hud_banner()) + "\",\"state\":" + build_state_json_locked() + "}";
    }

    if (cmd == "client_disconnect") {
        g_state.client_connected = false;
        g_state.add_log("Croc2Client disconnected (Standby / Vanilla Mode).");
        return "{\"status\":\"ok\",\"mode\":\"" + json_escape(g_state.get_hud_banner()) + "\"}";
    }

    if (cmd == "set_connection_info") {
        std::string host = extract_json_string(req, "server_host");
        int port = extract_json_int(req, "server_port", g_state.conn.server_port);
        std::string slot = extract_json_string(req, "slot_name"); // Exact case-sensitive!
        std::string pass = extract_json_string(req, "password");
        if (!host.empty()) g_state.conn.server_host = host;
        if (port > 0) g_state.conn.server_port = port;
        if (!slot.empty()) g_state.conn.slot_name = slot;
        g_state.conn.password = pass;
        save_config_ini(nullptr);
        g_state.add_log("Updated Server Info: " + g_state.conn.slot_name + " @ " + g_state.conn.server_host + ":" + std::to_string(g_state.conn.server_port));
        return "{\"status\":\"ok\",\"state\":" + build_state_json_locked() + "}";
    }

    if (cmd == "toggle_mod") {
        if (g_state.save_locked_to_mod) {
            g_state.add_log("Mod is LOCKED to this Randomizer Save (Seed #" + g_state.current_seed_hex + ")! Return to Title Screen or delete save to toggle.");
            return "{\"status\":\"locked_to_save\",\"active\":true,\"mod_enabled_toggle\":true,"
                   "\"save_locked_to_mod\":true,\"banner\":\"" + json_escape(g_state.get_hud_banner()) + "\"}";
        }
        g_state.manual_override_disabled = !g_state.manual_override_disabled;
        g_state.add_log(g_state.get_hud_banner());
        g_state.pending_events_json.push_back(
            "{\"event\":\"mod_toggled\",\"active\":" +
            std::string(g_state.is_randomizer_active() ? "true" : "false") +
            ",\"banner\":\"" + json_escape(g_state.get_hud_banner()) + "\"}"
        );
        return "{\"status\":\"ok\",\"active\":" + std::string(g_state.is_randomizer_active() ? "true" : "false") +
               ",\"mod_enabled_toggle\":" + std::string(g_state.is_mod_enabled_toggle() ? "true" : "false") +
               ",\"save_locked_to_mod\":false" +
               ",\"banner\":\"" + json_escape(g_state.get_hud_banner()) + "\"}";
    }

    if (cmd == "start_new_save") {
        int slot = extract_json_int(req, "slot", 0);
        uint64_t custom_seed = static_cast<uint64_t>(extract_json_int(req, "seed", 0));
        g_state.active_save_slot = slot;
        g_state.in_active_save = true;
        if (g_state.is_mod_enabled_toggle()) {
            g_state.save_locked_to_mod = true;
            generate_new_save_seed_locked(custom_seed);
            g_state.add_log("Started New Randomizer Save (Slot " + std::to_string(slot + 1) +
                            ", Seed #" + g_state.current_seed_hex + ") - Mod Locked ON!");
        } else {
            g_state.save_locked_to_mod = false;
            sync_state_to_save_footer_locked();
            g_state.add_log("Started New Vanilla Save (Slot " + std::to_string(slot + 1) + ").");
        }
        return "{\"status\":\"ok\",\"state\":" + build_state_json_locked() + "}";
    }

    if (cmd == "get_state") {
        return "{\"status\":\"ok\",\"state\":" + build_state_json_locked() + "}";
    }

    if (cmd == "return_to_title") {
        g_state.in_active_save = false;
        g_state.save_locked_to_mod = false;
        g_state.add_log("Returned to Title Screen — Mod Toggle Unlocked.");
        return "{\"status\":\"ok\",\"state\":" + build_state_json_locked() + "}";
    }

    if (cmd == "claim_solo_check") {
        int stage_idx = extract_json_int(req, "stage_idx", 0);
        int sub_check_idx = extract_json_int(req, "sub_check_idx", 0);
        std::string desc = extract_json_string(req, "description");
        if (desc.empty()) desc = "Stage " + std::to_string(stage_idx + 1) + " Check " + std::to_string(sub_check_idx + 1);
        claim_solo_check_locked(stage_idx, sub_check_idx, desc);
        return "{\"status\":\"ok\",\"state\":" + build_state_json_locked() + "}";
    }

    if (cmd == "collect_white_crystal") {
        int stage_idx = extract_json_int(req, "stage_idx", 0);
        int crystal_idx = extract_json_int(req, "crystal_index", 1);
        bool sent_check = claim_white_crystal_check_locked(stage_idx, crystal_idx);
        int loc_id = 7211000 + stage_idx * 100 + crystal_idx;
        return "{\"status\":\"ok\",\"check_triggered\":" + std::string(sent_check ? "true" : "false") +
               ",\"location_id\":" + std::to_string(loc_id) +
               ",\"current_crystals\":" + std::to_string(g_state.current_crystals) +
               ",\"state\":" + build_state_json_locked() + "}";
    }

    if (cmd == "configure_session") {
        g_state.client_connected = true;
        g_state.colored_crystals_mode = extract_json_int(req, "colored_crystals_mode", 1);
        g_state.shuffle_white_crystals = extract_json_int(req, "shuffle_white_crystals", 3);
        g_state.lock_level_access = extract_json_int(req, "lock_level_access", 1) != 0;
        g_state.shuffle_abilities = extract_json_int(req, "shuffle_abilities", 1) != 0;
        g_state.shuffle_levers = extract_json_int(req, "shuffle_levers", 1) != 0;
        g_state.shuffle_vehicles = extract_json_int(req, "shuffle_vehicles", 1) != 0;
        g_state.shuffle_plane_guns = extract_json_int(req, "shuffle_plane_guns", 1) != 0;
        g_state.shuffle_pete_store = extract_json_int(req, "shuffle_pete_store", 1);

        if (!g_state.shuffle_abilities) {
            g_state.abilities.tail_whack = true;
            g_state.abilities.butt_stomp = true;
            g_state.abilities.climbing_claws = true;
            g_state.abilities.power_gloves = true;
        }
        if (!g_state.shuffle_levers) g_state.abilities.lever_wrench = true;
        if (!g_state.shuffle_vehicles) g_state.abilities.vehicle_license = true;
        if (!g_state.shuffle_plane_guns) g_state.abilities.airplane_ammo_feed = true;
        return "{\"status\":\"ok\",\"state\":" + build_state_json_locked() + "}";
    }

    if (cmd == "set_door_destination") {
        std::string src = extract_json_string(req, "source_door");
        std::string dst = extract_json_string(req, "target_wad");
        if (!src.empty() && !dst.empty()) {
            register_door_override_locked(src, dst);
        }
        return "{\"status\":\"ok\",\"source_door\":\"" + src + "\",\"target_wad\":\"" + dst + "\"}";
    }

    if (cmd == "enter_door") {
        std::string door_id = extract_json_string(req, "door_id");
        std::string default_wad = extract_json_string(req, "default_wad");
        std::string resolved_wad = default_wad;
        if (g_state.is_mod_enabled_toggle()) {
            auto it = g_state.door_overrides.find(door_id);
            if (it != g_state.door_overrides.end()) {
                resolved_wad = it->second;
            }
            if (g_state.colored_crystals_mode == 2 && !resolved_wad.empty()) {
                g_state.stage_colored_crystals[resolved_wad] = CRYSTAL_ALL_5;
                g_state.activated_golden_gobbos.insert(resolved_wad);
            }
        }
        g_state.current_wad = resolved_wad;
        g_state.pending_events_json.push_back(
            "{\"event\":\"level_transition\",\"door_id\":\"" + door_id + "\",\"wad\":\"" + resolved_wad + "\"}"
        );
        return "{\"status\":\"ok\",\"resolved_wad\":\"" + resolved_wad + "\"}";
    }

    if (cmd == "notify_item_sent") {
        std::string item_name = extract_json_string(req, "item_name");
        std::string cls = extract_json_string(req, "classification");
        std::string receiver = extract_json_string(req, "receiver");
        std::string location = extract_json_string(req, "location");
        if (receiver.empty() || receiver == "Player1" || receiver == "Someone Else") {
            receiver = g_state.get_default_yaml_receiver();
        }
        if (location.empty()) location = g_state.current_wad;
        ItemImportance imp = parse_importance_string(cls, item_name);
        trigger_bottom_toast_locked(true, item_name, imp, "Croc", receiver, location);
        g_state.add_feed_entry(true, item_name, imp, receiver, location);
        g_state.add_log("Sent " + item_name + " to " + receiver + " (" + location + ")");
        if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
        return "{\"status\":\"ok\",\"item_feed_count\":" + std::to_string(g_state.item_feed.size()) + "}";
    }

    if (cmd == "get_item_feed") {
        std::ostringstream ss;
        ss << "{\"status\":\"ok\",\"feed\":[";
        for (size_t i = 0; i < g_state.item_feed.size(); ++i) {
            if (i > 0) ss << ",";
            const auto& e = g_state.item_feed[i];
            ss << "{"
               << "\"direction\":\"" << (e.is_outgoing ? "OUT" : "IN") << "\","
               << "\"item_name\":\"" << json_escape(e.item_name) << "\","
               << "\"importance\":\"" << importance_to_string(e.importance) << "\","
               << "\"player_name\":\"" << json_escape(e.player_name) << "\","
               << "\"location_name\":\"" << json_escape(e.location_name) << "\""
               << "}";
        }
        ss << "]}";
        return ss.str();
    }

    if (cmd == "give_item") {
        int item_id = extract_json_int(req, "item_id", 0);
        std::string item_name = extract_json_string(req, "item_name");
        std::string group = extract_json_string(req, "group");
        std::string wad = extract_json_string(req, "wad");
        std::string color = extract_json_string(req, "color");
        std::string sender = extract_json_string(req, "sender");
        std::string location = extract_json_string(req, "location");
        std::string cls = extract_json_string(req, "classification");

        // Known Croc 2 item IDs go through the single canonical ID table so the item is guaranteed to
        // reach both the mod state AND the live croc2.exe inventory wheel / HUD.
        if (item_id >= 7200001 && item_id <= 7200156) {
            int item_index = extract_json_int(req, "item_index", -1);
            if (item_index >= 0 && item_index < g_state.ap_items_applied) {
                // Already applied to this save (bridge reconnect / re-sync) - don't grant twice!
                return "{\"status\":\"ok\",\"duplicate\":true,\"item_id\":" + std::to_string(item_id) + "}";
            }
            apply_ap_item_by_id_locked(
                item_id,
                sender.empty() ? g_state.conn.slot_name : sender,
                location.empty() ? "Archipelago" : location,
                true
            );
            g_state.ap_items_applied = (item_index >= 0) ? (item_index + 1) : (g_state.ap_items_applied + 1);
            sync_state_to_save_footer_locked();
            if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
            bool golden_active = !wad.empty() &&
                                 (g_state.activated_golden_gobbos.find(wad) != g_state.activated_golden_gobbos.end());
            return "{\"status\":\"ok\",\"item_id\":" + std::to_string(item_id) +
                   ",\"item_name\":\"" + json_escape(item_name) + "\",\"golden_gobbo_active\":" +
                   (golden_active ? "true" : "false") + ",\"state\":" + build_state_json_locked() + "}";
        }

        if (item_id > 0) {
            g_state.ap_received_item_ids.push_back(item_id);
        }

        if (item_name == "Tail Whack") g_state.abilities.tail_whack = true;
        else if (item_name == "Butt Stomp") g_state.abilities.butt_stomp = true;
        else if (item_name == "Climbing Claws") g_state.abilities.climbing_claws = true;
        else if (item_name == "Power Gloves") g_state.abilities.power_gloves = true;
        else if (item_name == "Lever Wrench") g_state.abilities.lever_wrench = true;
        else if (item_name == "Vehicle License") g_state.abilities.vehicle_license = true;
        else if (item_name == "Airplane Ammo Feed") g_state.abilities.airplane_ammo_feed = true;
        else if (item_name == "Jigsaw Piece" || group == "Jigsaw Pieces") {
            g_state.jigsaw_pieces += 1;
            g_pending_jigsaws.fetch_add(1);
        } else if (item_name == "Golden Gobbo") {
            g_state.golden_gobbos += 1;
            g_pending_golden_gobbos.fetch_add(1);
        } else if (item_name == "Trophy") {
            g_state.trophies += 1;
            g_pending_golden_gobbos.fetch_add(1);
        } else if (item_name == "Heart Pot") {
            g_state.max_heart_pots += 1;
            g_state.current_hearts += 1;
            g_pending_heart_pots.fetch_add(1);
        } else if (item_name == "Silver Key") {
            g_state.silver_keys += 1;
            g_pending_keys.fetch_add(1);
        } else if (item_name == "Gold Key") {
            g_state.gold_keys += 1;
            g_pending_keys.fetch_add(1);
        } else if (item_name == "Clockwork Gobbo") {
            g_state.clockwork_gobbos += 1;
            g_pending_clockwork.fetch_add(1);
        } else if (item_name == "Gummi Jump (Red Jelly)") {
            g_state.red_jellies += 1;
            g_pending_red_jellies.fetch_add(1);
        } else if (item_name == "Jelly Jump (Blue Jelly)") {
            g_state.blue_jellies += 1;
            g_pending_blue_jellies.fetch_add(1);
        } else if (item_name == "High Jelly Jump (Green Jelly)") {
            g_state.green_jellies += 1;
            g_pending_green_jellies.fetch_add(1);
        } else if (item_name == "Binoculars") {
            g_state.binoculars = true;
            g_pending_binoculars.fetch_add(1);
        } else if (item_name == "Extra Life") {
            g_state.current_hearts = g_state.max_heart_pots;
            g_pending_heart_pots.fetch_add(1);
        } else if (item_name == "Croc Crystals (1)") {
            g_state.current_crystals += 1;
            g_pending_wallet_crystals.fetch_add(1);
        } else if (item_name == "Croc Crystals (5)") {
            g_state.current_crystals += 5;
            g_pending_wallet_crystals.fetch_add(5);
        } else if (item_name == "Croc Crystals (10)") {
            g_state.current_crystals += 10;
            g_pending_wallet_crystals.fetch_add(10);
        } else if (item_name == "Croc Crystals (50)") {
            g_state.current_crystals += 50;
            g_pending_wallet_crystals.fetch_add(50);
        } else if (item_name == "Croc Crystals (100)") {
            g_state.current_crystals += 100;
            g_pending_wallet_crystals.fetch_add(100);
        } else if (group == "VIP Passes") {
            g_state.vip_passes.insert(item_name);
        } else if (group == "Level Keys") {
            if (wad.empty()) {
                for (int i = 0; i < 20; ++i) {
                    if (item_name.find(kStageDoors[i].level_name) != std::string::npos ||
                        item_name.find(kStageDoors[i].door_id) != std::string::npos ||
                        item_name.find(kStageDoors[i].vanilla_wad) != std::string::npos) {
                        wad = kStageDoors[i].vanilla_wad;
                        break;
                    }
                }
            }
            if (!wad.empty()) g_state.unlocked_doors.insert(wad);
            g_pending_keys.fetch_add(1);
        } else if (group == "Colored Crystals") {
            if (wad.empty()) {
                for (int i = 0; i < 20; ++i) {
                    if (item_name.find(kStageDoors[i].level_name) != std::string::npos ||
                        item_name.find(kStageDoors[i].door_id) != std::string::npos ||
                        item_name.find(kStageDoors[i].vanilla_wad) != std::string::npos) {
                        wad = kStageDoors[i].vanilla_wad;
                        break;
                    }
                }
            }
            if (!wad.empty()) {
                uint8_t bit = color_to_bit(color);
                uint8_t prev = g_state.stage_colored_crystals[wad];
                uint8_t next = static_cast<uint8_t>(prev | bit);
                g_state.stage_colored_crystals[wad] = next;
                if (find_stage_index_by_wad(g_state.current_wad) == find_stage_index_by_wad(wad)) {
                    g_pending_colored_gems.fetch_add(1);
                }
                int s_idx = find_stage_index_by_wad(wad);
                std::string stage_disp = (s_idx >= 0) ? kStageDoors[s_idx].level_name : wad;
                if (next == CRYSTAL_ALL_5 && g_state.activated_golden_gobbos.find(wad) == g_state.activated_golden_gobbos.end()) {
                    g_state.activated_golden_gobbos.insert(wad);
                    g_state.add_log("ALL 5 CRYSTALS RECEIVED! Golden Gobbo Activated for " + stage_disp + "!");
                    g_state.pending_events_json.push_back(
                        "{\"event\":\"golden_gobbo_activated\",\"wad\":\"" + wad + "\",\"level_name\":\"" + json_escape(stage_disp) + "\"}"
                    );
                }
            }
        }

        g_need_full_inventory_sync.store(true);
        sync_state_to_save_footer_locked();

        ItemImportance imp = parse_importance_string(cls, item_name);
        g_state.add_feed_entry(
            false,
            item_name,
            imp,
            sender.empty() ? g_state.conn.slot_name : sender,
            location.empty() ? "Archipelago" : location
        );
        g_state.add_log("Received Item: " + item_name);
        if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);

        bool golden_active = !wad.empty() &&
                             (g_state.activated_golden_gobbos.find(wad) != g_state.activated_golden_gobbos.end());
        return "{\"status\":\"ok\",\"item_name\":\"" + json_escape(item_name) + "\",\"golden_gobbo_active\":" +
               (golden_active ? "true" : "false") + "}";
    }

    if (cmd == "apply_trap") {
        if (!g_state.is_mod_enabled_toggle()) {
            return "{\"status\":\"ignored_vanilla_mode\"}";
        }
        std::string trap_type = extract_json_string(req, "trap_type");
        std::string sender = extract_json_string(req, "sender");
        std::string location = extract_json_string(req, "location");
        std::string display_name = trap_type;

        if (trap_type == "fire_breakout") {
            display_name = "Fire Breakout Trap";
            if (g_state.current_hearts > 0) g_state.current_hearts -= 1;
            g_pending_heart_damage.fetch_add(1);
            g_state.fire_trap.active = true;
            g_state.fire_trap.timer = 2.0f;
            g_state.add_log("🔥 FIRE BREAKOUT TRAP! Instant Heart Lost with Fire Knockback!");
            g_state.pending_events_json.push_back(
                "{\"event\":\"fire_trap_applied\",\"damage_hearts\":1,\"hearts_remaining\":" +
                std::to_string(g_state.current_hearts) + "}"
            );
        } else if (trap_type == "health_drain") {
            display_name = "Health Drain Trap";
            if (g_state.current_hearts > 0) g_state.current_hearts -= 1;
            g_pending_heart_damage.fetch_add(1);
            g_state.add_log("TRAP! Health Drain (-1 Heart)!");
            g_state.pending_events_json.push_back(
                "{\"event\":\"health_drain_applied\",\"hearts_remaining\":" + std::to_string(g_state.current_hearts) + "}"
            );
        } else if (trap_type == "dantini_ambush") {
            display_name = "Dantini Ambush Trap";
            g_state.timed_traps.push_back({"dantini_ambush", 10.0f});
            g_state.add_log("TRAP! Dantini Ambush active for 10s!");
            g_state.pending_events_json.push_back("{\"event\":\"dantini_ambush_spawned\",\"duration\":10.0,\"count\":3}");
        } else {
            if (trap_type == "ice_physics") display_name = "Ice Physics Trap";
            else if (trap_type == "reverse_controls") display_name = "Reverse Controls Trap";
            else if (trap_type == "speed_distortion") display_name = "Speed Distortion Trap";
            else if (trap_type == "tail_knot") display_name = "Tail-Knot Trap";
            else if (trap_type == "lead_boots") display_name = "Lead Boots Trap";
            else if (trap_type == "darkness") display_name = "Darkness Trap";
            float dur = 10.0f; // All ill effects last a flat 10 seconds!
            g_state.timed_traps.push_back({trap_type, dur});
            g_state.add_log("TRAP! " + display_name + " active for 10s!");
            g_state.pending_events_json.push_back(
                "{\"event\":\"timed_trap_started\",\"trap_type\":\"" + trap_type + "\",\"duration\":10.0}"
            );
        }

        std::string trap_sender = (sender.empty() || sender == "Player1" || sender == "Someone Else") ? get_solo_peer_name(0, 99, true, g_state.custom_yaml_players) : sender;
        trigger_bottom_toast_locked(false, display_name, ItemImportance::TRAP, trap_sender, "Croc", location);

        g_state.add_feed_entry(
            false,
            display_name,
            ItemImportance::TRAP,
            trap_sender,
            location.empty() ? "Archipelago" : location
        );
        if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
        return "{\"status\":\"ok\",\"trap_type\":\"" + trap_type + "\"}";
    }

    if (cmd == "player_action") {
        std::string action = extract_json_string(req, "action");
        return "{\"status\":\"ok\",\"action\":\"" + action + "\"}";
    }

    if (cmd == "step_tick") {
        float dt = extract_json_float(req, "dt", 1.5f);
        step_internal_locked(dt);
        return "{\"status\":\"ok\",\"state\":" + build_state_json_locked() + "}";
    }

    if (cmd == "report_check") {
        int loc_id = extract_json_int(req, "location_id", 0);
        if (loc_id > 0 && g_state.is_randomizer_active()) {
            g_state.pending_events_json.push_back(
                "{\"event\":\"location_checked\",\"location_id\":" + std::to_string(loc_id) + "}"
            );
            if (g_state.conn.ap_server_connected) {
                send_ws_location_check_locked(loc_id);
            }
        }
        return "{\"status\":\"ok\",\"location_id\":" + std::to_string(loc_id) + "}";
    }

    if (cmd == "poll_events") {
        std::ostringstream ss;
        ss << "{\"status\":\"ok\",\"state\":" << build_state_json_locked() << ",\"events\":[";
        for (size_t i = 0; i < g_state.pending_events_json.size(); ++i) {
            if (i > 0) ss << ",";
            ss << g_state.pending_events_json[i];
        }
        ss << "]}";
        g_state.pending_events_json.clear();
        return ss.str();
    }

    if (cmd == "toggle_overlay") {
        g_state.overlay_full_visible = !g_state.overlay_full_visible;
        if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
        return "{\"status\":\"ok\",\"overlay_full_visible\":" +
               std::string(g_state.overlay_full_visible ? "true" : "false") + "}";
    }

    if (cmd == "get_overlay_state") {
        return "{\"status\":\"ok\",\"overlay_full_visible\":" +
               std::string(g_state.overlay_full_visible ? "true" : "false") +
               ",\"small_indicator_active\":true}";
    }

    if (cmd == "get_bottom_toast") {
        std::ostringstream ss;
        std::string p1 = g_state.active_toast.sender.empty() ? "Croc" : g_state.active_toast.sender;
        std::string p2 = g_state.active_toast.receiver.empty() ? "Croc" : g_state.active_toast.receiver;
        std::string formatted;
        if (g_state.active_toast.is_self || p1 == p2 || (p1 == "Croc" && p2 == "Croc")) {
            formatted = p1 + " got his own " + g_state.active_toast.item_name + "!";
        } else {
            formatted = p1 + " sent " + g_state.active_toast.item_name + " to " + p2 + "!";
        }
        ss << "{\"status\":\"ok\","
           << "\"active\":" << (g_state.toast_active ? "true" : "false") << ","
           << "\"is_outgoing\":" << (g_state.active_toast.is_outgoing ? "true" : "false") << ","
           << "\"is_self\":" << ((g_state.active_toast.is_self || p1 == p2) ? "true" : "false") << ","
           << "\"sender\":\"" << json_escape(p1) << "\","
           << "\"receiver\":\"" << json_escape(p2) << "\","
           << "\"item_name\":\"" << json_escape(g_state.active_toast.item_name) << "\","
           << "\"importance\":\"" << importance_to_string(g_state.active_toast.importance) << "\","
           << "\"location\":\"" << json_escape(g_state.active_toast.location) << "\","
           << "\"formatted\":\"" << json_escape(formatted) << "\""
           << "}";
        return ss.str();
    }

    if (cmd == "get_yaml_players") {
        std::ostringstream ss;
        ss << "{\"status\":\"ok\",\"players\":[";
        for (size_t i = 0; i < g_state.custom_yaml_players.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "\"" << json_escape(g_state.custom_yaml_players[i]) << "\"";
        }
        ss << "]}";
        return ss.str();
    }

    if (cmd == "set_yaml_players") {
        std::string custom_dir = extract_json_string(req, "dir");
        if (!custom_dir.empty()) {
            g_state.yaml_players_dir = custom_dir;
            load_yaml_player_names();
        }
        size_t arr_start = req.find("\"players\":[");
        if (arr_start != std::string::npos) {
            arr_start += 11;
            size_t arr_end = req.find(']', arr_start);
            if (arr_end != std::string::npos) {
                std::string arr_str = req.substr(arr_start, arr_end - arr_start);
                std::vector<std::string> new_players;
                size_t p = 0;
                while (p < arr_str.size()) {
                    size_t q1 = arr_str.find('"', p);
                    if (q1 == std::string::npos) break;
                    size_t q2 = arr_str.find('"', q1 + 1);
                    if (q2 == std::string::npos) break;
                    std::string pl = arr_str.substr(q1 + 1, q2 - q1 - 1);
                    if (!pl.empty() && std::find(new_players.begin(), new_players.end(), pl) == new_players.end()) {
                        new_players.push_back(pl);
                    }
                    p = q2 + 1;
                }
                if (!new_players.empty()) {
                    g_state.custom_yaml_players = new_players;
                }
            }
        }
        return "{\"status\":\"ok\",\"count\":" + std::to_string(g_state.custom_yaml_players.size()) + "}";
    }

    return "{\"status\":\"error\",\"message\":\"unknown_command\"}";
}

// ============================================================================
// Built-In Native Archipelago WebSocket Client (via WinHTTP)
// ============================================================================
static HINTERNET g_ws_session = nullptr;
static HINTERNET g_ws_connect = nullptr;
static HINTERNET g_ws_handle = nullptr;

static void send_ws_location_check_locked(int location_id) {
    if (!g_ws_handle || location_id <= 0) return;
    std::string pkt = "[{\"cmd\":\"LocationChecks\",\"locations\":[" + std::to_string(location_id) + "]}]";
    WinHttpWebSocketSend(
        g_ws_handle,
        WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        reinterpret_cast<PVOID>(const_cast<char*>(pkt.data())),
        static_cast<DWORD>(pkt.size())
    );
}

static void close_winhttp_handles() {
    if (g_ws_handle) {
        WinHttpCloseHandle(g_ws_handle);
        g_ws_handle = nullptr;
    }
    if (g_ws_connect) {
        WinHttpCloseHandle(g_ws_connect);
        g_ws_connect = nullptr;
    }
    if (g_ws_session) {
        WinHttpCloseHandle(g_ws_session);
        g_ws_session = nullptr;
    }
}

static void ap_websocket_worker() {
    std::string host;
    int port = 38281;
    std::string slot;
    std::string pass;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        host = g_state.conn.server_host;
        port = g_state.conn.server_port;
        slot = g_state.conn.slot_name;
        pass = g_state.conn.password;
        g_state.conn.connecting = true;
        g_state.conn.ap_server_connected = false;
        g_state.conn.status_text = "Connecting to " + host + ":" + std::to_string(port) + " as '" + slot + "'...";
        g_state.add_log(g_state.conn.status_text);
    }
    if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);

    std::wstring whost(host.begin(), host.end());
    g_ws_session = WinHttpOpen(L"Croc2ArchipelagoClient/0.1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!g_ws_session) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.conn.connecting = false;
        g_state.conn.status_text = "Failed to initialize WinHTTP session.";
        g_state.add_log(g_state.conn.status_text);
        if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
        return;
    }

    g_ws_connect = WinHttpConnect(g_ws_session, whost.c_str(), static_cast<INTERNET_PORT>(port), 0);
    if (!g_ws_connect) {
        close_winhttp_handles();
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.conn.connecting = false;
        g_state.conn.status_text = "Could not resolve host: " + host;
        g_state.add_log(g_state.conn.status_text);
        if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
        return;
    }

    // Try plain ws:// first on localhost, wss:// first on remote hosts, with automatic fallback
    bool try_tls_first = (host != "localhost" && host != "127.0.0.1");
    bool connected_ws = false;

    for (int attempt = 0; attempt < 2 && g_ws_running.load(); ++attempt) {
        bool use_tls = (attempt == 0) ? try_tls_first : !try_tls_first;
        DWORD flags = use_tls ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hReq = WinHttpOpenRequest(g_ws_connect, L"GET", L"/", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!hReq) continue;

        if (use_tls) {
            DWORD sec_flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            WinHttpSetOption(hReq, WINHTTP_OPTION_SECURITY_FLAGS, &sec_flags, sizeof(sec_flags));
        }

        if (WinHttpSetOption(hReq, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0) &&
            WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hReq, nullptr)) {
            g_ws_handle = WinHttpWebSocketCompleteUpgrade(hReq, 0);
            WinHttpCloseHandle(hReq);
            if (g_ws_handle) {
                connected_ws = true;
                break;
            }
        } else {
            WinHttpCloseHandle(hReq);
        }
    }

    if (!connected_ws || !g_ws_handle) {
        close_winhttp_handles();
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.conn.connecting = false;
        g_state.conn.ap_server_connected = false;
        g_state.conn.status_text = "Connection failed to " + host + ":" + std::to_string(port) + " (Is Server Running?)";
        g_state.add_log(g_state.conn.status_text);
        if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
        return;
    }

    // Receive loop for Archipelago WebSocket packets
    std::string msg_buf;
    char recv_chunk[8192];
    while (g_ws_running.load()) {
        DWORD bytes_read = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE buf_type = WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
        DWORD err = WinHttpWebSocketReceive(g_ws_handle, recv_chunk, sizeof(recv_chunk), &bytes_read, &buf_type);
        if (err != NO_ERROR) break;

        if (buf_type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) break;
        msg_buf.append(recv_chunk, bytes_read);

        if (buf_type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE || buf_type == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
            std::string packet = msg_buf;
            msg_buf.clear();

            if (packet.find("\"RoomInfo\"") != std::string::npos) {
                // Send Archipelago Connect handshake with exact case-sensitive slot_name
                std::ostringstream conn_pkt;
                conn_pkt << "[{\"cmd\":\"Connect\","
                         << "\"password\":\"" << json_escape(pass) << "\","
                         << "\"game\":\"Croc 2\","
                         << "\"name\":\"" << json_escape(slot) << "\","
                         << "\"uuid\":\"croc2-ap-native-client\","
                         << "\"version\":{\"major\":0,\"minor\":6,\"build\":7,\"class\":\"Version\"},"
                         << "\"items_handling\":7,"
                         << "\"tags\":[\"AP\"],"
                         << "\"slot_data\":true}]";
                std::string out_str = conn_pkt.str();
                WinHttpWebSocketSend(
                    g_ws_handle,
                    WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                    reinterpret_cast<PVOID>(const_cast<char*>(out_str.data())),
                    static_cast<DWORD>(out_str.size())
                );
            }

            if (packet.find("\"Connected\"") != std::string::npos) {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_state.conn.connecting = false;
                g_state.conn.ap_server_connected = true;
                g_state.our_slot_id = extract_json_int(packet, "slot", 1);

                // Extract players array if provided in Connected packet
                size_t p_pos = packet.find("\"players\":");
                if (p_pos != std::string::npos) {
                    size_t p_arr = packet.find('[', p_pos);
                    size_t p_end = packet.find(']', p_arr);
                    if (p_arr != std::string::npos && p_end != std::string::npos) {
                        std::string p_sub = packet.substr(p_arr, p_end - p_arr + 1);
                        size_t obj_pos = 0;
                        while ((obj_pos = p_sub.find('{', obj_pos)) != std::string::npos) {
                            size_t obj_end = p_sub.find('}', obj_pos);
                            if (obj_end == std::string::npos) break;
                            std::string obj_str = p_sub.substr(obj_pos, obj_end - obj_pos + 1);
                            int s_id = extract_json_int(obj_str, "slot", -1);
                            std::string s_name = extract_json_string(obj_str, "name");
                            if (s_id > 0 && !s_name.empty()) {
                                g_state.slot_to_player_name[s_id] = s_name;
                                if (s_id != g_state.our_slot_id) {
                                    if (std::find(g_state.custom_yaml_players.begin(), g_state.custom_yaml_players.end(), s_name) == g_state.custom_yaml_players.end()) {
                                        g_state.custom_yaml_players.push_back(s_name);
                                    }
                                }
                            }
                            obj_pos = obj_end + 1;
                        }
                    }
                }

                // Extract door_pairing from slot_data if present
                for (const auto& entry : kStageDoors) {
                    std::string target = extract_json_string(packet, entry.door_id);
                    if (!target.empty() && target.size() >= 8 && target[0] == 'T') {
                        register_door_override_locked(entry.door_id, target);
                    }
                }
                g_state.conn.status_text = "Connected as '" + slot + "' @ " + host + ":" + std::to_string(port);
                g_state.add_log("SUCCESS: Connected to Archipelago Server as '" + slot + "'!");
                MessageBeep(MB_OK);
                if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
            }

            if (packet.find("\"PrintJSON\"") != std::string::npos && packet.find("\"ItemSend\"") != std::string::npos) {
                std::lock_guard<std::mutex> lock(g_mutex);
                int receiving_slot = extract_json_int(packet, "receiving", -1);
                int finder_slot = extract_json_int(packet, "player", -1);
                int item_id = extract_json_int(packet, "item", 0);
                std::string item_name;
                ItemImportance imp = ItemImportance::PROGRESSION;
                get_ap_item_info(item_id, item_name, imp);

                if (finder_slot == g_state.our_slot_id && receiving_slot != g_state.our_slot_id) {
                    std::string receiver_name = g_state.get_player_name_by_slot(receiving_slot);
                    trigger_bottom_toast_locked(true, item_name, imp, "Croc", receiver_name, "Archipelago Multiworld");
                    g_state.add_feed_entry(true, item_name, imp, receiver_name, "Archipelago Multiworld");
                }
            }

            if (packet.find("\"ConnectionRefused\"") != std::string::npos) {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_state.conn.connecting = false;
                g_state.conn.ap_server_connected = false;
                g_state.conn.status_text = "Connection Refused! Check Slot Name (Case-Sensitive) & Password.";
                g_state.add_log("ERROR: Archipelago Server refused connection for slot '" + slot + "'.");
                MessageBeep(MB_ICONHAND);
                if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
            }

            if (packet.find("\"ReceivedItems\"") != std::string::npos) {
                std::lock_guard<std::mutex> lock(g_mutex);
                int running_index = extract_json_int(packet, "index", 0);
                size_t search_pos = 0;
                while ((search_pos = packet.find("\"item\":", search_pos)) != std::string::npos) {
                    search_pos += 7;
                    while (search_pos < packet.size() && (packet[search_pos] == ' ' || packet[search_pos] == '\t')) search_pos++;
                    int item_id = std::atoi(packet.c_str() + search_pos);
                    if (running_index >= g_state.ap_items_applied) {
                        if (item_id >= 7200001 && item_id <= 7200999) {
                            apply_ap_item_by_id_locked(item_id, slot, "Archipelago Multiworld", true);
                        }
                        g_state.ap_items_applied = running_index + 1;
                    }
                    ++running_index;
                }
                sync_state_to_save_footer_locked();
                if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
                if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
            }
        }
    }

    close_winhttp_handles();
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.conn.connecting = false;
        if (g_state.conn.ap_server_connected) {
            g_state.conn.ap_server_connected = false;
            g_state.conn.status_text = "Disconnected from Archipelago Server.";
            g_state.add_log("Disconnected from Archipelago Server.");
        }
    }
    if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
}

static void start_ap_websocket_connection() {
    stop_ap_websocket_connection();
    g_ws_running.store(true);
    g_ws_thread = std::thread(ap_websocket_worker);
}

static void stop_ap_websocket_connection() {
    g_ws_running.store(false);
    close_winhttp_handles();
    if (g_ws_thread.joinable()) {
        g_ws_thread.join();
    }
}

// ============================================================================
// Croc-Themed In-Game Overlay Bar + Connection Menu UI (Win32 GDI)
// ============================================================================
static void draw_diamond_gem(HDC hdc, int cx, int cy, int radius, COLORREF fill_col, COLORREF border_col) {
    POINT pts[4] = {
        {cx, cy - radius},
        {cx + radius, cy},
        {cx, cy + radius},
        {cx - radius, cy}
    };
    HBRUSH hBrush = CreateSolidBrush(fill_col);
    HPEN hPen = CreatePen(PS_SOLID, 1, border_col);
    HGDIOBJ oldB = SelectObject(hdc, hBrush);
    HGDIOBJ oldP = SelectObject(hdc, hPen);
    Polygon(hdc, pts, 4);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

static void refresh_menu_ui() {
    if (!g_menu_hwnd) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_btn_toggle_mod) {
        if (g_state.save_locked_to_mod) {
            std::string lbl = "Mode: LOCKED TO RANDOMIZER SAVE (Seed #" + g_state.current_seed_hex + ")";
            SetWindowTextA(g_btn_toggle_mod, lbl.c_str());
        } else {
            SetWindowTextA(
                g_btn_toggle_mod,
                g_state.is_mod_enabled_toggle()
                    ? "Mode: RANDOMIZER ENABLED  [Click or Press Shift+F10 to Toggle]"
                    : "Mode: VANILLA PASSTHROUGH [Click or Press Shift+F10 to Enable Mod]"
            );
        }
    }
    if (g_list_log) {
        SendMessageA(g_list_log, LB_RESETCONTENT, 0, 0);
        for (const auto& entry : g_state.activity_log) {
            SendMessageA(g_list_log, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(entry.c_str()));
        }
        int count = static_cast<int>(g_state.activity_log.size());
        if (count > 0) {
            SendMessageA(g_list_log, LB_SETTOPINDEX, count - 1, 0);
        }
    }
    InvalidateRect(g_menu_hwnd, nullptr, TRUE);
    if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
}

static COLORREF get_ap_importance_color(ItemImportance imp) {
    // Authentic Archipelago Text Client item importance palette (brightened for dark overlay legibility)
    switch (imp) {
        case ItemImportance::PROGRESSION: return RGB(195, 145, 255); // Plum / Bright Orchid (#AF99EF)
        case ItemImportance::USEFUL:      return RGB(110, 165, 255); // Slate Blue (#6D8BE8)
        case ItemImportance::FILLER:      return RGB(0, 238, 238);   // Cyan (#00EEEE)
        case ItemImportance::TRAP:        return RGB(255, 115, 95);  // Salmon / Coral (#FA8072)
        default:                          return RGB(0, 238, 238);
    }
}

static void draw_colored_seg(HDC hdc, int& x, int y, int max_x, const std::string& text, COLORREF col) {
    if (text.empty() || x >= max_x) return;
    SetTextColor(hdc, col);
    RECT clip_rc = {x, y, max_x, y + 18};
    ExtTextOutA(hdc, x, y, ETO_CLIPPED, &clip_rc, text.c_str(), static_cast<UINT>(text.size()), nullptr);
    SIZE sz{};
    if (GetTextExtentPoint32A(hdc, text.c_str(), static_cast<int>(text.size()), &sz)) {
        x += sz.cx;
    }
}

static LRESULT CALLBACK ToastWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        BottomToastNotification toast_snap;
        bool active = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            active = g_state.toast_active;
            toast_snap = g_state.active_toast;
        }

        if (active) {
            HBRUSH bgBrush = CreateSolidBrush(RGB(10, 28, 18));
            FillRect(memDC, &rc, bgBrush);
            DeleteObject(bgBrush);

            COLORREF border_col = (toast_snap.importance == ItemImportance::TRAP)
                ? RGB(255, 115, 95)
                : RGB(255, 215, 0);
            HPEN framePen = CreatePen(PS_SOLID, 2, border_col);
            HGDIOBJ oldPen = SelectObject(memDC, framePen);
            HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
            RoundRect(memDC, 2, 2, w - 2, h - 2, 12, 12);

            HPEN innerPen = CreatePen(PS_SOLID, 1, RGB(46, 204, 113));
            SelectObject(memDC, innerPen);
            RoundRect(memDC, 4, 4, w - 4, h - 4, 10, 10);
            SelectObject(memDC, oldPen);
            SelectObject(memDC, oldBrush);
            DeleteObject(framePen);
            DeleteObject(innerPen);

            draw_diamond_gem(memDC, 24, h / 2, 7, get_ap_importance_color(toast_snap.importance), RGB(255, 255, 255));

            HFONT hFontToast = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                          DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HGDIOBJ oldFont = SelectObject(memDC, hFontToast);
            SetBkMode(memDC, TRANSPARENT);

            COLORREF col_gold = RGB(255, 230, 95);
            COLORREF col_text = RGB(232, 242, 238);
            COLORREF col_item = get_ap_importance_color(toast_snap.importance);

            std::string p1 = toast_snap.sender.empty() ? "Croc" : toast_snap.sender;
            std::string p2 = toast_snap.receiver.empty() ? "Croc" : toast_snap.receiver;

            struct TextSegment { std::string text; COLORREF color; };
            std::vector<TextSegment> segs;
            if (toast_snap.is_self || p1 == p2 || (p1 == "Croc" && p2 == "Croc")) {
                segs.push_back({p1, col_gold});
                segs.push_back({" got his own ", col_text});
                segs.push_back({toast_snap.item_name, col_item});
                segs.push_back({"!", col_text});
            } else {
                segs.push_back({p1, col_gold});
                segs.push_back({" sent ", col_text});
                segs.push_back({toast_snap.item_name, col_item});
                segs.push_back({" to ", col_text});
                segs.push_back({p2, col_gold});
                segs.push_back({"!", col_text});
            }

            int total_text_w = 0;
            int text_h = 20;
            for (const auto& s : segs) {
                SIZE sz{};
                if (GetTextExtentPoint32A(memDC, s.text.c_str(), static_cast<int>(s.text.size()), &sz)) {
                    total_text_w += sz.cx;
                    if (sz.cy > text_h) text_h = sz.cy;
                }
            }

            int start_x = 44;
            if (w - 50 > total_text_w) {
                start_x = (w - total_text_w) / 2;
                if (start_x < 44) start_x = 44;
            }
            int start_y = (h - text_h) / 2;

            int cur_x = start_x;
            for (const auto& s : segs) {
                draw_colored_seg(memDC, cur_x, start_y, w - 12, s.text, s.color);
            }

            SelectObject(memDC, oldFont);
            DeleteObject(hFontToast);
        }

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_LBUTTONDOWN: {
            bool toggled_expand = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                if (!g_state.overlay_full_visible) {
                    g_state.overlay_full_visible = true;
                    toggled_expand = true;
                }
            }
            if (toggled_expand) {
                InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_menu_hwnd) {
                g_menu_visible = !g_menu_visible;
                ShowWindow(g_menu_hwnd, g_menu_visible ? SW_SHOW : SW_HIDE);
                if (g_menu_visible) {
                    SetForegroundWindow(g_menu_hwnd);
                    refresh_menu_ui();
                }
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

            bool mod_on = false;
            bool rando_active = false;
            bool save_locked = false;
            bool fire_active = false;
            bool full_visible = false;
            std::string line1;
            std::string line2;
            std::vector<OverlayFeedEntry> feed_snapshot;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                mod_on = g_state.is_mod_enabled_toggle();
                rando_active = g_state.is_randomizer_active();
                save_locked = g_state.save_locked_to_mod;
                fire_active = g_state.fire_trap.active;
                full_visible = g_state.overlay_full_visible;
                feed_snapshot = g_state.item_feed;

                if (!mod_on) {
                    line1 = "CROC 2 ARCHIPELAGO  |  VANILLA MODE (Mod Off)";
                } else if (g_state.conn.ap_server_connected) {
                    line1 = "CROC 2 AP  |  CONNECTED (" + g_state.conn.slot_name + ")  |  " + g_state.last_door_redirect_desc;
                } else if (save_locked) {
                    line1 = "CROC 2 RANDO  |  " + g_state.last_door_redirect_desc;
                } else {
                    line1 = "CROC 2 AP  |  MOD ON (Title Screen - New Save Generates Random Seed)";
                }

                if (fire_active) {
                    line2 = "🔥 FIRE BREAKOUT TRAP! (-1 Heart Knockback)";
                } else if (save_locked) {
                    line2 = "[F9] Menu | [F10] Minimize | Seed #" + g_state.current_seed_hex +
                            " | Jellies:" + std::to_string(g_state.red_jellies + g_state.blue_jellies + g_state.green_jellies) +
                            " Clockwork:" + std::to_string(g_state.clockwork_gobbos) +
                            " Keys:" + std::to_string(g_state.silver_keys + g_state.gold_keys);
                } else {
                    line2 = "[F9] Server & Slot Menu   |   [F10] Minimize Overlay";
                }
            }

            if (!full_visible) {
                // Minimal indicator badge (e.g. 220x28) indicating mod presence without taking screen space
                HBRUSH bgBrush = CreateSolidBrush(!mod_on ? RGB(30, 38, 34) : RGB(14, 46, 24));
                FillRect(memDC, &rc, bgBrush);
                DeleteObject(bgBrush);

                COLORREF border_col = !mod_on ? RGB(130, 145, 135) : RGB(255, 215, 0);
                HPEN goldPen = CreatePen(PS_SOLID, 1, border_col);
                HGDIOBJ oldPen = SelectObject(memDC, goldPen);
                HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
                RoundRect(memDC, 1, 1, w - 1, h - 1, 6, 6);
                SelectObject(memDC, oldPen);
                SelectObject(memDC, oldBrush);
                DeleteObject(goldPen);

                COLORREF orb_col = !mod_on ? RGB(150, 155, 150) : (rando_active ? RGB(46, 235, 110) : RGB(255, 205, 0));
                draw_diamond_gem(memDC, 14, h / 2, 5, orb_col, RGB(255, 255, 255));

                HFONT hFontMin = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                             DEFAULT_PITCH | FF_SWISS, "Segoe UI");
                HGDIOBJ oldFont = SelectObject(memDC, hFontMin);
                SetBkMode(memDC, TRANSPARENT);

                int mx = 26;
                int my = (h - 16) / 2;
                draw_colored_seg(memDC, mx, my, w - 6, "CROC 2 AP  |  ", RGB(255, 225, 65));
                draw_colored_seg(memDC, mx, my, w - 6, "[F10] Console", RGB(120, 245, 175));

                SelectObject(memDC, oldFont);
                DeleteObject(hFontMin);

                BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
                SelectObject(memDC, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDC);
                EndPaint(hwnd, &ps);
                return 0;
            }

            // Croc Jungle Emerald & Gobbo Gold Palette
            COLORREF bg_col = !mod_on ? RGB(30, 38, 34) : (fire_active ? RGB(95, 22, 16) : RGB(14, 46, 24));
            COLORREF border_col = !mod_on ? RGB(130, 145, 135) : (fire_active ? RGB(255, 90, 40) : RGB(255, 215, 0));

            HBRUSH bgBrush = CreateSolidBrush(bg_col);
            FillRect(memDC, &rc, bgBrush);
            DeleteObject(bgBrush);

            // Outer Gobbo Gold 2px frame + Inner Emerald accent line
            HPEN goldPen = CreatePen(PS_SOLID, 2, border_col);
            HGDIOBJ oldPen = SelectObject(memDC, goldPen);
            HGDIOBJ oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
            Rectangle(memDC, 1, 1, w - 1, h - 1);
            SelectObject(memDC, oldPen);
            DeleteObject(goldPen);

            HPEN innerPen = CreatePen(PS_SOLID, 1, RGB(46, 204, 113));
            SelectObject(memDC, innerPen);
            Rectangle(memDC, 3, 3, w - 3, h - 3);
            SelectObject(memDC, oldBrush);
            DeleteObject(innerPen);

            // Status LED orb + 5 Colored Crystal Diamonds on the left
            COLORREF orb_col = !mod_on ? RGB(150, 155, 150) : (rando_active ? RGB(46, 235, 110) : RGB(255, 205, 0));
            draw_diamond_gem(memDC, 18, 17, 7, orb_col, RGB(255, 255, 255));

            draw_diamond_gem(memDC, 12, 38, 4, RGB(255, 59, 48), RGB(255, 220, 220));   // Red Crystal
            draw_diamond_gem(memDC, 22, 38, 4, RGB(0, 140, 255), RGB(220, 240, 255));   // Blue Crystal
            draw_diamond_gem(memDC, 32, 38, 4, RGB(52, 199, 89), RGB(220, 255, 220));   // Green Crystal
            draw_diamond_gem(memDC, 42, 38, 4, RGB(255, 204, 0), RGB(255, 250, 220));   // Yellow Crystal
            draw_diamond_gem(memDC, 52, 38, 4, RGB(175, 82, 222), RGB(245, 220, 255));  // Purple Crystal

            SetBkMode(memDC, TRANSPARENT);
            HFONT hFontBold = CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                          DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hFontSub = CreateFontA(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                         DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hFontFeed = CreateFontA(13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                          DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            HGDIOBJ oldFont = SelectObject(memDC, hFontBold);
            SetTextColor(memDC, !mod_on ? RGB(200, 210, 205) : RGB(255, 225, 65));
            TextOutA(memDC, 64, 6, line1.c_str(), static_cast<int>(line1.size()));

            SelectObject(memDC, hFontSub);
            SetTextColor(memDC, fire_active ? RGB(255, 230, 180) : RGB(175, 245, 195));
            TextOutA(memDC, 64, 27, line2.c_str(), static_cast<int>(line2.size()));

            // =========================================================================
            // Archipelago Text Client Color-Coordinated Outgoing / Incoming Item Box
            // =========================================================================
            RECT feedRc = {8, 50, w - 8, h - 7};
            HBRUSH feedBrush = CreateSolidBrush(RGB(8, 22, 14));
            FillRect(memDC, &feedRc, feedBrush);
            DeleteObject(feedBrush);

            HPEN feedPen = CreatePen(PS_SOLID, 1, RGB(46, 204, 113));
            SelectObject(memDC, feedPen);
            SelectObject(memDC, GetStockObject(NULL_BRUSH));
            Rectangle(memDC, feedRc.left, feedRc.top, feedRc.right, feedRc.bottom);
            DeleteObject(feedPen);

            // Header bar with Archipelago Importance Color Legend
            SelectObject(memDC, hFontFeed);
            int hdr_x = 14;
            int hdr_y = 53;
            int max_x = w - 14;
            draw_colored_seg(memDC, hdr_x, hdr_y, max_x, "AP ITEM FEED  |  ", RGB(255, 215, 0));
            draw_colored_seg(memDC, hdr_x, hdr_y, max_x, "[Progression] ", get_ap_importance_color(ItemImportance::PROGRESSION));
            draw_colored_seg(memDC, hdr_x, hdr_y, max_x, "[Useful] ", get_ap_importance_color(ItemImportance::USEFUL));
            draw_colored_seg(memDC, hdr_x, hdr_y, max_x, "[Filler] ", get_ap_importance_color(ItemImportance::FILLER));
            draw_colored_seg(memDC, hdr_x, hdr_y, max_x, "[Trap] ", get_ap_importance_color(ItemImportance::TRAP));
            draw_colored_seg(memDC, hdr_x, hdr_y, max_x, "[Location]", RGB(85, 240, 140));

            HPEN divPen = CreatePen(PS_SOLID, 1, RGB(28, 82, 45));
            SelectObject(memDC, divPen);
            MoveToEx(memDC, feedRc.left + 4, 71, nullptr);
            LineTo(memDC, feedRc.right - 4, 71);
            DeleteObject(divPen);

            const COLORREF col_text   = RGB(225, 232, 228);
            const COLORREF col_player = RGB(255, 230, 95);  // Archipelago Player Gold
            const COLORREF col_loc    = RGB(85, 240, 140);  // Archipelago Location Green
            const COLORREF col_out    = RGB(255, 185, 80);  // [OUT] Badge Amber
            const COLORREF col_in     = RGB(105, 255, 165); // [IN]  Badge Mint

            if (feed_snapshot.empty()) {
                int rx = 14;
                int ry = 76;
                draw_colored_seg(memDC, rx, ry, max_x, "[READY] ", col_in);
                draw_colored_seg(memDC, rx, ry, max_x, "Collect any Crystal or Check to see ", col_text);
                draw_colored_seg(memDC, rx, ry, max_x, "Outgoing", col_out);
                draw_colored_seg(memDC, rx, ry, max_x, " & ", col_text);
                draw_colored_seg(memDC, rx, ry, max_x, "Incoming", col_in);
                draw_colored_seg(memDC, rx, ry, max_x, " items here!", col_text);
            } else {
                size_t max_rows = 6;
                size_t start_idx = (feed_snapshot.size() > max_rows) ? (feed_snapshot.size() - max_rows) : 0;
                for (size_t i = start_idx; i < feed_snapshot.size(); ++i) {
                    const auto& e = feed_snapshot[i];
                    int row_idx = static_cast<int>(i - start_idx);
                    int rx = 14;
                    int ry = 74 + row_idx * 17;
                    COLORREF item_col = get_ap_importance_color(e.importance);

                    if (e.is_outgoing) {
                        draw_colored_seg(memDC, rx, ry, max_x, "[OUT] ", col_out);
                        draw_colored_seg(memDC, rx, ry, max_x, "Croc sent ", col_text);
                        draw_colored_seg(memDC, rx, ry, max_x, e.item_name, item_col);
                        draw_colored_seg(memDC, rx, ry, max_x, " to ", col_text);
                        draw_colored_seg(memDC, rx, ry, max_x, e.player_name, col_player);
                        if (!e.location_name.empty()) {
                            draw_colored_seg(memDC, rx, ry, max_x, " (" + e.location_name + ")", col_loc);
                        }
                    } else {
                        draw_colored_seg(memDC, rx, ry, max_x, "[IN]  ", col_in);
                        draw_colored_seg(memDC, rx, ry, max_x, e.player_name, col_player);
                        draw_colored_seg(memDC, rx, ry, max_x, " sent ", col_text);
                        draw_colored_seg(memDC, rx, ry, max_x, e.item_name, item_col);
                        draw_colored_seg(memDC, rx, ry, max_x, " to Croc", col_text);
                        if (!e.location_name.empty()) {
                            draw_colored_seg(memDC, rx, ry, max_x, " (" + e.location_name + ")", col_loc);
                        }
                    }
                }
            }

            SelectObject(memDC, oldFont);
            DeleteObject(hFontBold);
            DeleteObject(hFontSub);
            DeleteObject(hFontFeed);

            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT hFont = CreateFontA(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            std::string host, port_str, slot, pass;
            bool mod_on = true;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                host = g_state.conn.server_host;
                port_str = std::to_string(g_state.conn.server_port);
                slot = g_state.conn.slot_name;
                pass = g_state.conn.password;
                mod_on = g_state.is_mod_enabled_toggle();
            }

            g_btn_toggle_mod = CreateWindowExA(
                0, "BUTTON",
                mod_on ? "Mode: RANDOMIZER ENABLED  [Click or Press Shift+F10 to Toggle]"
                       : "Mode: VANILLA PASSTHROUGH [Click or Press Shift+F10 to Enable Mod]",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 74, 480, 32, hwnd, reinterpret_cast<HMENU>(static_cast<uintptr_t>(IDC_BTN_TOGGLE_MOD)), nullptr, nullptr
            );

            g_edit_host = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", host.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 175, 122, 195, 25,
                hwnd, reinterpret_cast<HMENU>(static_cast<uintptr_t>(IDC_EDIT_HOST)), nullptr, nullptr);

            g_edit_port = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", port_str.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER, 425, 122, 75, 25,
                hwnd, reinterpret_cast<HMENU>(static_cast<uintptr_t>(IDC_EDIT_PORT)), nullptr, nullptr);

            // Note: NO ES_LOWERCASE or ES_UPPERCASE -> preserves exact case-sensitive slot name!
            g_edit_slot = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", slot.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 175, 156, 325, 25,
                hwnd, reinterpret_cast<HMENU>(static_cast<uintptr_t>(IDC_EDIT_SLOT)), nullptr, nullptr);

            g_edit_pass = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", pass.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD, 175, 190, 325, 25,
                hwnd, reinterpret_cast<HMENU>(static_cast<uintptr_t>(IDC_EDIT_PASS)), nullptr, nullptr);

            g_btn_connect = CreateWindowExA(0, "BUTTON", "Connect to Archipelago Server",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 20, 228, 250, 32,
                hwnd, reinterpret_cast<HMENU>(static_cast<uintptr_t>(IDC_BTN_CONNECT)), nullptr, nullptr);

            g_btn_disconnect = CreateWindowExA(0, "BUTTON", "Disconnect",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 280, 228, 105, 32,
                hwnd, reinterpret_cast<HMENU>(static_cast<uintptr_t>(IDC_BTN_DISCONNECT)), nullptr, nullptr);

            HWND btn_close = CreateWindowExA(0, "BUTTON", "Hide (F9)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 395, 228, 105, 32,
                hwnd, reinterpret_cast<HMENU>(static_cast<uintptr_t>(IDC_BTN_CLOSE_MENU)), nullptr, nullptr);

            g_list_log = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                20, 360, 480, 125,
                hwnd, reinterpret_cast<HMENU>(static_cast<uintptr_t>(IDC_LIST_LOG)), nullptr, nullptr);

            for (HWND ctrl : {g_btn_toggle_mod, g_edit_host, g_edit_port, g_edit_slot, g_edit_pass, g_btn_connect, g_btn_disconnect, btn_close, g_list_log}) {
                if (ctrl) SendMessageA(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
            }
            return 0;
        }
        case WM_APP + 1: {
            refresh_menu_ui();
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BTN_TOGGLE_MOD) {
                bool is_on = false;
                bool locked = false;
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    locked = g_state.save_locked_to_mod;
                    if (locked) {
                        g_state.add_log("Mod is LOCKED to this Randomizer Save (Seed #" + g_state.current_seed_hex + ")! Return to Title Screen or delete save to toggle.");
                    } else {
                        g_state.manual_override_disabled = !g_state.manual_override_disabled;
                        is_on = g_state.is_mod_enabled_toggle();
                        g_state.add_log(g_state.get_hud_banner());
                    }
                }
                if (locked) {
                    Beep(300, 140);
                } else {
                    Beep(is_on ? 880 : 440, 85);
                }
                refresh_menu_ui();
                return 0;
            }
            if (id == IDC_BTN_CONNECT) {
                char host_buf[256] = {0};
                char port_buf[64] = {0};
                char slot_buf[256] = {0};
                char pass_buf[256] = {0};
                GetWindowTextA(g_edit_host, host_buf, sizeof(host_buf));
                GetWindowTextA(g_edit_port, port_buf, sizeof(port_buf));
                GetWindowTextA(g_edit_slot, slot_buf, sizeof(slot_buf));
                GetWindowTextA(g_edit_pass, pass_buf, sizeof(pass_buf));

                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_state.conn.server_host = host_buf[0] ? host_buf : "localhost";
                    int p = std::atoi(port_buf);
                    g_state.conn.server_port = (p > 0) ? p : 38281;
                    g_state.conn.slot_name = slot_buf[0] ? slot_buf : "Player1"; // Preserves exact case!
                    g_state.conn.password = pass_buf;
                    g_state.manual_override_disabled = false;
                    save_config_ini(nullptr);
                }
                start_ap_websocket_connection();
                refresh_menu_ui();
                return 0;
            }
            if (id == IDC_BTN_DISCONNECT) {
                stop_ap_websocket_connection();
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_state.conn.status_text = "Disconnected (Standby / Vanilla Mode)";
                    g_state.add_log("Disconnected from Archipelago Server.");
                }
                refresh_menu_ui();
                return 0;
            }
            if (id == IDC_BTN_CLOSE_MENU) {
                g_menu_visible = false;
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            break;
        }
        case WM_CLOSE: {
            g_menu_visible = false;
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Deep Croc Jungle background
            HBRUSH bgBrush = CreateSolidBrush(RGB(16, 48, 26));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);

            // Header Banner
            RECT hdr = {0, 0, rc.right, 64};
            HBRUSH hdrBrush = CreateSolidBrush(RGB(10, 32, 17));
            FillRect(hdc, &hdr, hdrBrush);
            DeleteObject(hdrBrush);

            // Gold Border around dialog & header divider
            HPEN goldPen = CreatePen(PS_SOLID, 2, RGB(255, 215, 0));
            HGDIOBJ oldPen = SelectObject(hdc, goldPen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, 1, 1, rc.right - 1, rc.bottom - 1);
            MoveToEx(hdc, 0, 64, nullptr);
            LineTo(hdc, rc.right, 64);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(goldPen);

            // Decorative 5 Colored Crystals on Header
            draw_diamond_gem(hdc, 28, 32, 9, RGB(255, 59, 48), RGB(255, 235, 235));
            draw_diamond_gem(hdc, 50, 32, 9, RGB(0, 140, 255), RGB(235, 245, 255));
            draw_diamond_gem(hdc, 470, 32, 9, RGB(52, 199, 89), RGB(235, 255, 235));
            draw_diamond_gem(hdc, 492, 32, 9, RGB(175, 82, 222), RGB(245, 235, 255));

            SetBkMode(hdc, TRANSPARENT);
            HFONT hTitleFont = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                           DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hLabelFont = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                           DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            HGDIOBJ oldFont = SelectObject(hdc, hTitleFont);
            SetTextColor(hdc, RGB(255, 220, 55));
            TextOutA(hdc, 74, 12, "CROC 2: KINGDOM OF THE GOBBOS", 29);

            SelectObject(hdc, hLabelFont);
            SetTextColor(hdc, RGB(165, 240, 190));
            TextOutA(hdc, 74, 37, "Archipelago Multiworld Connection & Mod Controller", 50);

            // Field Labels
            SetTextColor(hdc, RGB(255, 230, 120));
            TextOutA(hdc, 20, 125, "Server Address:", 15);
            TextOutA(hdc, 382, 125, "Port:", 5);
            TextOutA(hdc, 20, 159, "Slot Name (Case-Sens.):", 23);
            TextOutA(hdc, 20, 193, "Password (Optional):", 20);

            // Status & Tracker Box
            RECT trackRc = {20, 270, 500, 350};
            HBRUSH trackBrush = CreateSolidBrush(RGB(10, 34, 18));
            FillRect(hdc, &trackRc, trackBrush);
            DeleteObject(trackBrush);

            HPEN emeraldPen = CreatePen(PS_SOLID, 1, RGB(46, 204, 113));
            SelectObject(hdc, emeraldPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, trackRc.left, trackRc.top, trackRc.right, trackRc.bottom);
            DeleteObject(emeraldPen);

            std::string status_line;
            std::string tracker_line1;
            std::string tracker_line2;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                status_line = "Status: " + g_state.conn.status_text;
                std::ostringstream t1;
                t1 << "Abilities: "
                   << (g_state.abilities.tail_whack ? "[Tail:ON] " : "[Tail:--] ")
                   << (g_state.abilities.butt_stomp ? "[Stomp:ON] " : "[Stomp:--] ")
                   << (g_state.abilities.climbing_claws ? "[Climb:ON] " : "[Climb:--] ")
                   << (g_state.abilities.power_gloves ? "[Carry:ON] " : "[Carry:--] ")
                   << (g_state.abilities.lever_wrench ? "[Lever:ON]" : "[Lever:--]");
                tracker_line1 = t1.str();

                std::ostringstream t2;
                t2 << "Vehicles: " << (g_state.abilities.vehicle_license ? "ON" : "--")
                   << " | PlaneGun: " << (g_state.abilities.airplane_ammo_feed ? "ON" : "--")
                   << " | Jigsaws: " << g_state.jigsaw_pieces << "/4"
                   << " | Golden Gobbos: " << g_state.golden_gobbos << "/20"
                   << " | Trophies: " << g_state.trophies << "/16";
                tracker_line2 = t2.str();
            }

            SetTextColor(hdc, RGB(255, 225, 90));
            TextOutA(hdc, 30, 278, status_line.c_str(), static_cast<int>(status_line.size()));
            SetTextColor(hdc, RGB(190, 250, 205));
            TextOutA(hdc, 30, 302, tracker_line1.c_str(), static_cast<int>(tracker_line1.size()));
            TextOutA(hdc, 30, 324, tracker_line2.c_str(), static_cast<int>(tracker_line2.size()));

            SelectObject(hdc, oldFont);
            DeleteObject(hTitleFont);
            DeleteObject(hLabelFont);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static BOOL CALLBACK FindCrocWindowCallback(HWND hwnd, LPARAM lParam) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) &&
        hwnd != g_overlay_hwnd && hwnd != g_menu_hwnd && hwnd != g_toast_hwnd) {
        LONG style = GetWindowLongA(hwnd, GWL_STYLE);
        if ((style & WS_CAPTION) || (style & WS_POPUP)) {
            RECT rc;
            GetWindowRect(hwnd, &rc);
            if ((rc.right - rc.left) >= 320 && (rc.bottom - rc.top) >= 240) {
                *reinterpret_cast<HWND*>(lParam) = hwnd;
                return FALSE;
            }
        }
    }
    return TRUE;
}

static void ui_overlay_worker() {
    HINSTANCE hInst = GetModuleHandleA(nullptr);

    WNDCLASSEXA wcOverlay{};
    wcOverlay.cbSize = sizeof(WNDCLASSEXA);
    wcOverlay.style = CS_HREDRAW | CS_VREDRAW;
    wcOverlay.lpfnWndProc = OverlayWndProc;
    wcOverlay.hInstance = hInst;
    wcOverlay.hCursor = LoadCursor(nullptr, IDC_HAND);
    wcOverlay.lpszClassName = "Croc2APOverlayBarClass";
    RegisterClassExA(&wcOverlay);

    WNDCLASSEXA wcToast{};
    wcToast.cbSize = sizeof(WNDCLASSEXA);
    wcToast.style = CS_HREDRAW | CS_VREDRAW;
    wcToast.lpfnWndProc = ToastWndProc;
    wcToast.hInstance = hInst;
    wcToast.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcToast.lpszClassName = "Croc2APToastWndClass";
    RegisterClassExA(&wcToast);

    WNDCLASSEXA wcMenu{};
    wcMenu.cbSize = sizeof(WNDCLASSEXA);
    wcMenu.style = CS_HREDRAW | CS_VREDRAW;
    wcMenu.lpfnWndProc = MenuWndProc;
    wcMenu.hInstance = hInst;
    wcMenu.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcMenu.hbrBackground = CreateSolidBrush(RGB(16, 48, 26));
    wcMenu.lpszClassName = "Croc2APMenuWndClass";
    RegisterClassExA(&wcMenu);

    // Initial overlay is collapsed into a small indicator badge (220x28)
    g_overlay_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        "Croc2APOverlayBarClass",
        "Croc 2 Archipelago Overlay",
        WS_POPUP | WS_VISIBLE,
        24, 36, 220, 28,
        nullptr, nullptr, hInst, nullptr
    );
    SetLayeredWindowAttributes(g_overlay_hwnd, 0, 240, LWA_ALPHA);
    ShowWindow(g_overlay_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(g_overlay_hwnd);

    // Bottom-screen toast banner window (layered, click-through transparent)
    g_toast_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        "Croc2APToastWndClass",
        "Croc 2 AP Notification Toast",
        WS_POPUP,
        0, 0, 620, 46,
        nullptr, nullptr, hInst, nullptr
    );
    if (g_toast_hwnd) {
        SetLayeredWindowAttributes(g_toast_hwnd, 0, 235, LWA_ALPHA);
        ShowWindow(g_toast_hwnd, SW_HIDE);
    }

    int scr_w = GetSystemMetrics(SM_CXSCREEN);
    int scr_h = GetSystemMetrics(SM_CYSCREEN);
    g_menu_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        "Croc2APMenuWndClass",
        "Croc 2 Archipelago — Server & Mod Settings (F9)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        (scr_w - 526) / 2, (scr_h - 530) / 2, 526, 530,
        nullptr, nullptr, hInst, nullptr
    );

    refresh_menu_ui();

    bool f10_was_down = false;
    bool f9_was_down = false;
    DWORD last_tick = GetTickCount();

    while (g_ui_running.load()) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (!IsDialogMessageA(g_menu_hwnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }

        // Poll F10 (Toggle Overlay Visibility, or Shift+F10 to Toggle Vanilla) and F9 (Menu)
        int vk_f10 = 0x79;
        int vk_f9 = 0x78;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            vk_f10 = g_state.toggle_hotkey_vk;
            vk_f9 = g_state.menu_hotkey_vk;
        }

        bool f10_down = (GetAsyncKeyState(vk_f10) & 0x8000) != 0;
        bool shift_down = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        if (f10_down && !f10_was_down) {
            if (shift_down) {
                bool is_on = false;
                bool locked = false;
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    locked = g_state.save_locked_to_mod;
                    if (locked) {
                        g_state.add_log("Mod is LOCKED to this Randomizer Save (Seed #" + g_state.current_seed_hex + ")! Return to Title Screen or delete save to toggle.");
                    } else {
                        g_state.manual_override_disabled = !g_state.manual_override_disabled;
                        is_on = g_state.is_mod_enabled_toggle();
                        g_state.add_log(g_state.get_hud_banner());
                        g_state.pending_events_json.push_back(
                            "{\"event\":\"mod_toggled\",\"active\":" +
                            std::string(g_state.is_randomizer_active() ? "true" : "false") + "}"
                        );
                    }
                }
                if (locked) {
                    Beep(300, 140);
                } else {
                    Beep(is_on ? 880 : 440, 85);
                }
                refresh_menu_ui();
            } else {
                // F10 toggles overlay visibility (minimal indicator badge <-> full console)
                bool full_vis = false;
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_state.overlay_full_visible = !g_state.overlay_full_visible;
                    full_vis = g_state.overlay_full_visible;
                }
                Beep(full_vis ? 750 : 500, 60);
                if (g_overlay_hwnd) {
                    InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
                }
            }
        }
        f10_was_down = f10_down;

        bool f9_down = (GetAsyncKeyState(vk_f9) & 0x8000) != 0;
        if (f9_down && !f9_was_down) {
            g_menu_visible = !g_menu_visible;
            ShowWindow(g_menu_hwnd, g_menu_visible ? SW_SHOW : SW_HIDE);
            if (g_menu_visible) {
                SetWindowPos(g_menu_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                SetForegroundWindow(g_menu_hwnd);
                refresh_menu_ui();
            }
            Beep(660, 60);
        }
        f9_was_down = f9_down;

        // Advance Bottom-Screen Toast queue and timer
        bool toast_show = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            DWORD now_t = GetTickCount();
            if (g_state.toast_active) {
                if (now_t >= g_state.active_toast.expire_tick) {
                    if (!g_state.toast_queue.empty()) {
                        g_state.active_toast = g_state.toast_queue.front();
                        g_state.toast_queue.pop_front();
                        g_state.active_toast.expire_tick = now_t + 3500;
                        toast_show = true;
                    } else {
                        g_state.toast_active = false;
                        toast_show = false;
                    }
                } else {
                    toast_show = true;
                }
            }
        }
        if (g_toast_hwnd) {
            if (toast_show) {
                if (!IsWindowVisible(g_toast_hwnd)) {
                    ShowWindow(g_toast_hwnd, SW_SHOWNOACTIVATE);
                }
                InvalidateRect(g_toast_hwnd, nullptr, FALSE);
            } else {
                if (IsWindowVisible(g_toast_hwnd)) {
                    ShowWindow(g_toast_hwnd, SW_HIDE);
                }
            }
        }

        // Anchor overlay & toast to Croc 2 game window every 250ms
        DWORD now = GetTickCount();
        if (now - last_tick >= 250) {
            float dt = static_cast<float>(now - last_tick) / 1000.0f;
            last_tick = now;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                step_internal_locked(dt);
            }

            HWND game_hwnd = nullptr;
            EnumWindows(FindCrocWindowCallback, reinterpret_cast<LPARAM>(&game_hwnd));
            if (game_hwnd) {
                RECT grc;
                if (GetWindowRect(game_hwnd, &grc)) {
                    bool full_vis = false;
                    {
                        std::lock_guard<std::mutex> lock(g_mutex);
                        full_vis = g_state.overlay_full_visible;
                    }
                    int ow = full_vis ? 600 : 220;
                    int oh = full_vis ? 184 : 28;
                    if (g_overlay_hwnd) {
                        SetWindowPos(
                            g_overlay_hwnd, HWND_TOPMOST,
                            grc.left + 18, grc.top + 38, ow, oh,
                            SWP_NOACTIVATE | SWP_SHOWWINDOW
                        );
                    }
                    if (g_toast_hwnd) {
                        int gw = grc.right - grc.left;
                        int tw = 620;
                        if (tw > gw - 32) tw = gw - 32;
                        if (tw < 300) tw = 300;
                        int th = 46;
                        int tx = grc.left + (gw - tw) / 2;
                        int ty = grc.bottom - th - 24;
                        SetWindowPos(g_toast_hwnd, HWND_TOPMOST, tx, ty, tw, th, SWP_NOACTIVATE);
                    }
                }
            }
            if (g_overlay_hwnd) {
                InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
            }
        }

        Sleep(25);
    }

    if (g_toast_hwnd) {
        DestroyWindow(g_toast_hwnd);
        g_toast_hwnd = nullptr;
    }
    if (g_menu_hwnd) {
        DestroyWindow(g_menu_hwnd);
        g_menu_hwnd = nullptr;
    }
    if (g_overlay_hwnd) {
        DestroyWindow(g_overlay_hwnd);
        g_overlay_hwnd = nullptr;
    }
}

static void tcp_server_worker() {
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return;
    }

    g_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_socket == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    int opt = 1;
    setsockopt(g_listen_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(g_state.tcp_port);

    if (bind(g_listen_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    if (listen(g_listen_socket, 4) == SOCKET_ERROR) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    while (g_running.load()) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(g_listen_socket, &read_set);
        timeval tv{0, 50000};

        int sel = select(0, &read_set, nullptr, nullptr, &tv);
        if (!g_running.load()) break;

        if (sel > 0 && FD_ISSET(g_listen_socket, &read_set)) {
            SOCKET client_sock = accept(g_listen_socket, nullptr, nullptr);
            if (client_sock != INVALID_SOCKET) {
                std::string buffer;
                char chunk[2048];
                while (g_running.load()) {
                    int n = recv(client_sock, chunk, sizeof(chunk) - 1, 0);
                    if (n <= 0) break;
                    chunk[n] = '\0';
                    buffer.append(chunk, n);
                    size_t nl;
                    while ((nl = buffer.find('\n')) != std::string::npos) {
                        std::string line = buffer.substr(0, nl);
                        buffer.erase(0, nl + 1);
                        std::string resp;
                        {
                            std::lock_guard<std::mutex> lock(g_mutex);
                            resp = handle_command_locked(line) + "\n";
                        }
                        if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
                        send(client_sock, resp.c_str(), static_cast<int>(resp.size()), 0);
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    g_state.client_connected = false;
                }
                if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
                closesocket(client_sock);
            }
        }
    }

    if (g_listen_socket != INVALID_SOCKET) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
    }
    WSACleanup();
}

// ============================================================================
// Live x64 Function Detours & Memory Hooks for croc2.exe
// ============================================================================
using FnGetStageSaveIndex = int32_t(__fastcall*)(int32_t tribe, int32_t level, int32_t submap, int32_t map_type);

static void write_abs_jmp64(uint8_t* dst, const void* target, size_t total_len) {
    // 14-byte absolute 64-bit jump: FF 25 00 00 00 00 <8-byte target address>
    dst[0] = 0xFF;
    dst[1] = 0x25;
    dst[2] = 0x00;
    dst[3] = 0x00;
    dst[4] = 0x00;
    dst[5] = 0x00;
    uint64_t addr = reinterpret_cast<uint64_t>(target);
    std::memcpy(dst + 6, &addr, 8);
    for (size_t i = 14; i < total_len; ++i) {
        dst[i] = 0x90; // NOP pad remaining instruction bytes
    }
}

static void inspect_and_sync_save_lifecycle_locked(const MapCoords& curr) {
    if (!g_exe_base) return;

    // Title Screen / Main Menu is T1I1M001 (tribe == 1, level == 1, submap == 1, map_type == 3)
    const bool is_title_screen = (curr.map_type == 3 && curr.tribe == 1 && curr.level == 1);
    if (is_title_screen) {
        if (g_state.in_active_save || g_state.save_locked_to_mod) {
            g_state.in_active_save = false;
            g_state.save_locked_to_mod = false;
            g_in_redirected_stage = false;
            g_state.add_log("Returned to Title Screen — Mod Toggle (F10) is now unlocked.");
            if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
            if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
        }
        return;
    }

    int32_t slot_idx = *reinterpret_cast<int32_t*>(g_exe_base + RVA_ACTIVE_SAVE_SLOT);
    if (slot_idx >= 0 && slot_idx < 5) {
        g_state.active_save_slot = slot_idx;
    }

    auto* footer = reinterpret_cast<Croc2RandoSaveFooter*>(
        g_exe_base + RVA_SAVE_DATA_TABLE + RVA_RANDO_SAVE_FOOTER_OFF
    );

    if (footer->magic == RANDO_SAVE_MAGIC && footer->is_rando_save == 1) {
        // Existing Randomizer Save loaded or active in memory!
        if (!g_state.in_active_save || !g_state.save_locked_to_mod || g_state.current_save_seed != footer->seed) {
            restore_state_from_save_footer_locked(*footer);
            if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
            if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
        }
    } else if (footer->magic == VANILLA_SAVE_MAGIC) {
        // Existing Vanilla Save loaded in memory!
        if (!g_state.in_active_save) {
            g_state.in_active_save = true;
            g_state.save_locked_to_mod = false;
            g_state.manual_override_disabled = true; // Keep vanilla save in Vanilla Mode
            g_state.add_log("Loaded Vanilla Save (Slot " + std::to_string(g_state.active_save_slot + 1) + ") — Mod Disabled.");
            if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
            if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
        }
    } else {
        // Freshly initialized save slot (footer->magic == 0 because croc2.exe copied RVA_CLEAN_SAVE_TEMPLATE)!
        g_state.in_active_save = true;
        if (g_state.is_mod_enabled_toggle()) {
            g_state.save_locked_to_mod = true;
            uint64_t fresh_seed = generate_fresh_entropy_seed();
            generate_new_save_seed_locked(fresh_seed);
            g_state.add_log("NEW RANDOMIZER SAVE (Slot " + std::to_string(g_state.active_save_slot + 1) +
                            ", Seed #" + g_state.current_seed_hex + ")! Mod Locked ON for this save!");
        } else {
            g_state.save_locked_to_mod = false;
            sync_state_to_save_footer_locked();
            g_state.add_log("New Vanilla Save created (Slot " + std::to_string(g_state.active_save_slot + 1) + ").");
        }
        if (g_menu_hwnd) PostMessageA(g_menu_hwnd, WM_APP + 1, 0, 0);
        if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
    }
}

static bool handle_map_transition_redirect() {
    if (!g_exe_base) return true;

    auto* curr = reinterpret_cast<MapCoords*>(g_exe_base + RVA_CURR_MAP_TRIBE);
    auto* prev = reinterpret_cast<MapCoords*>(g_exe_base + RVA_PREV_MAP_TRIBE);

    std::lock_guard<std::mutex> lock(g_mutex);

    inspect_and_sync_save_lifecycle_locked(*curr);

    const bool mod_on = g_state.is_mod_enabled_toggle();

    // Check if leaving a Village Hub (level == 1, map_type == 0, submap 1..3) into a Stage
    const bool from_hub = (prev->tribe >= 1 && prev->tribe <= 4 &&
                           prev->level == 1 && prev->map_type == 0 &&
                           prev->submap >= 1 && prev->submap <= 3);
    const bool to_stage = ((curr->map_type == 0 && curr->level >= 2) ||
                           curr->map_type == 1 || curr->map_type == 2);
    const bool to_hub   = (curr->tribe >= 1 && curr->tribe <= 4 &&
                           curr->level == 1 && curr->map_type == 0 &&
                           curr->submap >= 1 && curr->submap <= 3);

    if (!mod_on) {
        g_in_redirected_stage = false;
        g_state.current_wad = coords_to_wad(*curr);
        return true;
    }

    if (from_hub && to_stage) {
        std::string orig_wad = coords_to_wad(*curr);
        int orig_idx = find_stage_index_by_wad(orig_wad);

        // Check if level access is locked and Croc does not have the key (Sailor Village Door 1 is open from start)
        if (g_state.lock_level_access && orig_idx > 0 && g_state.is_randomizer_active()) {
            bool has_key = (g_state.unlocked_doors.count(orig_wad) > 0) ||
                           (g_state.unlocked_doors.count(kStageDoors[orig_idx].door_id) > 0);
            if (!has_key) {
                *curr = *prev; // Block transition
                static DWORD s_last_door_locked_toast = 0;
                DWORD now = GetTickCount();
                if (now - s_last_door_locked_toast > 2500) {
                    s_last_door_locked_toast = now;
                    std::string level_disp = (orig_idx >= 0) ? kStageDoors[orig_idx].level_name : orig_wad;
                    trigger_bottom_toast_locked(false, level_disp + " Key Required!", ItemImportance::TRAP, "Padlock", "Croc", "Stage door is locked!");
                    g_state.add_log("Door Locked: " + level_disp + " (Requires " + level_disp + " Key from checks!)");
                }
                return false;
            }
        }

        auto it = g_state.door_overrides.find(orig_wad);
        if (it != g_state.door_overrides.end() && !it->second.empty()) {
            MapCoords target_coords{};
            if (wad_to_coords(it->second, target_coords)) {
                g_saved_hub = *prev;
                g_saved_orig_door = *curr;
                g_saved_rand_stage = target_coords;
                g_in_redirected_stage = true;

                *curr = target_coords;
                g_state.current_wad = it->second;
                int target_idx = find_stage_index_by_wad(it->second);
                std::string orig_name = (orig_idx >= 0) ? kStageDoors[orig_idx].level_name : orig_wad;
                std::string target_name = (target_idx >= 0) ? kStageDoors[target_idx].level_name : it->second;
                g_state.last_door_redirect_desc =
                    orig_name + " -> " + target_name + " (Seed #" + g_state.current_seed_hex + ")";
                g_state.add_log("Door Shuffle: " + orig_name + " -> " + target_name);
                if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
                return true;
            }
        }
        g_in_redirected_stage = false;
        g_state.current_wad = orig_wad;
        return true;
    } else if (g_in_redirected_stage && to_hub) {
        // Sync completion flag from the randomized stage to the original Hub door slot
        auto fn_get_save_idx = reinterpret_cast<FnGetStageSaveIndex>(g_exe_base + RVA_FN_CALC_SAVE_INDEX);
        int32_t rand_idx = fn_get_save_idx(
            g_saved_rand_stage.tribe, g_saved_rand_stage.level,
            g_saved_rand_stage.submap, g_saved_rand_stage.map_type
        );
        int32_t orig_idx = fn_get_save_idx(
            g_saved_orig_door.tribe, g_saved_orig_door.level,
            g_saved_orig_door.submap, g_saved_orig_door.map_type
        );
        if (rand_idx >= 0 && orig_idx >= 0) {
            uint8_t* save_base = reinterpret_cast<uint8_t*>(g_exe_base + RVA_SAVE_DATA_TABLE);
            if ((save_base[rand_idx + 0x304] & 1) != 0) {
                save_base[orig_idx + 0x304] |= 1;
                int stage_i = find_stage_index_by_wad(coords_to_wad(g_saved_rand_stage));
                if (stage_i >= 0) {
                    claim_solo_check_locked(stage_i, 8, std::string(kStageDoors[stage_i].level_name) + " - Stage Clear");
                }
            }
        }

        // Return Croc to the Village Hub right outside the door he originally entered
        *curr = g_saved_hub;
        *prev = g_saved_orig_door;
        g_in_redirected_stage = false;
        g_state.current_wad = coords_to_wad(*curr);
        sync_state_to_save_footer_locked();
        g_state.add_log("Returned from shuffled stage to Hub (" + g_state.current_wad + ")");
        if (g_overlay_hwnd) InvalidateRect(g_overlay_hwnd, nullptr, FALSE);
        return true;
    } else {
        g_state.current_wad = coords_to_wad(*curr);
        return true;
    }
}

static void fixup_spawn_trigger_after_map_load(void* map_header) {
    if (!g_exe_base || !map_header) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_state.is_mod_enabled_toggle()) return;

    auto* prev = reinterpret_cast<MapCoords*>(g_exe_base + RVA_PREV_MAP_TRIBE);
    if (prev->tribe == 0) return;

    uintptr_t wad_buf = *reinterpret_cast<uintptr_t*>(g_exe_base + RVA_WAD_BUFFER_BASE);
    if (!wad_buf) return;

    uintptr_t hdr = reinterpret_cast<uintptr_t>(map_header);
    uint16_t trig_count = *reinterpret_cast<uint16_t*>(hdr + 0x18);
    uint32_t trig_offset = *reinterpret_cast<uint32_t*>(hdr + 0x1C);
    if (trig_count == 0) return;

    uintptr_t trig_base = wad_buf + trig_offset;

    // 1. Check if any door trigger already matches *prev (as 0x1400628CD expects)
    for (uint16_t i = 0; i < trig_count; ++i) {
        uintptr_t t = trig_base + static_cast<uintptr_t>(i) * 0x54;
        uint8_t flags = *reinterpret_cast<uint8_t*>(t + 0x1E);
        if ((flags & 4) == 0) continue;

        int32_t dt = *reinterpret_cast<int32_t*>(t + 0x0C) & 0xFFF;
        int32_t dl = *reinterpret_cast<int32_t*>(t + 0x10) & 0xFFF;
        int32_t ds = *reinterpret_cast<int32_t*>(t + 0x14) & 0xFFF;
        int32_t dm = *reinterpret_cast<int32_t*>(t + 0x18);

        if (dt == prev->tribe && dl == prev->level && ds == prev->submap && dm == prev->map_type) {
            return; // Exact match already exists!
        }
    }

    // 2. No trigger matched *prev (because we redirected across doors/villages).
    // Find the stage's entrance door trigger (pointing back to a Hub: level == 1, map_type == 0)
    // or fallback to the first door trigger with (flags & 4) != 0, and align *prev to it!
    uintptr_t best_trig = 0;
    for (uint16_t i = 0; i < trig_count; ++i) {
        uintptr_t t = trig_base + static_cast<uintptr_t>(i) * 0x54;
        uint8_t flags = *reinterpret_cast<uint8_t*>(t + 0x1E);
        if ((flags & 4) == 0) continue;

        if (!best_trig) best_trig = t;
        int32_t dl = *reinterpret_cast<int32_t*>(t + 0x10) & 0xFFF;
        int32_t dm = *reinterpret_cast<int32_t*>(t + 0x18);
        if (dl == 1 && dm == 0) {
            best_trig = t;
            break;
        }
    }

    if (best_trig) {
        prev->tribe    = *reinterpret_cast<int32_t*>(best_trig + 0x0C) & 0xFFF;
        prev->level    = *reinterpret_cast<int32_t*>(best_trig + 0x10) & 0xFFF;
        prev->submap   = *reinterpret_cast<int32_t*>(best_trig + 0x14) & 0xFFF;
        prev->map_type = *reinterpret_cast<int32_t*>(best_trig + 0x18);
    }
}

static void __fastcall Hooked_OnDoorTransition() {
    if (!handle_map_transition_redirect()) {
        g_transition_already_redirected = true;
        return; // Locked door! Do not initiate transition
    }
    g_transition_already_redirected = true;
    g_orig_on_door_transition();
}

static void* __fastcall Hooked_LoadMap(int32_t tribe, int32_t level, int32_t submap, int32_t map_type) {
    if (!g_transition_already_redirected) {
        if (!handle_map_transition_redirect()) {
            if (g_exe_base) {
                auto* prev = reinterpret_cast<MapCoords*>(g_exe_base + RVA_PREV_MAP_TRIBE);
                return g_orig_load_map(prev->tribe, prev->level, prev->submap, prev->map_type);
            }
        }
    }
    g_transition_already_redirected = false;

    if (g_exe_base) {
        auto* curr = reinterpret_cast<MapCoords*>(g_exe_base + RVA_CURR_MAP_TRIBE);
        tribe    = curr->tribe;
        level    = curr->level;
        submap   = curr->submap;
        map_type = curr->map_type;
    }

    void* map_header = g_orig_load_map(tribe, level, submap, map_type);
    fixup_spawn_trigger_after_map_load(map_header);
    return map_header;
}

static int64_t __fastcall Hooked_SaveGameSlot(int32_t slot_idx, void* save_table, void* ctx) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (slot_idx >= 0 && slot_idx < 5) {
            g_state.active_save_slot = slot_idx;
        }
        sync_state_to_save_footer_locked();
        if (g_state.save_locked_to_mod) {
            g_state.add_log("Saved Randomizer Progress (Slot " + std::to_string(slot_idx + 1) +
                            ", Seed #" + g_state.current_seed_hex + ")!");
        }
    }
    return g_orig_save_game_slot(slot_idx, save_table, ctx);
}

static void reset_pending_native_grants() {
    g_pending_heart_pots.store(0);
    g_pending_keys.store(0);
    g_pending_clockwork.store(0);
    g_pending_red_jellies.store(0);
    g_pending_blue_jellies.store(0);
    g_pending_green_jellies.store(0);
    g_pending_binoculars.store(0);
    g_pending_jigsaws.store(0);
    g_pending_golden_gobbos.store(0);
    g_pending_wallet_crystals.store(0);
    g_pending_colored_gems.store(0);
}

// Runs on croc2.exe's own game thread (from Hooked_ReadInput). Pushes every pending AP / Solo item
// into the REAL in-game inventory wheel + HUD using croc2.exe's native inventory manager, so the
// items visibly pop up and actually exist when the player opens their inventory.
static void drain_pending_native_inventory_grants() {
    if (!g_exe_base) return;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_state.is_mod_enabled_toggle() || !g_state.in_active_save) return;
    }

    auto* curr = reinterpret_cast<MapCoords*>(g_exe_base + RVA_CURR_MAP_TRIBE);
    if (curr->map_type == 3) return;                                                 // Title Screen / menus
    if (*reinterpret_cast<int32_t*>(g_exe_base + RVA_HEALTH_STRUCT) <= 0) return;    // Not in gameplay yet
    if (*reinterpret_cast<int32_t*>(g_exe_base + RVA_PAUSE_MENU_ACTIVE) != 0) return; // Wait until unpaused

    auto fn_add_item = reinterpret_cast<FnAddInventoryItem>(g_exe_base + RVA_FN_ADD_INVENTORY_ITEM);
    auto fn_rebuild  = reinterpret_cast<FnRebuildInvWheel>(g_exe_base + RVA_FN_REBUILD_INV_WHEEL);
    auto i32 = [](uintptr_t rva) -> int32_t& {
        return *reinterpret_cast<int32_t*>(g_exe_base + rva);
    };

    bool changed = false;
    // Grant at most one of each item type per frame so every HUD popup animation is visible.
    auto grant_one = [&](std::atomic<int>& pending, int32_t item_type) -> bool {
        int n = pending.load();
        if (n <= 0) return false;
        pending.fetch_sub(1);
        fn_add_item(item_type, 1, 1, 0, 0);
        changed = true;
        return true;
    };

    grant_one(g_pending_red_jellies, 5);    // Gummi Jump (Red Jelly)      -> 0x140BA9B48
    grant_one(g_pending_green_jellies, 6);  // High Jelly Jump (Green)     -> 0x140BA9B50
    grant_one(g_pending_blue_jellies, 7);   // Jelly Jump (Blue Jelly)     -> 0x140BA9B4C
    grant_one(g_pending_jigsaws, 10);       // Jigsaw Piece                -> 0x140BA9B60
    grant_one(g_pending_golden_gobbos, 2);  // Golden Gobbo / Trophy       -> 0x140BA9B40

    if (grant_one(g_pending_clockwork, 8)) { // Clockwork Gobbo (wheel + HUD counter)
        int32_t& hud = i32(RVA_HUD_CLOCKWORK_GOBBOS);
        if (hud < 9) hud += 1;
    }
    if (grant_one(g_pending_keys, 12)) {     // Key (wheel + HUD key counter)
        i32(RVA_HUD_KEYS) += 1;
    }
    if (grant_one(g_pending_binoculars, 4)) { // Binoculars (wheel + camera/HUD flag)
        i32(RVA_HUD_BINOCULARS) = 1;
        g_pending_binoculars.store(0);       // Binoculars are a single unlock
    }

    int crystals = g_pending_wallet_crystals.exchange(0);
    if (crystals > 0) {
        i32(RVA_WALLET_CRYSTALS) += crystals;
        changed = true;
    }
    g_pending_colored_gems.store(0); // Colored crystal counts are mirrored by the per-stage crystal sync above

    if (changed || g_need_full_inventory_sync.exchange(false)) {
        // Rebuild the 0x1C-byte slot array at 0x140BC4EB0, recompute [0x140BA9B64] slot count,
        // and set [0x140BC4E9C] = 2 so the inventory wheel can be opened immediately.
        fn_rebuild();
    }
}

static uint32_t __fastcall Hooked_ReadInput(void* input_ctx) {
    uint32_t buttons = g_orig_read_input(input_ctx);
    if (!g_exe_base) return buttons;

    uint32_t raw_buttons = buttons;
    uint32_t newly_pressed = raw_buttons & ~g_prev_raw_buttons;
    g_prev_raw_buttons = raw_buttons;

    int dmg_to_apply = 0;
    int pots_to_apply = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto* curr = reinterpret_cast<MapCoords*>(g_exe_base + RVA_CURR_MAP_TRIBE);
        inspect_and_sync_save_lifecycle_locked(*curr);

        if (!g_state.is_mod_enabled_toggle()) {
            return buttons;
        }



        // Only drain pending heart grants while actually in-game (never on the Title Screen),
        // so items received before a save is started/loaded are never silently lost.
        const bool in_game_for_hearts =
            (curr->map_type != 3) && (*reinterpret_cast<int32_t*>(g_exe_base + RVA_HEALTH_STRUCT) > 0);
        if (in_game_for_hearts) {
            dmg_to_apply = g_pending_heart_damage.exchange(0);
            pots_to_apply = g_pending_heart_pots.exchange(0);
        }

        // Live In-Stage Check Detection (Every individual White Crystal 1..100 & Colored Crystal pickups)
        int stage_idx = find_stage_index_by_wad(g_state.current_wad);
        if (stage_idx >= 0 && g_state.is_randomizer_active()) {
            int32_t level_crystals = *reinterpret_cast<int32_t*>(g_exe_base + RVA_LEVEL_CRYSTALS);
            int max_c = (level_crystals > 100) ? 100 : level_crystals;
            for (int c = 1; c <= max_c; ++c) {
                int c_zero = c - 1;
                if ((g_state.white_crystals_claimed[stage_idx][c_zero / 8] & (1u << (c_zero % 8))) == 0) {
                    claim_white_crystal_check_locked(stage_idx, c);
                }
            }
            if (level_crystals >= 10) {
                claim_solo_check_locked(stage_idx, 0, std::string(kStageDoors[stage_idx].level_name) + " - 10 White Crystals");
            }
            if (level_crystals >= 25) {
                claim_solo_check_locked(stage_idx, 1, std::string(kStageDoors[stage_idx].level_name) + " - 25 White Crystals");
            }
            if (level_crystals >= 40) {
                claim_solo_check_locked(stage_idx, 2, std::string(kStageDoors[stage_idx].level_name) + " - 40 White Crystals");
            }
        }

        // Sync Colored Crystal count for current stage -> activates Golden Gobbo at 5/5!
        uintptr_t crystal_ptr = *reinterpret_cast<uintptr_t*>(g_exe_base + RVA_COLORED_CRYSTAL_PTR);
        if (crystal_ptr != 0) {
            int32_t in_stage_val = *reinterpret_cast<int32_t*>(crystal_ptr);
            int in_stage_gems = in_stage_val / 0x1000;
            if (stage_idx >= 0 && g_state.is_randomizer_active()) {
                static const char* kGemColors[5] = {"Red", "Blue", "Green", "Yellow", "Purple"};
                for (int g = 0; g < in_stage_gems && g < 5; ++g) {
                    claim_solo_check_locked(
                        stage_idx, 3 + g,
                        std::string(kStageDoors[stage_idx].level_name) + " - " + kGemColors[g] + " Crystal"
                    );
                }
            }

            if (g_state.colored_crystals_mode == 1 && g_state.is_randomizer_active()) {
                std::string canonical_wad = (stage_idx >= 0)
                    ? std::string(kStageDoors[stage_idx].vanilla_wad)
                    : g_state.current_wad;
                int count = 0;
                auto it = g_state.stage_colored_crystals.find(canonical_wad);
                if (it != g_state.stage_colored_crystals.end()) {
                    uint8_t mask = it->second;
                    for (int b = 0; b < 5; ++b) {
                        if (mask & (1 << b)) ++count;
                    }
                }
                int32_t game_val = count * 0x1000;
                // Clamp HUD crystal display to the number of colored crystals ACTUALLY received for this stage!
                // This prevents picking up an in-level check from falsely filling the player's HUD crystal inventory.
                *reinterpret_cast<int32_t*>(crystal_ptr) = game_val;
            }
        }

        // Enforce Randomized Moveset Locks & Active Traps in ANY Randomizer Save (both Solo Seed & Archipelago)!
        int32_t paused = *reinterpret_cast<int32_t*>(g_exe_base + RVA_PAUSE_MENU_ACTIVE);
        if (g_state.is_randomizer_active() && paused == 0 && curr->map_type != 3) {
            bool has_tail_knot = false;
            bool has_lead_boots = false;
            bool has_reverse_controls = false;
            bool has_fire_knockback = false;

            if (g_state.fire_trap.active && g_state.fire_trap.timer > 1.2f) {
                has_fire_knockback = true;
            }

            for (const auto& trap : g_state.timed_traps) {
                if (trap.trap_type == "tail_knot") has_tail_knot = true;
                if (trap.trap_type == "lead_boots") has_lead_boots = true;
                if (trap.trap_type == "reverse_controls") has_reverse_controls = true;
            }

            if (has_fire_knockback) {
                // Apply knockback recoil: Croc reels backward, movement locked
                buttons &= ~0x00F0u; // Clear directional D-Pad
                buttons |= 0x0040u;  // Force Down/Backward stumble impulse
            }

            if (has_tail_knot) {
                buttons &= ~0x8000u; // Tail Whack disabled while tail is knotted (flat 10s, cannot be untied early)
            } else if (!g_state.abilities.tail_whack) {
                buttons &= ~0x8000u; // Lock Square (Tail Whack) until unlocked in seed
            }

            if (has_lead_boots) {
                buttons &= ~0x4000u; // Heavy boots disable jumping
            } else if (!g_state.abilities.butt_stomp && (newly_pressed & 0x4000u) != 0) {
                DWORD now_tick = GetTickCount();
                if (now_tick - g_last_jump_press_tick < 650) {
                    buttons &= ~0x4000u; // Lock mid-air double-X (Butt Stomp) while allowing normal ground Jump!
                } else {
                    g_last_jump_press_tick = now_tick;
                }
            }

            if (has_reverse_controls) {
                uint32_t dpad = buttons & 0x00F0u;
                uint32_t rev = 0;
                if (dpad & 0x0010u) rev |= 0x0040u; // Up -> Down
                if (dpad & 0x0040u) rev |= 0x0010u; // Down -> Up
                if (dpad & 0x0080u) rev |= 0x0020u; // Left -> Right
                if (dpad & 0x0020u) rev |= 0x0080u; // Right -> Left
                buttons = (buttons & ~0x00F0u) | rev;
            }

            if (!g_state.abilities.power_gloves && !g_state.abilities.lever_wrench) {
                buttons &= ~0x2000u; // Lock Circle (Carry / Lever Switch) until unlocked in seed
            }
        }
    }

    void* health_struct = reinterpret_cast<void*>(g_exe_base + RVA_HEALTH_STRUCT);
    if (*reinterpret_cast<int32_t*>(health_struct) > 0) {
        auto fn_dmg = reinterpret_cast<FnTakeHeartDamage>(g_exe_base + RVA_FN_TAKE_HEART_DAMAGE);
        auto fn_pot = reinterpret_cast<FnAddHeartPot>(g_exe_base + RVA_FN_ADD_HEART_POT);
        while (dmg_to_apply-- > 0) {
            fn_dmg(health_struct);
        }
        while (pots_to_apply-- > 0) {
            fn_pot(health_struct);
        }
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.max_heart_pots = *reinterpret_cast<int32_t*>(health_struct);
        g_state.current_hearts = *reinterpret_cast<int32_t*>(g_exe_base + RVA_HEALTH_STRUCT + 4);
    }

    drain_pending_native_inventory_grants();
    return buttons;
}

static void install_croc2_engine_hooks() {
    if (g_hooks_installed) return;
    g_exe_base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    if (!g_exe_base) return;

    // Verify expected croc2.exe prologues before patching
    uint8_t* fn_load_map  = reinterpret_cast<uint8_t*>(g_exe_base + RVA_FN_LOAD_MAP);
    uint8_t* fn_door_tr   = reinterpret_cast<uint8_t*>(g_exe_base + RVA_FN_ON_DOOR_TRANSITION);
    uint8_t* fn_read_in   = reinterpret_cast<uint8_t*>(g_exe_base + RVA_FN_READ_INPUT);
    uint8_t* fn_save_slot = reinterpret_cast<uint8_t*>(g_exe_base + RVA_FN_SAVE_GAME_SLOT);

    if (fn_load_map[0] != 0x55 || fn_load_map[1] != 0x41 || fn_door_tr[0] != 0x56) {
        return; // Not the target croc2.exe binary (e.g., running under python.exe test harness)
    }

    uint8_t* tramp_mem = reinterpret_cast<uint8_t*>(
        VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE)
    );
    if (!tramp_mem) return;

    // 1. Hook LoadMap (0x140070B60, 19-byte prologue)
    uint8_t* tramp_load_map = tramp_mem;
    std::memcpy(tramp_load_map, fn_load_map, 19);
    write_abs_jmp64(tramp_load_map + 19, fn_load_map + 19, 14);
    g_orig_load_map = reinterpret_cast<FnLoadMap>(tramp_load_map);

    DWORD old_prot = 0;
    VirtualProtect(fn_load_map, 32, PAGE_EXECUTE_READWRITE, &old_prot);
    write_abs_jmp64(fn_load_map, reinterpret_cast<const void*>(&Hooked_LoadMap), 19);
    VirtualProtect(fn_load_map, 32, old_prot, &old_prot);

    // 2. Hook OnDoorTransition (0x14002BA00, 8-byte prologue + 15-byte 0xCC padding before it)
    uint8_t* tramp_door = tramp_mem + 64;
    std::memcpy(tramp_door, fn_door_tr, 8);
    write_abs_jmp64(tramp_door + 8, fn_door_tr + 8, 14);
    g_orig_on_door_transition = reinterpret_cast<FnOnDoorTransition>(tramp_door);

    uint8_t* cc_cave = fn_door_tr - 15; // 0x14002B9F1..0x14002B9FF is 15 bytes of 0xCC padding
    VirtualProtect(cc_cave, 32, PAGE_EXECUTE_READWRITE, &old_prot);
    write_abs_jmp64(cc_cave, reinterpret_cast<const void*>(&Hooked_OnDoorTransition), 14);
    // 5-byte relative jmp from 0x14002BA00 to 0x14002B9F1 (offset = -15 - 5 = -20 = 0xFFFFFFEC)
    fn_door_tr[0] = 0xE9;
    fn_door_tr[1] = 0xEC;
    fn_door_tr[2] = 0xFF;
    fn_door_tr[3] = 0xFF;
    fn_door_tr[4] = 0xFF;
    fn_door_tr[5] = 0x90;
    fn_door_tr[6] = 0x90;
    fn_door_tr[7] = 0x90;
    VirtualProtect(cc_cave, 32, old_prot, &old_prot);

    // 3. Hook ReadInput (0x140040990, 21-byte prologue)
    uint8_t* tramp_input = tramp_mem + 128;
    std::memcpy(tramp_input, fn_read_in, 21);
    write_abs_jmp64(tramp_input + 21, fn_read_in + 21, 14);
    g_orig_read_input = reinterpret_cast<FnReadInput>(tramp_input);

    VirtualProtect(fn_read_in, 32, PAGE_EXECUTE_READWRITE, &old_prot);
    write_abs_jmp64(fn_read_in, reinterpret_cast<const void*>(&Hooked_ReadInput), 21);
    VirtualProtect(fn_read_in, 32, old_prot, &old_prot);

    // 4. Hook SaveGameSlot (0x1401DCE40, 21-byte prologue)
    if (fn_save_slot[0] == 0x55 && fn_save_slot[1] == 0x41) {
        uint8_t* tramp_save = tramp_mem + 192;
        std::memcpy(tramp_save, fn_save_slot, 21);
        write_abs_jmp64(tramp_save + 21, fn_save_slot + 21, 14);
        g_orig_save_game_slot = reinterpret_cast<FnSaveGameSlot>(tramp_save);

        VirtualProtect(fn_save_slot, 32, PAGE_EXECUTE_READWRITE, &old_prot);
        write_abs_jmp64(fn_save_slot, reinterpret_cast<const void*>(&Hooked_SaveGameSlot), 21);
        VirtualProtect(fn_save_slot, 32, old_prot, &old_prot);
    }

    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    g_hooks_installed = true;

    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.add_log("Installed live croc2.exe engine hooks (LoadMap, DoorTransition, Input, SaveSlot).");
}

} // namespace croc2_ap

// System version.dll proxy forwarding for zero-friction drop-in loading
static HMODULE g_real_version_dll = nullptr;

static void ensure_real_version_dll() {
    if (!g_real_version_dll) {
        char sys_dir[MAX_PATH] = {0};
        GetSystemDirectoryA(sys_dir, MAX_PATH);
        std::string full_path = std::string(sys_dir) + "\\version.dll";
        g_real_version_dll = LoadLibraryA(full_path.c_str());
    }
}

extern "C" {

__declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    ensure_real_version_dll();
    using Fn = BOOL(WINAPI*)(LPCSTR, DWORD, DWORD, LPVOID);
    auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(g_real_version_dll, "GetFileVersionInfoA")));
    return fn ? fn(lptstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

__declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    ensure_real_version_dll();
    using Fn = BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPVOID);
    auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(g_real_version_dll, "GetFileVersionInfoW")));
    return fn ? fn(lptstrFilename, dwHandle, dwLen, lpData) : FALSE;
}

__declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle) {
    ensure_real_version_dll();
    using Fn = DWORD(WINAPI*)(LPCSTR, LPDWORD);
    auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(g_real_version_dll, "GetFileVersionInfoSizeA")));
    return fn ? fn(lptstrFilename, lpdwHandle) : 0;
}

__declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    ensure_real_version_dll();
    using Fn = DWORD(WINAPI*)(LPCWSTR, LPDWORD);
    auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(g_real_version_dll, "GetFileVersionInfoSizeW")));
    return fn ? fn(lptstrFilename, lpdwHandle) : 0;
}

__declspec(dllexport) BOOL WINAPI Proxy_VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    ensure_real_version_dll();
    using Fn = BOOL(WINAPI*)(LPCVOID, LPCSTR, LPVOID*, PUINT);
    auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(g_real_version_dll, "VerQueryValueA")));
    return fn ? fn(pBlock, lpSubBlock, lplpBuffer, puLen) : FALSE;
}

__declspec(dllexport) BOOL WINAPI Proxy_VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen) {
    ensure_real_version_dll();
    using Fn = BOOL(WINAPI*)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
    auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(g_real_version_dll, "VerQueryValueW")));
    return fn ? fn(pBlock, lpSubBlock, lplpBuffer, puLen) : FALSE;
}

// Exported API for direct testing and runtime inspection
__declspec(dllexport) void __cdecl Croc2AP_Initialize(const char* ini_path) {
    {
        std::lock_guard<std::mutex> lock(croc2_ap::g_mutex);
        croc2_ap::g_state = croc2_ap::ModState{};
        croc2_ap::init_default_door_shuffle_locked();
        croc2_ap::load_config_ini(ini_path);
        croc2_ap::load_yaml_player_names();
        croc2_ap::check_cmdline_flags();
        croc2_ap::g_state.add_log("Croc 2 Archipelago Hook initialized. Press F9 for Server Menu, F10 to Toggle Vanilla.");
    }

    if (!croc2_ap::g_running.load() && !croc2_ap::g_state.forced_vanilla_cmdline) {
        croc2_ap::g_running.store(true);
        croc2_ap::g_server_thread = std::thread(croc2_ap::tcp_server_worker);
    }
}

__declspec(dllexport) void __cdecl Croc2AP_StartOverlayUI() {
    if (!croc2_ap::g_ui_running.load() && !croc2_ap::g_state.forced_vanilla_cmdline) {
        croc2_ap::g_ui_running.store(true);
        croc2_ap::g_ui_thread = std::thread(croc2_ap::ui_overlay_worker);
    }
}

__declspec(dllexport) void __cdecl Croc2AP_ShowConnectionMenu(int show) {
    for (int i = 0; i < 40 && !croc2_ap::g_menu_hwnd; ++i) {
        Sleep(25);
    }
    if (croc2_ap::g_menu_hwnd) {
        croc2_ap::g_menu_visible = (show != 0);
        ShowWindow(croc2_ap::g_menu_hwnd, croc2_ap::g_menu_visible ? SW_SHOW : SW_HIDE);
        if (croc2_ap::g_menu_visible) {
            SetWindowPos(croc2_ap::g_menu_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            SetForegroundWindow(croc2_ap::g_menu_hwnd);
            PostMessageA(croc2_ap::g_menu_hwnd, WM_APP + 1, 0, 0);
        }
    }
}

__declspec(dllexport) void __cdecl Croc2AP_Shutdown() {
    croc2_ap::stop_ap_websocket_connection();

    if (croc2_ap::g_ui_running.load()) {
        croc2_ap::g_ui_running.store(false);
        if (croc2_ap::g_ui_thread.joinable()) {
            croc2_ap::g_ui_thread.join();
        }
    }

    if (croc2_ap::g_running.load()) {
        croc2_ap::g_running.store(false);
        if (croc2_ap::g_listen_socket != INVALID_SOCKET) {
            closesocket(croc2_ap::g_listen_socket);
            croc2_ap::g_listen_socket = INVALID_SOCKET;
        }
        if (croc2_ap::g_server_thread.joinable()) {
            croc2_ap::g_server_thread.join();
        }
    }
}

__declspec(dllexport) int __cdecl Croc2AP_IsRandomizerActive() {
    std::lock_guard<std::mutex> lock(croc2_ap::g_mutex);
    return croc2_ap::g_state.is_randomizer_active() ? 1 : 0;
}

__declspec(dllexport) int __cdecl Croc2AP_ProcessCommandJson(const char* request_json, char* response_buf, int response_buf_len) {
    if (!request_json || !response_buf || response_buf_len <= 0) return -1;
    std::string resp;
    {
        std::lock_guard<std::mutex> lock(croc2_ap::g_mutex);
        resp = croc2_ap::handle_command_locked(request_json);
    }
    int copy_len = static_cast<int>(resp.size());
    if (copy_len >= response_buf_len) copy_len = response_buf_len - 1;
    std::memcpy(response_buf, resp.c_str(), copy_len);
    response_buf[copy_len] = '\0';
    return copy_len;
}

__declspec(dllexport) void __cdecl Croc2AP_StepTick(float dt_seconds) {
    std::lock_guard<std::mutex> lock(croc2_ap::g_mutex);
    croc2_ap::step_internal_locked(dt_seconds);
}

} // extern "C"

// Safe Win32 bootstrap thread launched outside DllMain Loader Lock
static DWORD WINAPI SafeBootstrapThread(LPVOID lpParam) {
    (void)lpParam;
    Sleep(350); // Wait for croc2.exe process initialization to finish
    Croc2AP_Initialize(nullptr);
    croc2_ap::install_croc2_engine_hooks();
    Croc2AP_StartOverlayUI();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        char exe_path[MAX_PATH] = {0};
        GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
        std::string exe_str(exe_path);
        std::transform(exe_str.begin(), exe_str.end(), exe_str.begin(), ::tolower);
        if (exe_str.find("croc2.exe") != std::string::npos) {
            HANDLE hThread = CreateThread(nullptr, 0, SafeBootstrapThread, nullptr, 0, nullptr);
            if (hThread) CloseHandle(hThread);
        }
    }
    return TRUE;
}
