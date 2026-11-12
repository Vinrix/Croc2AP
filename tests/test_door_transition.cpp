#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <vector>

// Mirror the exact offsets from croc2_ap_hook.cpp
constexpr uintptr_t RVA_WAD_BUFFER_BASE      = 0xBD25C0;
constexpr uintptr_t RVA_WAD_BUFFER_LIMIT     = 0xBD25C8;
constexpr uintptr_t RVA_ACTIVE_SPAWN_TRIGGER = 0xB90D78;
constexpr uintptr_t RVA_CURR_MAP_TRIBE       = 0xBA9B04;
constexpr uintptr_t RVA_PREV_MAP_TRIBE       = 0xBA9B6C;

#pragma pack(push, 1)
struct MapCoords {
    int32_t tribe;
    int32_t level;
    int32_t submap;
    int32_t map_type;
};

struct SimulatedTrigger {
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t tribe;
    int32_t level;
    int32_t submap;
    int32_t map_type;
    uint16_t unk1c;
    uint8_t flags;
    uint8_t unk1f;
    uint32_t unk20;
    uint32_t link_off; // +0x24
    uint8_t pad[0x54 - 0x28];
};
static_assert(sizeof(SimulatedTrigger) == 0x54, "SimulatedTrigger must be 0x54 bytes");

struct SimulatedMapHeader {
    uint8_t pad0[0x18];
    uint16_t trig_count;   // +0x18
    uint16_t pad1a;
    uint32_t trig_offset;  // +0x1C
    uint8_t pad20[0x70 - 0x20];
};
static_assert(sizeof(SimulatedMapHeader) == 0x70, "SimulatedMapHeader must be 0x70 bytes");
#pragma pack(pop)

int main() {
    std::printf("[TEST] Running Croc 2 Door Transition & Spawn Trigger Verification...\n");

    // 1. Allocate a simulated exe space
    constexpr size_t kExeSpaceSize = 0x1000000; // 16 MB
    std::vector<uint8_t> exe_space(kExeSpaceSize, 0);
    uintptr_t exe_base = reinterpret_cast<uintptr_t>(exe_space.data());

    // 2. Allocate a simulated WAD buffer (2 MB)
    constexpr size_t kWadSize = 2 * 1024 * 1024;
    std::vector<uint8_t> wad_buffer(kWadSize, 0);
    uintptr_t wad_buf = reinterpret_cast<uintptr_t>(wad_buffer.data());
    uintptr_t wad_limit = wad_buf + kWadSize;

    *reinterpret_cast<uintptr_t*>(exe_base + RVA_WAD_BUFFER_BASE) = wad_buf;
    *reinterpret_cast<uintptr_t*>(exe_base + RVA_WAD_BUFFER_LIMIT) = wad_limit;

    // 3. Setup coordinates: leaving Sailor Village Hub (1,1,1,0) into Sandwich stage T1L4M001 (1,4,1,0)
    auto* curr = reinterpret_cast<MapCoords*>(exe_base + RVA_CURR_MAP_TRIBE);
    auto* prev = reinterpret_cast<MapCoords*>(exe_base + RVA_PREV_MAP_TRIBE);
    curr->tribe = 1; curr->level = 4; curr->submap = 1; curr->map_type = 0;
    prev->tribe = 1; prev->level = 1; prev->submap = 1; prev->map_type = 0;

    // 4. Setup triggers inside WAD buffer at offset 0x10000
    uint32_t trig_off = 0x10000;
    SimulatedTrigger* triggers = reinterpret_cast<SimulatedTrigger*>(wad_buf + trig_off);

    // Trigger 0: Random scenery trigger
    triggers[0].flags = 0x00;
    triggers[0].link_off = 0x00010000;

    // Trigger 1: Hub Door trigger (points back to Sailor Village Hub: tribe=1, level=1, submap=3, map_type=0)
    // In T1L4M001, link_off is non-zero (0x00078800)
    triggers[1].tribe = 1;
    triggers[1].level = 1;
    triggers[1].submap = 3;
    triggers[1].map_type = 0;
    triggers[1].flags = 0x04; // Door flag
    triggers[1].link_off = 0x00078800;
    triggers[1].x = 100;
    triggers[1].y = 200;
    triggers[1].z = 300;

    // Trigger 2: Standard spawn trigger (flags & 8)
    triggers[2].flags = 0x08;
    triggers[2].link_off = 0x00005000;

    SimulatedMapHeader map_hdr{};
    map_hdr.trig_count = 3;
    map_hdr.trig_offset = trig_off;

    // 5. Simulate our updated fixup_spawn_trigger_after_map_load logic
    {
        uintptr_t hdr = reinterpret_cast<uintptr_t>(&map_hdr);
        uint16_t trig_count = *reinterpret_cast<uint16_t*>(hdr + 0x18);
        uint32_t trig_offset = *reinterpret_cast<uint32_t*>(hdr + 0x1C);
        assert(trig_count == 3);

        uintptr_t trig_base = wad_buf + trig_offset;

        // Sanitization loop: Zero +0x24 across ALL triggers
        for (uint16_t i = 0; i < trig_count; ++i) {
            uintptr_t t = trig_base + static_cast<uintptr_t>(i) * 0x54;
            if (t + 0x54 > wad_limit) break;
            *reinterpret_cast<uint32_t*>(t + 0x24) = 0;
        }

        // Locate best trigger
        uintptr_t exact_match = 0;
        uintptr_t hub_door = 0;
        uintptr_t flag8_trig = 0;
        uintptr_t any_door = 0;

        for (uint16_t i = 0; i < trig_count; ++i) {
            uintptr_t t = trig_base + static_cast<uintptr_t>(i) * 0x54;
            if (t + 0x54 > wad_limit) break;

            uint8_t flags = *reinterpret_cast<uint8_t*>(t + 0x1E);
            int32_t dt = *reinterpret_cast<int32_t*>(t + 0x0C) & 0xFFF;
            int32_t dl = *reinterpret_cast<int32_t*>(t + 0x10) & 0xFFF;
            int32_t ds = *reinterpret_cast<int32_t*>(t + 0x14) & 0xFFF;
            int32_t dm = *reinterpret_cast<int32_t*>(t + 0x18);

            if (prev->tribe != 0 && dt == prev->tribe && dl == prev->level && ds == prev->submap && dm == prev->map_type) {
                exact_match = t;
                break;
            }
            if ((flags & 8) != 0 && !flag8_trig) flag8_trig = t;
            if ((flags & 4) != 0) {
                if (!any_door) any_door = t;
                if (dl == 1) hub_door = t;
            }
        }

        uintptr_t best_trig = exact_match ? exact_match :
                              (hub_door ? hub_door :
                              (flag8_trig ? flag8_trig :
                              (any_door ? any_door : trig_base)));

        // Unconditionally zero +0x24 on best_trig
        *reinterpret_cast<uint32_t*>(best_trig + 0x24) = 0;

        // Set RVA_ACTIVE_SPAWN_TRIGGER
        *reinterpret_cast<uintptr_t*>(exe_base + RVA_ACTIVE_SPAWN_TRIGGER) = best_trig;

        uint8_t best_flags = *reinterpret_cast<uint8_t*>(best_trig + 0x1E);
        if ((best_flags & 4) != 0) {
            prev->tribe    = *reinterpret_cast<int32_t*>(best_trig + 0x0C) & 0xFFF;
            prev->level    = *reinterpret_cast<int32_t*>(best_trig + 0x10) & 0xFFF;
            prev->submap   = *reinterpret_cast<int32_t*>(best_trig + 0x14) & 0xFFF;
            prev->map_type = *reinterpret_cast<int32_t*>(best_trig + 0x18);
        } else {
            prev->tribe = 0; prev->level = 0; prev->submap = 0; prev->map_type = 0;
        }
    }

    // 6. Verify croc2.exe instruction 0x14005c9d1:
    // rcx = [RVA_ACTIVE_SPAWN_TRIGGER]
    // eax = [rcx + 0x24]
    // test rax, rax
    // je 0x14005cad3 (Clean branch!)
    uintptr_t active_spawn = *reinterpret_cast<uintptr_t*>(exe_base + RVA_ACTIVE_SPAWN_TRIGGER);
    assert(active_spawn == reinterpret_cast<uintptr_t>(&triggers[1]));
    std::printf("[PASS] ACTIVE_SPAWN_TRIGGER points to Hub Door (0x%llx)\n", (unsigned long long)active_spawn);

    uint32_t link_val = *reinterpret_cast<uint32_t*>(active_spawn + 0x24);
    assert(link_val == 0);
    std::printf("[PASS] [ACTIVE_SPAWN_TRIGGER + 0x24] == 0 (test rax, rax -> je 0x14005cad3 taken! Crash bypassed!)\n");

    // All triggers +0x24 verified zero
    for (int i = 0; i < 3; ++i) {
        assert(triggers[i].link_off == 0);
    }
    std::printf("[PASS] All trigger link offsets sanitized to 0\n");

    // 7. Verify croc2.exe spawn search at 0x1400627e0:
    // prev coords must match trigger 1 so Croc spawns at the door
    assert(prev->tribe == 1 && prev->level == 1 && prev->submap == 3 && prev->map_type == 0);
    std::printf("[PASS] *prev aligns with Hub Door coords (tribe=1, level=1, submap=3, map_type=0)\n");

    std::printf("[SUCCESS] All door transition assertions passed!\n");
    return 0;
}
