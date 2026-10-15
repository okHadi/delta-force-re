#include "dfre/assets.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace dfre {
namespace {

std::uint16_t u16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0] | (static_cast<unsigned>(data[1]) << 8));
}

std::uint32_t u32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

bool safe_name(std::string_view name) {
    if (name.empty() || name == "." || name == "..") return false;
    for (const unsigned char c : name) {
        if (c < 32 || c >= 127 || c == '/' || c == '\\' || c == ':') return false;
    }
    return true;
}

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}

bool same_name(std::string_view first, std::string_view second) {
    return first.size() == second.size() &&
           std::equal(first.begin(), first.end(), second.begin(),
                      [](char a, char b) { return lower(a) == lower(b); });
}

bool stem(std::string_view name) {
    if (name.empty() || name.size() > 11) return false;
    for (char c : name) {
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '_') return false;
    }
    return true;
}

void require_member(std::string_view name, std::string_view extension) {
    if (name.size() <= extension.size() ||
        !same_name(name.substr(name.size() - extension.size()), extension) ||
        !stem(name.substr(0, name.size() - extension.size()))) {
        throw AssetError("Invalid " + std::string(extension) + " member name");
    }
}

void read_exact(std::ifstream& source, std::uint8_t* target, std::size_t count) {
    if (count == 0) return;
    source.read(reinterpret_cast<char*>(target), static_cast<std::streamsize>(count));
    if (!source || static_cast<std::size_t>(source.gcount()) != count) {
        throw AssetError("Truncated archive read");
    }
}

std::string terminated_name(const std::uint8_t* bytes, std::size_t size) {
    const auto end = std::find(bytes, bytes + size, 0);
    if (end == bytes + size) throw AssetError("Missing member name terminator");
    const std::string name(reinterpret_cast<const char*>(bytes), static_cast<std::size_t>(end - bytes));
    if (!safe_name(name)) throw AssetError("Unsafe archive member name");
    return name;
}

bool space(char c) { return c == ' ' || c == '\t' || c == '\r'; }

struct Token {
    std::string text;
    bool quoted = false;
};

std::vector<Token> tokens(std::string_view line) {
    std::vector<Token> result;
    std::size_t at = 0;
    while (at < line.size()) {
        while (at < line.size() && space(line[at])) ++at;
        if (at == line.size() || line.substr(at, 2) == "//") break;
        Token token;
        if (line[at] == '"') {
            token.quoted = true;
            const auto end = line.find('"', ++at);
            if (end == std::string_view::npos) throw AssetError("Unclosed TRN string");
            token.text = std::string(line.substr(at, end - at));
            at = end + 1;
            if (at < line.size() && !space(line[at]) && line.substr(at, 2) != "//") {
                throw AssetError("Invalid TRN string boundary");
            }
        } else {
            const auto start = at;
            while (at < line.size() && !space(line[at]) && line.substr(at, 2) != "//") {
                if (line[at] == '"') throw AssetError("Invalid TRN quotation");
                ++at;
            }
            token.text = std::string(line.substr(start, at - start));
        }
        result.push_back(std::move(token));
    }
    return result;
}

} // namespace

PffArchive::PffArchive(std::filesystem::path path) : path_(std::move(path)) {
    std::ifstream source(path_, std::ios::binary | std::ios::ate);
    if (!source) throw AssetError("Cannot open archive: " + path_.string());
    const auto end = source.tellg();
    if (end < 20) throw AssetError("Truncated PFF header");
    size_ = static_cast<std::uint64_t>(end);
    source.seekg(0);
    std::array<std::uint8_t, 20> header{};
    read_exact(source, header.data(), header.size());
    const auto count = u32(header.data() + 8);
    const auto table = u32(header.data() + 16);
    if (u32(header.data()) != 20 ||
        !std::equal(header.begin() + 4, header.begin() + 8, "PFF3") ||
        u32(header.data() + 12) != 32) throw AssetError("Expected DF1 PFF3 header");
    if (table < 20 || table > size_ || count > (size_ - table) / 32) {
        throw AssetError("PFF entry table is outside the archive");
    }
    footer_bytes_ = size_ - table - static_cast<std::uint64_t>(count) * 32;
    source.seekg(table);
    entries_.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::array<std::uint8_t, 32> record{};
        read_exact(source, record.data(), record.size());
        const auto flags = u32(record.data());
        if (flags > 1) throw AssetError("Unknown PFF entry flags");
        PffEntry entry;
        entry.name = terminated_name(record.data() + 16, 16);
        entry.offset = u32(record.data() + 4);
        entry.size = u32(record.data() + 8);
        entry.timestamp = u32(record.data() + 12);
        entry.deleted = flags == 1;
        if (entry.deleted && entry.name != "<DEAD SPACE>") throw AssetError("Invalid deleted PFF entry");
        if (entry.offset < 20 || entry.offset > table || entry.size > table - entry.offset) {
            throw AssetError("PFF member data is outside the data section");
        }
        entries_.push_back(std::move(entry));
    }
}

const PffEntry& PffArchive::find(std::string_view name) const {
    if (!safe_name(name)) throw AssetError("Unsafe archive lookup name");
    const PffEntry* found = nullptr;
    for (const auto& entry : entries_) {
        if (!entry.deleted && same_name(name, entry.name)) {
            if (found) throw AssetError("Duplicate live PFF member: " + std::string(name));
            found = &entry;
        }
    }
    if (!found) throw AssetError("Missing PFF member: " + std::string(name));
    return *found;
}

AssetBytes PffArchive::read(std::string_view name) const {
    const auto& entry = find(name);
    std::ifstream source(path_, std::ios::binary | std::ios::ate);
    if (!source || source.tellg() != static_cast<std::streamoff>(size_)) {
        throw AssetError("Archive size changed or archive is unavailable");
    }
    source.seekg(entry.offset);
    AssetBytes data(entry.size);
    read_exact(source, data.data(), data.size());
    return data;
}

PcxImage decode_pcx(const AssetBytes& data) {
    if (data.size() < 128) throw AssetError("Truncated PCX header");
    if (data[0] != 10 || data[1] != 5 || data[2] != 1 || data[3] != 8 || data[65] != 1) {
        throw AssetError("Expected version 5 RLE 8-bit single-plane PCX");
    }
    const auto xmin = u16(data.data() + 4), ymin = u16(data.data() + 6);
    const auto xmax = u16(data.data() + 8), ymax = u16(data.data() + 10);
    if (xmax < xmin || ymax < ymin) throw AssetError("Invalid PCX image bounds");
    PcxImage image;
    image.width = static_cast<std::uint32_t>(xmax) - xmin + 1;
    image.height = static_cast<std::uint32_t>(ymax) - ymin + 1;
    const std::uint32_t stride = u16(data.data() + 66);
    if (image.width > 32768 || image.height > 32768 || stride < image.width || stride % 2) {
        throw AssetError("Invalid PCX size or row stride");
    }
    if (static_cast<std::uint64_t>(stride) * image.height > 64 * 1024 * 1024) {
        throw AssetError("PCX image exceeds the 64 MiB decode limit");
    }
    image.pixels.reserve(static_cast<std::size_t>(image.width) * image.height);
    std::size_t at = 128;
    for (std::uint32_t row = 0; row < image.height; ++row) {
        std::uint32_t column = 0;
        while (column < stride) {
            if (at >= data.size()) throw AssetError("Truncated PCX RLE data");
            auto value = data[at++];
            std::uint32_t count = 1;
            if (value >= 192) {
                count = value & 63;
                if (count == 0) throw AssetError("Zero-length PCX RLE run");
                if (at >= data.size()) throw AssetError("Truncated PCX RLE run");
                value = data[at++];
            }
            if (count > stride - column) throw AssetError("PCX RLE run crosses a row boundary");
            if (column < image.width) {
                image.pixels.insert(image.pixels.end(), std::min(count, image.width - column), value);
            }
            column += count;
        }
    }
    if (image.width == 4 && image.height == 4 && stride == 4 && data.size() - at == 1797 &&
        std::equal(data.begin() + static_cast<std::ptrdiff_t>(at),
                   data.begin() + static_cast<std::ptrdiff_t>(at + 4), "CerP")) {
        image.ancillary.assign(data.begin() + static_cast<std::ptrdiff_t>(at),
                               data.begin() + static_cast<std::ptrdiff_t>(at + 1028));
        at += 1028;
    }
    if (data.size() != at) {
        if (data.size() - at != 769 || data[at] != 12) throw AssetError("Invalid PCX palette or trailing data");
        image.palette.assign(data.begin() + static_cast<std::ptrdiff_t>(at + 1), data.end());
    }
    return image;
}

TerrainConfig parse_terrain_config(const AssetBytes& data) {
    if (data.size() > 65536) throw AssetError("TRN text exceeds 64 KiB");
    for (auto c : data) {
        if (c >= 127 || (c < 32 && c != '\r' && c != '\n' && c != '\t')) {
            throw AssetError("Invalid TRN text byte");
        }
    }
    TerrainConfig config;
    config.text.assign(data.begin(), data.end());
    std::istringstream lines(config.text);
    std::string line;
    while (std::getline(lines, line)) {
        auto words = tokens(line);
        if (words.empty()) continue;
        if (words[0].quoted || words[0].text.empty()) throw AssetError("Invalid TRN field name");
        TerrainField field;
        field.name = words[0].text;
        for (std::size_t at = 1; at < words.size(); ++at) field.values.push_back(words[at].text);
        std::string* map = nullptr;
        if (field.name == "color_map") map = &config.color_map;
        else if (field.name == "elev_map") map = &config.elevation_map;
        else if (field.name == "detail_map") map = &config.detail_map;
        if (map) {
            if (!map->empty() || words.size() != 2 || words[1].quoted || !stem(words[1].text)) {
                throw AssetError("Invalid or duplicate TRN map field: " + field.name);
            }
            *map = words[1].text + ".PCX";
        }
        config.fields.push_back(std::move(field));
    }
    if (config.color_map.empty() || config.elevation_map.empty() || config.detail_map.empty()) {
        throw AssetError("TRN requires color_map, elev_map, and detail_map");
    }
    return config;
}

std::string mission_terrain_name(const AssetBytes& data) {
    if (data.size() < 100 || !std::equal(data.begin(), data.begin() + 4, "BMS\r")) {
        throw AssetError("Expected BMS header and terrain field at byte 84");
    }
    auto name = terminated_name(data.data() + 84, 16);
    require_member(name, ".trn");
    return name;
}

AssetStore::AssetStore(const std::filesystem::path& game_folder) {
    archives_.emplace_back(game_folder / "Df.pff");
    archives_.emplace_back(game_folder / "Dfbase.pff");
}

std::vector<std::string> AssetStore::missions() const {
    std::vector<std::string> result;
    for (const auto& archive : archives_) {
        for (const auto& entry : archive.entries()) {
            if (!entry.deleted && entry.name.size() > 4 &&
                same_name(std::string_view(entry.name).substr(entry.name.size() - 4), ".bms")) {
                result.push_back(entry.name);
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

AssetData AssetStore::read(std::string_view name) const {
    if (!safe_name(name)) throw AssetError("Unsafe asset lookup name");
    const PffArchive* source = nullptr;
    const PffEntry* found = nullptr;
    for (const auto& archive : archives_) {
        for (const auto& entry : archive.entries()) {
            if (!entry.deleted && same_name(entry.name, name)) {
                if (found) throw AssetError("Duplicate live asset: " + std::string(name));
                source = &archive;
                found = &entry;
            }
        }
    }
    if (!found) throw AssetError("Missing asset: " + std::string(name));
    return {{source->path().filename().string(), found->name, found->offset, found->size}, source->read(found->name)};
}

TerrainImage AssetStore::load_image(std::string_view name) const {
    auto asset = read(name);
    return {std::move(asset.source), decode_pcx(asset.bytes)};
}

MissionTerrain AssetStore::load_mission(std::string_view name) const {
    require_member(name, ".bms");
    auto mission = read(name);
    auto terrain = read(mission_terrain_name(mission.bytes));
    auto config = parse_terrain_config(terrain.bytes);
    auto color = load_image(config.color_map);
    auto elevation = load_image(config.elevation_map);
    auto detail = load_image(config.detail_map);
    return {std::move(mission.source), std::move(terrain.source), std::move(config),
            std::move(color), std::move(elevation), std::move(detail)};
}

} // namespace dfre
