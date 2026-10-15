#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dfre {

using AssetBytes = std::vector<std::uint8_t>;

class AssetError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct PffEntry {
    std::string name;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    std::uint32_t timestamp = 0;
    bool deleted = false;
};

class PffArchive {
public:
    explicit PffArchive(std::filesystem::path path);
    const std::filesystem::path& path() const noexcept { return path_; }
    const std::vector<PffEntry>& entries() const noexcept { return entries_; }
    std::uint64_t size() const noexcept { return size_; }
    std::uint64_t footer_bytes() const noexcept { return footer_bytes_; }
    const PffEntry& find(std::string_view name) const;
    AssetBytes read(std::string_view name) const;

private:
    std::filesystem::path path_;
    std::uint64_t size_ = 0;
    std::uint64_t footer_bytes_ = 0;
    std::vector<PffEntry> entries_;
};

struct PcxImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    AssetBytes pixels;
    AssetBytes palette;   // Empty or 256 RGB entries.
    AssetBytes ancillary; // Opaque CerP block, when present.
};

PcxImage decode_pcx(const AssetBytes& data);

struct TerrainField {
    std::string name;
    std::vector<std::string> values;
};

struct TerrainConfig {
    std::string text;
    std::vector<TerrainField> fields; // Order and repeated fields are preserved.
    std::string color_map;
    std::string elevation_map;
    std::string detail_map;
};

TerrainConfig parse_terrain_config(const AssetBytes& data);
std::string mission_terrain_name(const AssetBytes& data);

struct AssetLocation {
    std::string archive;
    std::string member;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
};

struct AssetData {
    AssetLocation source;
    AssetBytes bytes;
};

struct TerrainImage {
    AssetLocation source;
    PcxImage image;
};

struct MissionTerrain {
    AssetLocation mission;
    AssetLocation terrain;
    TerrainConfig config;
    TerrainImage color;
    TerrainImage elevation;
    TerrainImage detail;
};

class AssetStore {
public:
    explicit AssetStore(const std::filesystem::path& game_folder);
    const std::vector<PffArchive>& archives() const noexcept { return archives_; }
    std::vector<std::string> missions() const;
    AssetData read(std::string_view name) const;
    TerrainImage load_image(std::string_view name) const;
    MissionTerrain load_mission(std::string_view name) const;

private:
    std::vector<PffArchive> archives_;
};

} // namespace dfre
