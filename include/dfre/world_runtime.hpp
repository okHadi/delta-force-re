#pragma once
#include "dfre/actor_runtime.hpp"
#include "dfre/ai.hpp"
#include "dfre/assets.hpp"
#include "dfre/item_runtime.hpp"
#include "dfre/item_callbacks.hpp"
#include "dfre/items.hpp"
#include "dfre/menu.hpp"
#include "dfre/mission.hpp"
#include "dfre/resource_plan.hpp"
#include "dfre/resources.hpp"
#include "dfre/terrain.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dfre {

// Joined single-player mission initialization for Df.exe
// (sha256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff).
//
// Original order inside 0x403A60 (single-player branch):
//   0x444C50  placement slot 0: profile name, face id, side
//   0x449C70  BMS load: 4 group tables (0x449710 per record), 64 type slots
//             (0x449E62), People Names lookup from the campaign RTXT
//   0x402EC0  resource pass: definition handles (0x45D6F0), actor +0x10/+0x14/
//             +0x18/+0x74 writes, squad/powerup slot removals
//   0x403C93-0x403DD4  terrain height loops, groups 3, 0, 1, 2 (group 3 +0x10000)
//   0x440640  placement actors through 0x45D520/0x43F610 (needs 0x67F528)
//   0x44A110  live type counts (needs 0x67F528)
//   0x4829F0  local player actor scan and writes
//
// Native pointers become deterministic nonzero ids with exposed maps; 0 means
// null. Nothing here allocates native memory or guesses callback behavior.

inline constexpr std::uint32_t kModelHandleBase = 0x71000000u;      // 0xC80D20 records
inline constexpr std::uint32_t kCharacterHandleBase = 0x72000000u;  // 0x42CBB0 CHR registry
inline constexpr std::uint32_t kAnimationHandleBase = 0x73000000u;  // 0x42D200 ANM registry
inline constexpr std::uint32_t kCharacterInstanceBase = 0x74000000u; // 0x42CCE0 instances
inline constexpr std::uint32_t kPersonHandleBase = 0x75000000u;     // 0x794680 + slot*0xB4
inline constexpr std::uint32_t kActorHandleBase = 0x76000000u;      // person owner token

std::uint32_t person_handle(std::size_t slot) noexcept;
std::uint32_t actor_handle(std::size_t group, std::size_t slot) noexcept;
// Original PE table address for wire fields and source comparisons.
std::uint32_t mission_actor_source_address(std::size_t group,std::size_t slot);

// Original 0x407D54 filename: "dfcamp%02i.bin" with campaign_index + 1.
std::string original_campaign_text_file(std::uint32_t campaign_index);

// Archives contain a live member with this name (case-insensitive). The
// original skips People Names when the 0x503238 RTXT handle is null.
bool mission_asset_present(const AssetStore& assets, std::string_view name);

// One explicit placement slot write (slots 1..16 in single player).
struct MissionPlacementInput {
    std::size_t index = 0;            // 0..16
    std::uint32_t object_id = 0;      // +0x00
    std::string name;                 // +0x0C
    std::uint16_t field_9c = 0;       // +0x9C
    std::uint32_t argument1 = 0;      // +0xA0
    std::uint32_t argument2 = 0;      // +0xA4
    std::uint32_t team_code = 0;      // +0x1C4
    bool active = false;              // +0x18C
};

// Caller globals, kept explicit. Addresses are the original ones.
struct MissionInitConfig {
    std::uint32_t mode = 0;            // 0x8CF97C (class selector, 0x402EC0 prelude)
    std::uint32_t game_type = 0;       // 0x67F53C (0x43F610 bind switch)
    bool night = false;                // 0x8CF982 bit 0x10
    bool networked = true;             // 0x67F528 (single player runs as host)
    bool multiplayer = false;          // 0x67F520
    bool color32 = true;               // 0x67F4E4 -> 0x503138
    std::uint32_t rank = 1;            // 0x67F530
    std::uint32_t animation_counter = 0; // 0x4F821C
    std::uint32_t bind_counter = 0;    // 0x4F820C before the 0x440640 pass
    bool squad_pass = false;           // 0x67F549 bit 2
    bool powerup_pass = true;          // 0x402E70 gate (0x67F549 bit 3 clear)
    bool patch_pass = true;            // 0x488980
    bool scale_health = false;         // 0x67F549 bit 0
    std::string campaign_text_file;    // People Names RTXT; empty skips names
    // Placement slot 0 inputs (0x444C50 plus the 0x43FFB0 enable writes).
    std::uint32_t player_object_id = 0;  // 0x50133C
    std::string player_name;             // 0x67F420
    std::uint32_t player_face_id = 0;    // 0x67F430; factory argument 1
    std::uint32_t player_side = 0;       // 0x67F434; factory argument 2
    std::uint32_t player_team = 0;       // slot +0x1C4
    bool local_player_active = true;     // +0x18C; producer is 0x43F830 (not ported)
    std::uint32_t weapon_slot_value = 0; // C239C0[C239B8] for 0x4829F0
    std::vector<MissionPlacementInput> extra_placements;
};

// Registry bindings in original load order.
struct ModelBinding {
    std::string name;
    std::uint8_t bank = 0;      // palette bank passed to the loader
    std::uint32_t handle = 0;   // kModelHandleBase + registry index
};

struct AppliedActorRemoval {
    std::size_t group = 0;
    std::size_t slot = 0;
    std::uint32_t item_id = 0;
    std::string reason;         // "squad" (0x402E00) or "powerup" (0x402E70)
};

struct MissionRuntimeState {
    MissionData mission;

    // 0x4827A0 records with the 0x45D6F0 resource handles filled.
    std::vector<RuntimeItemDefinition> definitions;

    // Registry maps and bindings (deterministic ids).
    std::vector<ModelBinding> models;
    std::vector<std::string> missing_models;
    std::map<std::string, std::uint32_t> characters;
    std::map<std::string, std::uint32_t> animations;
    std::vector<std::uint32_t> character_instances; // item index per 0x42CCE0 call

    // 0x449710 group tables and the 0x402EC0 per-slot extra fields.
    std::array<std::vector<AiActorState>, 4> groups;
    std::array<std::vector<std::uint16_t>, 4> health; // +0x74 u16 writes
    std::vector<AppliedActorRemoval> removals;
    AiTypeSlotTable type_slots;  // 0x449E62 loader copy
    AiTypeSlotTable live_types;  // 0x44A110 refresh result

    // Group 3 table continued by 0x440640; seeded with the converted records.
    std::optional<RuntimeActorFactory> actors;
    std::size_t group3_count = 0;             // 0x8CFA18 after the pass
    std::uint32_t bind_counter = 0;           // 0x4F820C after the pass
    std::vector<ActorPlacement> placements;   // 0x6E9A90, 17 slots
    std::vector<ActorSpawn> spawns;           // creation order
    std::optional<LocalPlayerActor> local_player;
    std::vector<AiPersonState> people;        // one continued 0x794680 pool

    // Plan counters copied from 0x402EC0's call list.
    std::size_t resource_calls = 0;
    std::size_t resource_steps = 0;
    std::size_t null_lookups = 0;
    std::size_t skipped_null_slots = 0;
};

// True when a group 3 slot was written by the 0x440640 pass; the factory
// reuses rejected converted slots, so this is not just "past the count".
bool mission_actor_created(const MissionRuntimeState& state, std::size_t slot);

// Exact 0x114 image of one converted or created actor, including the
// 0x402EC0 +0x10/+0x14/+0x18 writes and the +0x74 health word. Group 3
// created slots read the 0x440640 table.
ActorImage mission_actor_image(const MissionRuntimeState& state, std::size_t group,
                               std::size_t slot);

// Core assembly from prepared data; no file access, deterministic.
MissionRuntimeState assemble_mission_runtime(const MissionRuntimeResources& resources,
                                             const ItemCatalog& items,
                                             const TerrainGeometry& terrain,
                                             const RtxtTable* people_names,
                                             const MissionInitConfig& config);
MissionRuntimeState assemble_mission_runtime(const MissionRuntimeResources& resources,
                                             const ItemCatalog& items,
                                             const TerrainGeometry& terrain,
                                             const RtxtTable* people_names,
                                             const MissionInitConfig& config,
                                             const ItemSoundResolver& sound);

// Load the mission and its terrain, then assemble. One empty cache per call.
MissionRuntimeState initialize_mission(const AssetStore& assets, std::string_view mission_file,
                                       const MissionInitConfig& config);

} // namespace dfre
