#include "dfre/items.hpp"
#include "dfre/scripts.hpp"
#include <algorithm>
#include <charconv>
#include <sstream>
#include <unordered_set>
namespace dfre {
namespace {
std::string lower(std::string value) {
    for (auto& c : value) if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return value;
}
std::vector<std::string> tokens(std::string_view line) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < line.size()) {
        while (at < line.size() && (line[at]==' ' || line[at]=='\t' || line[at]=='\r')) ++at;
        if (at == line.size() || line[at] == '/') break;
        const bool quoted = line[at] == '"';
        if (quoted) ++at;
        const auto start = at;
        while (at < line.size() && (quoted ? line[at] != '"' : line[at]!=' ' && line[at]!='\t' && line[at]!='\r' && line[at]!='/')) ++at;
        out.emplace_back(line.substr(start,at-start));
        if (quoted) {
            if (at == line.size()) throw AssetError("Unclosed item text quote");
            ++at;
            if (at < line.size() && line[at]!=' ' && line[at]!='\t' && line[at]!='\r' && line[at]!='/')
                throw AssetError("Missing space after item text quote");
        }
    }
    return out;
}
}
std::string_view ItemDefinition::value(std::string_view key) const noexcept {
    for (const auto& field : fields)
        if (field.key == key && !field.values.empty()) return field.values.front();
    return {};
}
ItemCatalog::ItemCatalog(std::string_view text) {
    if (text.size() > 16*1024*1024 || text.find('\0') != std::string_view::npos)
        throw AssetError("Invalid item definition text");
    std::istringstream source{std::string(text)};
    std::string line;
    ItemDefinition current;
    bool open = false, has_id = false, has_type = false;
    std::unordered_set<std::uint32_t> ids;
    while (std::getline(source,line)) {
        auto words = tokens(line);
        if (words.empty()) continue;
        const auto command = lower(words[0]);
        if (command == "begin") {
            if (open || words.size()!=2 || words[1].empty()) throw AssetError("Invalid item begin record");
            current = {}; current.name = words[1]; open = true; has_id = has_type = false;
        } else if (command == "end") {
            if (!open || words.size()!=1 || !has_id || !has_type) throw AssetError("Incomplete item definition");
            if (!ids.insert(current.id).second) throw AssetError("Duplicate item ID");
            entries_.push_back(std::move(current)); open = false;
        } else {
            if (!open || words.size()<2) throw AssetError("Item field outside a definition");
            if (command == "id") {
                if (has_id || words.size()!=2) throw AssetError("Invalid item ID field");
                const auto parsed = std::from_chars(words[1].data(),words[1].data()+words[1].size(),current.source_id);
                if (parsed.ec!=std::errc{} || parsed.ptr!=words[1].data()+words[1].size()) throw AssetError("Invalid item ID");
                current.id = current.source_id - 100000u;
                has_id = true;
            } else if (command == "type") {
                if (has_type || words.size()!=2) throw AssetError("Invalid item type field");
                current.type = lower(words[1]); has_type = true;
            }
            current.fields.push_back({command,{words.begin()+1,words.end()}});
        }
    }
    if (open) throw AssetError("Unclosed item definition");
    if (entries_.empty()) throw AssetError("Missing item definitions");
}
const ItemDefinition* ItemCatalog::find_exact(std::uint32_t id) const noexcept {
    for (const auto& entry : entries_) if (entry.id == id) return &entry;
    return nullptr;
}
const ItemDefinition& ItemCatalog::find(std::uint32_t id) const {
    if (const auto* entry = find_exact(id)) return *entry;
    return entries_.front(); // Original 0x4828c0 returns table index zero.
}
ItemCatalog load_item_catalog(const AssetStore& assets) {
    return ItemCatalog(decode_game_script(assets.read("items.def").bytes));
}
}
