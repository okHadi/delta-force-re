#pragma once
#include "dfre/items.hpp"
#include "dfre/mission.hpp"
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
class MissionAssetCache {
public:
    explicit MissionAssetCache(const AssetStore& assets);
    MissionScene load(std::string_view mission);
    const ObjectAppearance& appearance(std::uint32_t item_id);
    const Model& model(std::string_view name) const;
    const CharacterModel& character(std::string_view name) const;
    std::string_view animations(std::string_view name) const;
    const ItemCatalog& items() const noexcept { return items_; }
    std::size_t model_count() const noexcept { return models_.size(); }
    std::size_t character_count() const noexcept { return characters_.size(); }
    std::size_t animation_count() const noexcept { return animations_.size(); }
private:
    const AssetStore& assets_;
    ItemCatalog items_;
    std::map<std::uint32_t,ObjectAppearance> appearances_;
    std::map<std::string,Model> models_;
    std::map<std::string,CharacterModel> characters_;
    std::map<std::string,std::string> animations_;
};
}
