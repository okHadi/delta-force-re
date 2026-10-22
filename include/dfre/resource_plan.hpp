#pragma once
#include "dfre/items.hpp"
#include "dfre/mission.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dfre {

// Original ITEMS.DEF type words to runtime type numbers. The parser writes
// item+0x28 at 0x4822BA..0x48249E: person 3, vehicle 1, building 5,
// decoration 2, marker 4, object 6, effect 8. Unknown words return 0.
std::uint32_t original_item_type_number(std::string_view type) noexcept;

// Original 0x45D737 loader bank switch: types 1,2,3,5,6 -> banks 1,2,3,4,5.
// All other types use bank 0. color32 mirrors DAT_00503138 (DAT_0067F4E4 != 0).
std::uint8_t original_item_bank(std::uint32_t type_number, bool color32) noexcept;

// One stock weapon record from Df.exe 0x4F9934 (39 records of 0xA8).
struct WeaponItemIds {
    std::uint32_t player_item = 0; // Record +0x64, resolved to +0x94.
    std::uint32_t world_item = 0;  // Record +0x68, resolved to +0x98.
};
// The 39 stock records. Df.exe 0x4F9934, resolved by 0x4828C0 at 0x46BFB0.
const std::array<WeaponItemIds, 39>& original_weapon_items() noexcept;

enum class ResourceKind : std::uint8_t { model, husk, character, animation };
enum class ResourceStage : std::uint8_t { soldier, weapons, effects, group0, group1, group3, group2, projectiles };
inline constexpr std::size_t no_slot = static_cast<std::size_t>(-1);

// One file request in original call order.
struct ResourceStep {
    ResourceStage stage = ResourceStage::soldier;
    ResourceKind kind = ResourceKind::model;
    std::uint32_t item_id = 0;
    std::size_t item_index = 0;
    std::size_t slot = no_slot;
    std::string name;        // ITEMS.DEF name without the loader extension.
    std::uint8_t bank = 0;   // Bank passed for this request.
    bool first_load = false; // First request for this name in its registry.
};

// One original FUN_0045D6F0 call.
struct ResourceCall {
    ResourceStage stage = ResourceStage::soldier;
    std::size_t slot = no_slot;
    std::uint32_t item_id = 0; // Requested id, or 0 for a direct index call.
    std::size_t item_index = 0;
    bool found = false;        // False when the id fell back to index 0.
    bool chr_instance = false; // Group 3 0x42CCE0 instance after the load.
};

// Original FUN_0045DA30 slot removal. Removals clear slots; models stay loaded.
struct ResourceRemoval {
    ResourceStage stage = ResourceStage::group0;
    std::size_t slot = no_slot;
    std::uint32_t item_id = 0;
    std::uint32_t callee = 0x45DA30;
    const char* reason = "";
};

struct ResourceNote {
    ResourceStage stage = ResourceStage::group2;
    std::string text;
};

struct ResourcePlanOptions {
    bool color32 = true;      // DAT_00503138 from DAT_0067F4E4.
    bool multiplayer = false; // DAT_0067F520.
    bool squad_pass = false;  // DAT_0067F549 bit 2: group 3 squad removal.
    bool powerup_pass = true; // Group 0 powerup removal gate.
    bool patch_pass = true;   // 0x488980 post-load patch, no file loads.
};

struct ResourcePlanInput {
    const ItemCatalog* items = nullptr;
    const MissionData* mission = nullptr;
    // Null uses original_weapon_items().
    const std::vector<WeaponItemIds>* weapons = nullptr;
    std::uint32_t theme = 0; // BMS header +0x74; mission header wins when set.
    bool night = false;      // BMS header +0x7A bit 0x10.
    ResourcePlanOptions options{};
};

struct ResourcePlan {
    std::vector<ResourceCall> calls;
    std::vector<ResourceStep> steps;
    std::vector<ResourceRemoval> removals;
    std::vector<ResourceNote> notes;
    std::size_t null_lookups = 0;  // Id lookups that fell back to index 0.
    std::size_t skipped_nulls = 0; // Group slots skipped because index 0.
    // First requests per loader registry, in plan order.
    std::vector<const ResourceStep*> first_loads() const;
};

// BMS header fields the original copies to 0x8CF908.
std::uint32_t mission_theme(const MissionData& mission) noexcept;
bool mission_night(const MissionData& mission) noexcept;

// Ordered single-player resource plan for original 0x402EC0.
ResourcePlan build_resource_plan(const ResourcePlanInput& input);
// Append the separate 4633F0 item-load pass after 402EC0. Reuse its registries.
void append_projectile_resource_plan(ResourcePlan&,const ResourcePlanInput&);

} // namespace dfre
