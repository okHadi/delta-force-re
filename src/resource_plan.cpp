#include "dfre/resource_plan.hpp"
#include <set>

namespace dfre {
namespace {

// Original ITEMS.DEF type words at 0x4822BA..0x48249E.
struct TypeWord { std::string_view word; std::uint32_t number; };
constexpr TypeWord type_words[] = {
    {"person", 3}, {"vehicle", 1}, {"building", 5}, {"decoration", 2},
    {"marker", 4}, {"object", 6}, {"effect", 8}};

std::string lower_name(std::string_view value) {
    std::string out(value);
    for (auto& c : out) if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return out;
}

// 0x402EC0 branches 0x402EDE/0x402EF4 (snow night), 0x4030B4/0x4030CA
// (snow day), 0x4030DD/0x4030F3 (night), 0x4030FD/0x403113 (day).
struct SoldierPair { std::uint32_t first, second; };
SoldierPair soldier_pair(bool snow, bool night) noexcept {
    if (snow && night) return {5046, 5047};
    if (snow) return {5010, 5011};
    if (night) return {5044, 5045};
    return {5003, 5004};
}

// 0x45DD40 after FUN_0045D6F0(0).
constexpr std::uint32_t effect_ids[] = {
    7016, 7015, 7013, 7014, 7001, 7002, 7003, 7004, 7005, 7006, 7007,
    7008, 7009, 7010, 7011, 7012, 4046, 4051, 4058, 4059, 4060, 4061,
    4062, 4064, 4065, 4066, 4067, 4068, 4069, 4070, 4075};

// 0x483CD0 after both weapon table passes: claymore motion, claymore radio,
// satchel.
constexpr std::uint32_t weapon_fixed_ids[] = {4014, 4040, 4020};

// 0x402E00 group 3 "Squad" placeholder ids.
constexpr std::uint32_t squad_ids[] = {5002, 5038, 5048, 5049};

bool has_powerup(const ItemDefinition& item) {
    for (const auto& field : item.fields) {
        if (field.key != "attrib:") continue;
        for (const auto& value : field.values)
            if (lower_name(value) == "powerup") return true;
    }
    return false;
}

// Original single-player filter: disabled when flags 0x10 or 0x80 are set.
bool enabled_sp(std::uint32_t flags) noexcept { return (flags & 0x90) == 0; }

class PlanBuilder {
public:
    explicit PlanBuilder(const ResourcePlanInput& input): input_(input), items_(*input.items) {
        settled_graphic_.resize(items_.entries().size());
        settled_husk_.resize(items_.entries().size());
    }

    ResourcePlan build() {
        soldiers();
        weapons();
        effects();
        groups();
        return std::move(plan_);
    }
    ResourcePlan projectile_pass(const ResourcePlan& prior) {
        for(const auto& step:prior.steps){
            if(step.item_index>=settled_graphic_.size())throw AssetError("Projectile resource plan uses another item catalog");
            switch(step.kind){
            case ResourceKind::model:settled_graphic_[step.item_index]=1;models_.insert(lower_name(step.name));break;
            case ResourceKind::husk:settled_husk_[step.item_index]=1;models_.insert(lower_name(step.name));break;
            case ResourceKind::character:characters_.insert(lower_name(step.name));break;
            case ResourceKind::animation:animations_.insert(lower_name(step.name));break;
            }
        }
        for(const auto id:{0xfa1u,0xfc1u,0xfcau})add_lookup(ResourceStage::projectiles,no_slot,id,false);
        return std::move(plan_);
    }

private:
    void soldiers() {
        const bool snow = input_.mission ? mission_theme(*input_.mission) == 2 : input_.theme == 2;
        const bool night = input_.mission ? mission_night(*input_.mission) : input_.night;
        const auto pair = soldier_pair(snow, night);
        add_lookup(ResourceStage::soldier, no_slot, pair.first, false);
        add_lookup(ResourceStage::soldier, no_slot, pair.second, false);
    }

    void weapons() {
        if (input_.weapons) {
            for (const auto& weapon : *input_.weapons)
                add_lookup(ResourceStage::weapons, no_slot, weapon.player_item, false);
            for (const auto& weapon : *input_.weapons)
                add_lookup(ResourceStage::weapons, no_slot, weapon.world_item, false);
        } else {
            for (const auto& weapon : original_weapon_items())
                add_lookup(ResourceStage::weapons, no_slot, weapon.player_item, false);
            for (const auto& weapon : original_weapon_items())
                add_lookup(ResourceStage::weapons, no_slot, weapon.world_item, false);
        }
        for (const auto id : weapon_fixed_ids)
            add_lookup(ResourceStage::weapons, no_slot, id, false);
    }

    void effects() {
        add_index(ResourceStage::effects, no_slot, 0);
        for (const auto id : effect_ids)
            add_lookup(ResourceStage::effects, no_slot, id, false);
    }

    void groups() {
        if (!input_.mission) return;
        run_group(ResourceStage::group0, 0, false);
        run_group(ResourceStage::group1, 1, false);
        run_group(ResourceStage::group3, 3, true);
        if (input_.options.squad_pass) squad_removals();
        if (input_.options.powerup_pass) powerup_removals();
        group2_notes();
    }

    void run_group(ResourceStage stage, std::size_t group, bool chr_instance) {
        const auto& objects = input_.mission->object_groups[group];
        for (std::size_t index = 0; index < objects.size(); ++index) {
            const auto& object = objects[index];
            if (!enabled_sp(object.flags)) continue;
            if (item_index(object.type_id) == 0) { ++plan_.skipped_nulls; continue; }
            add_lookup(stage, index, object.type_id, chr_instance);
        }
    }

    void squad_removals() {
        const auto& objects = input_.mission->object_groups[3];
        for (std::size_t index = 0; index < objects.size(); ++index) {
            const auto& object = objects[index];
            if (!enabled_sp(object.flags)) continue;
            const auto& item = resolved(object.type_id);
            if (!is_squad(item.id)) continue;
            plan_.removals.push_back({ResourceStage::group3, index, item.id, 0x45DA30, "squad"});
        }
    }

    void powerup_removals() {
        const auto& objects = input_.mission->object_groups[0];
        for (std::size_t index = 0; index < objects.size(); ++index) {
            const auto& object = objects[index];
            if (!enabled_sp(object.flags)) continue;
            const auto& item = resolved(object.type_id);
            if (!has_powerup(item)) continue;
            plan_.removals.push_back({ResourceStage::group0, index, item.id, 0x45DA30, "powerup"});
        }
    }

    void group2_notes() {
        if (input_.options.multiplayer)
            plan_.notes.push_back({ResourceStage::group2,
                "multiplayer flag set; plan keeps the single-player object filter and removal gates"});
        plan_.notes.push_back({ResourceStage::group2,
            "0x402EC0 does not preload group 2 objects; stock markers have no graphic or husk"});
        const auto& objects = input_.mission->object_groups[2];
        for (std::size_t index = 0; index < objects.size(); ++index) {
            const auto& object = objects[index];
            if (!enabled_sp(object.flags)) continue;
            const auto& item = resolved(object.type_id);
            if (item.value("graphic").empty() && item.value("husk").empty()) continue;
            plan_.notes.push_back({ResourceStage::group2,
                "group2 slot " + std::to_string(index) + " item " + std::to_string(item.id) +
                " has a graphic; 0x402EC0 does not preload group 2"});
        }
    }

    static bool is_squad(std::uint32_t id) noexcept {
        for (const auto candidate : squad_ids) if (id == candidate) return true;
        return false;
    }

    const ItemDefinition& resolved(std::uint32_t id) const {
        const auto* exact = items_.find_exact(id);
        return exact ? *exact : items_.entries().front();
    }

    std::size_t item_index(std::uint32_t id) const {
        const auto* exact = items_.find_exact(id);
        return exact ? std::size_t(exact - items_.entries().data()) : 0;
    }

    void add_lookup(ResourceStage stage, std::size_t slot, std::uint32_t id, bool chr_instance) {
        const auto* exact = items_.find_exact(id);
        if (!exact) ++plan_.null_lookups;
        add_call(stage, slot, id, item_index(id), exact != nullptr, chr_instance);
    }

    void add_index(ResourceStage stage, std::size_t slot, std::size_t index) {
        const auto& item = items_.entries()[index];
        add_call(stage, slot, item.id, index, true, false);
    }

    void add_call(ResourceStage stage, std::size_t slot, std::uint32_t id, std::size_t index,
                  bool found, bool chr_instance) {
        ResourceCall call;
        call.stage = stage;
        call.slot = slot;
        call.item_id = id;
        call.item_index = index;
        call.found = found;
        call.chr_instance = chr_instance;
        plan_.calls.push_back(call);
        load_item(stage, slot, items_.entries()[index], index);
    }

    void load_item(ResourceStage stage, std::size_t slot, const ItemDefinition& item,
                   std::size_t index) {
        const std::uint32_t type = original_item_type_number(item.type);
        const std::uint8_t bank = original_item_bank(type, input_.options.color32);
        const auto graphic = item.value("graphic");
        if (!graphic.empty() && !settled_graphic_[index]) {
            settled_graphic_[index] = 1;
            add_step(stage, ResourceKind::model, item, index, slot, graphic, bank, models_);
            if (type == 3) {
                const auto chr = item.value("chr_file");
                if (!chr.empty())
                    add_step(stage, ResourceKind::character, item, index, slot, chr, bank, characters_);
                const auto anim = item.value("anim_def");
                if (!anim.empty())
                    add_step(stage, ResourceKind::animation, item, index, slot, anim, bank, animations_);
            }
        }
        const auto husk = item.value("husk");
        if (!husk.empty() && !settled_husk_[index]) {
            settled_husk_[index] = 1;
            add_step(stage, ResourceKind::husk, item, index, slot, husk, bank, models_);
        }
    }

    void add_step(ResourceStage stage, ResourceKind kind, const ItemDefinition& item,
                  std::size_t index, std::size_t slot, std::string_view name, std::uint8_t bank,
                  std::set<std::string>& registry) {
        ResourceStep step;
        step.stage = stage;
        step.kind = kind;
        step.item_id = item.id;
        step.item_index = index;
        step.slot = slot;
        step.name = std::string(name);
        step.bank = bank;
        step.first_load = registry.insert(lower_name(name)).second;
        plan_.steps.push_back(std::move(step));
    }

    const ResourcePlanInput& input_;
    const ItemCatalog& items_;
    ResourcePlan plan_;
    std::vector<std::uint8_t> settled_graphic_, settled_husk_;
    std::set<std::string> models_, characters_, animations_;
};

} // namespace

std::uint32_t original_item_type_number(std::string_view type) noexcept {
    const auto word = lower_name(type);
    for (const auto& entry : type_words) if (word == entry.word) return entry.number;
    return 0;
}

std::uint8_t original_item_bank(std::uint32_t type_number, bool color32) noexcept {
    if (!color32) return 0;
    switch (type_number) {
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        case 5: return 4;
        case 6: return 5;
        default: return 0;
    }
}

const std::array<WeaponItemIds, 39>& original_weapon_items() noexcept {
    // Df.exe 0x4F9934 records, fields +0x64 and +0x68. Resolved to item
    // indices by 0x4828C0 during 0x46BFB0.
    static const std::array<WeaponItemIds, 39> stock = {{
        {0, 0}, {4005, 4004}, {0, 0}, {0, 0}, {4032, 4031}, {4007, 4006},
        {4057, 4056}, {4057, 4056}, {4003, 4002}, {4003, 4002}, {4057, 4056},
        {4003, 4002}, {4024, 4023}, {4024, 4023}, {4024, 4023}, {4028, 4027},
        {4003, 4002}, {4003, 4002}, {4003, 4002}, {4003, 4002}, {4030, 4029},
        {4030, 4029}, {4030, 4029}, {4003, 4002}, {4003, 4002}, {4003, 4002},
        {4003, 4002}, {4073, 4074}, {0, 4041}, {4020, 0}, {4071, 4072},
        {4003, 4002}, {4026, 4025}, {4037, 4033}, {4003, 4002}, {4003, 4022},
        {4003, 4022}, {4039, 0}, {4039, 0}}};
    return stock;
}

std::uint32_t mission_theme(const MissionData& mission) noexcept {
    const auto& header = mission.header;
    return std::uint32_t(header[0x74]) | (std::uint32_t(header[0x75]) << 8) |
           (std::uint32_t(header[0x76]) << 16) | (std::uint32_t(header[0x77]) << 24);
}

bool mission_night(const MissionData& mission) noexcept { return (mission.header[0x7A] & 0x10) != 0; }

std::vector<const ResourceStep*> ResourcePlan::first_loads() const {
    std::vector<const ResourceStep*> result;
    for (const auto& step : steps) if (step.first_load) result.push_back(&step);
    return result;
}

ResourcePlan build_resource_plan(const ResourcePlanInput& input) {
    if (!input.items) throw AssetError("Resource plan needs an item catalog");
    return PlanBuilder(input).build();
}
void append_projectile_resource_plan(ResourcePlan& plan,const ResourcePlanInput& input){
    if(!input.items)throw AssetError("Projectile resource plan needs an item catalog");
    auto next=PlanBuilder(input).projectile_pass(plan);
    plan.calls.insert(plan.calls.end(),next.calls.begin(),next.calls.end());
    plan.steps.insert(plan.steps.end(),next.steps.begin(),next.steps.end());
    plan.null_lookups+=next.null_lookups;
}

} // namespace dfre
