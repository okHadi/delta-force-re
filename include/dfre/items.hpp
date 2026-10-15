#pragma once
#include "dfre/assets.hpp"
#include <string>
#include <string_view>
#include <vector>
namespace dfre {
struct ItemField { std::string key; std::vector<std::string> values; };
struct ItemDefinition {
    std::uint32_t id = 0, source_id = 0;
    std::string name, type;
    std::vector<ItemField> fields;
    std::string_view value(std::string_view key) const noexcept;
};
class ItemCatalog {
public:
    explicit ItemCatalog(std::string_view text);
    const ItemDefinition& find(std::uint32_t id) const;
    const ItemDefinition* find_exact(std::uint32_t id) const noexcept;
    const std::vector<ItemDefinition>& entries() const noexcept { return entries_; }
private:
    std::vector<ItemDefinition> entries_;
};
ItemCatalog load_item_catalog(const AssetStore& assets);
}
