#pragma once
#include "dfre/items.hpp"
#include "dfre/mission.hpp"
#include "dfre/resource_plan.hpp"
#include "dfre/models.hpp"
#include <map>
namespace dfre {
struct ObjectAppearance {
    std::uint32_t item_id = 0;
    std::string model, husk, character, animations;
};
struct MissionScene {
    MissionData mission;
    std::array<std::vector<ObjectAppearance>,4> objects;
};
struct MissionModelResource {
    std::string name;
    std::uint8_t bank{};
};
struct MissionRuntimeResources {
    MissionData mission;
    ResourcePlan plan;
    ResourcePlanOptions options;
    std::vector<MissionModelResource> models; // First loads in original registry order.
    std::vector<std::string> missing_models; // Original failed loads keep a null model handle.
};
class MissionAssetCache {
public:
    explicit MissionAssetCache(const AssetStore& assets);
    MissionScene load(std::string_view mission);
    // Start with an empty cache. Load the original single-player resource plan.
    MissionRuntimeResources load_runtime(std::string_view mission,const ResourcePlanOptions& options={});
    // Separate 4633F0 load pass. Call after load_runtime and before assembly.
    void load_projectile_runtime(MissionRuntimeResources&);
    const ObjectAppearance& appearance(std::uint32_t item_id);
    const Model& model(std::string_view name) const;
    const CharacterModel& character(std::string_view name) const;
    std::string_view animations(std::string_view name) const;
    const ItemCatalog& items() const noexcept { return items_; }
    std::size_t model_count() const noexcept { return models_.size(); }
    std::size_t character_count() const noexcept { return characters_.size(); }
    std::size_t animation_count() const noexcept { return animations_.size(); }
private:
    void load_runtime_steps(MissionRuntimeResources&,std::size_t begin);
    const AssetStore& assets_;
    ItemCatalog items_;
    std::map<std::uint32_t,ObjectAppearance> appearances_;
    std::map<std::string,Model> models_;
    std::map<std::string,CharacterModel> characters_;
    std::map<std::string,std::string> animations_;
};
}
