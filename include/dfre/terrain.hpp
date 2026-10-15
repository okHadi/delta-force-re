#pragma once

#include "dfre/assets.hpp"

#include <array>
#include <optional>

namespace dfre {

class TerrainGeometry {
public:
    // Inputs use the original runtime row order; each detail word packs color, shade, elevation.
    TerrainGeometry(AssetBytes height, AssetBytes character, std::vector<std::uint32_t> detail);
    std::int32_t base_height(std::int32_t x, std::int32_t y) const;
    std::int32_t surface_height(std::int32_t x, std::int32_t y) const;
    std::int32_t nearest_height(std::int32_t x, std::int32_t y) const;
    std::array<std::int32_t, 2> slope(std::int32_t x, std::int32_t y) const;
    const AssetBytes& height_bytes() const noexcept { return height_; }
    const AssetBytes& character_bytes() const noexcept { return character_; }
    const std::vector<std::uint32_t>& detail_words() const noexcept { return detail_; }

private:
    AssetBytes height_;
    AssetBytes character_;
    std::vector<std::uint32_t> detail_;
};

struct TerrainRuntime {
    TerrainGeometry geometry;
    TerrainConfig config;
    PcxImage color;
    AssetBytes detail_palette;
    PcxImage sky_palette;
    std::optional<PcxImage> sky;
    std::optional<PcxImage> water;
    std::optional<PcxImage> water_palette;
    std::int32_t water_height = 0; // Original fixed-point runtime value; zero disables water.
    unsigned pixel_bits = 8;
};

TerrainRuntime load_terrain_runtime(const AssetStore& assets, std::string_view mission, unsigned pixel_bits = 8);

struct TerrainPalette8 {
    AssetLocation source;
    AssetBytes raw_rgb;
    AssetBytes gamma_rgb;
    std::array<std::uint8_t, 3> fog{};
    std::array<std::uint8_t, 3> light{};
};

AssetBytes load_terrain_gamma(const std::filesystem::path& path);
TerrainPalette8 make_terrain_palette8(const AssetBytes& palette, const AssetBytes& sky_palette, const AssetBytes& gamma);
TerrainPalette8 load_terrain_palette8(const AssetStore& assets, std::string_view mission,
    const TerrainRuntime& terrain, const AssetBytes& gamma);
AssetBytes terrain_height_mip(const AssetBytes& source, std::uint32_t width);
AssetBytes terrain_sky_water_mip(const AssetBytes& source, std::uint32_t width);
std::vector<AssetBytes> build_terrain_height_bounds(const TerrainGeometry& geometry);

struct TerrainBlendTables {
    AssetBytes blend; // 256 by 256 palette lookup.
    AssetBytes shade; // 64 by 256 palette lookup.
    std::vector<std::uint32_t> selectors; // Original authored 16 by 16 blend pattern; values 0 through 11.
};

struct TerrainDetailTile {
    std::array<std::uint16_t, 256> height{};
    std::array<std::uint8_t, 256> color{};
};

std::vector<std::uint32_t> decode_terrain_blend_selectors(const AssetBytes& bytes);
std::vector<std::uint32_t> load_terrain_blend_selectors(const std::filesystem::path& path);
TerrainBlendTables build_terrain_blend_tables(const AssetBytes& gamma_palette,
    const std::array<std::uint8_t, 3>& light_color, std::vector<std::uint32_t> selectors);

// Target RGB entries are already gamma mapped; source RGB entries use gamma during matching.
AssetBytes remap_palette8(const AssetBytes& source_palette, const AssetBytes& target_gamma_palette, const AssetBytes& gamma);

struct TerrainColorControl {
    int brightness = 128;
    int saturation = 128;
    std::array<int, 3> channels{128, 128, 128};
};

// 64 rows by 256 colors. Words pack luminance, red, green, blue; the high byte is not alpha.
std::vector<std::uint32_t> build_terrain_shades32(const AssetBytes& gamma_palette,
    const std::array<std::uint8_t, 3>& light_color, const TerrainColorControl& control = {});

struct TerrainPalettes32 {
    std::array<std::vector<std::uint32_t>, 10> coarse; // 256 two-pixel pairs per level.
    std::vector<std::uint32_t> detail;
    std::vector<std::uint32_t> sky; // 256 gradient rows by 64 cloud values by two pixels.
    std::uint32_t light = 0;
};

TerrainPalettes32 build_terrain_palettes32(const TerrainRuntime& terrain, const AssetBytes& gamma,
    const TerrainColorControl& control = {});
std::vector<AssetBytes> build_terrain_color_mips32(const AssetBytes& color, const AssetBytes& palette,
    const AssetBytes& gamma);
std::array<std::vector<std::uint32_t>, 5> build_terrain_detail_mips32(const TerrainGeometry& geometry,
    const AssetBytes& detail_palette, const AssetBytes& gamma);

struct TerrainDetailTile32 {
    std::array<std::uint16_t, 256> height{};
    std::array<std::uint32_t, 256> color{};
};

TerrainDetailTile32 build_terrain_detail_tile32(const TerrainGeometry& geometry, const AssetBytes& color,
    const std::array<std::vector<std::uint32_t>, 5>& detail_mips, const TerrainPalettes32& palettes,
    std::uint32_t x, std::uint32_t y, unsigned lod);

TerrainDetailTile build_terrain_detail_tile(const TerrainGeometry& geometry, const AssetBytes& color,
    const TerrainBlendTables& tables, std::uint32_t x, std::uint32_t y);

struct VoxelColumn {
    std::int32_t depth_index = -1;
    std::uint32_t finished = 0;
    std::int32_t pixel_offset = 0;
    std::array<std::uint32_t, 3> position{};
    std::array<std::uint32_t, 3> step{};
    std::array<std::uint32_t, 3> acceleration{};
};

struct VoxelPass8 {
    unsigned lod = 0;
    std::uint32_t stop_depth = 0;
    std::uint32_t row_stride = 0;
    std::int32_t top_offset = 0;
    std::array<std::uint32_t, 3> acceleration_step{};
    std::int32_t step_scale = 65536;
};

struct VoxelCamera {
    std::array<std::int32_t, 3> position{}; // Original 16.16 coordinates.
    std::uint32_t yaw = 0;
    std::uint32_t pitch = 0;
    std::uint32_t width = 640;
    std::uint32_t height = 480;
    std::uint32_t row_stride = 640;
    std::int32_t left = 0;
    std::int32_t top = 0;
    std::int32_t right = 639;
    std::int32_t bottom = 479;
    std::int32_t focal_distance = 320;
    std::int32_t field_of_view = 90;
};

struct VoxelColumns {
    std::vector<VoxelColumn> columns;
    std::array<std::uint32_t, 3> acceleration_step{};
};

VoxelColumns initialize_voxel_columns(const VoxelCamera& camera);

struct TerrainHorizon32 {
    std::int32_t row = -1;
    AssetBytes gradient;
};
TerrainHorizon32 build_terrain_horizon32(const VoxelCamera& camera, std::int32_t horizon_scale = 65536);


struct VoxelLod {
    bool detail = false;
    double distance = 0;
    double sample_step = 0;
    std::int32_t world_bound = 0;
    std::uint32_t lod = 0;
    std::int32_t ray_step = 0;
    std::int32_t sample_limit = 0;
    std::uint32_t depth_end = 0;
    std::int32_t next_step_scale = 0;
};

struct VoxelProjection {
    std::vector<VoxelLod> levels;
    std::vector<std::uint32_t> depths;
};

VoxelProjection build_voxel_projection(std::int32_t focal_distance, float detail_density = 2.2f,
                                       float coarse_density = 2.2f, float depth_factor = 1.0f);

struct TerrainScene32 {
    TerrainRuntime terrain;
    TerrainPalettes32 palettes;
    std::array<std::vector<std::uint32_t>, 5> detail_mips;
    std::vector<AssetBytes> height_mips;
    std::vector<AssetBytes> color_mips;
};
TerrainScene32 prepare_terrain_scene32(TerrainRuntime terrain, const AssetBytes& gamma,
    const TerrainColorControl& control = {});

struct TerrainFrame32 {
    std::vector<std::uint32_t> pixels;
    std::vector<std::uint16_t> depths;
    std::vector<VoxelColumn> columns;
};
void render_voxel_pass32(const TerrainScene32& scene, bool detail, const VoxelPass8& pass,
    const std::vector<std::uint32_t>& depth_table, std::vector<VoxelColumn>& columns,
    std::vector<std::uint32_t>& pixels, std::vector<std::uint16_t>& depths);
// Draws original flat sky and terrain. Water and world objects use later passes.
TerrainFrame32 render_terrain_frame32(const TerrainScene32& scene, const VoxelCamera& camera);

// Original 0x459635 byte-color kernel, normal two-pixel-column mode.
// Coordinates use the original wrapping 10.22 representation. The depth table has one guard entry.
void render_voxel_pass_8(const VoxelPass8& pass, const AssetBytes& height, const AssetBytes& color,
                         const std::vector<std::uint32_t>& depth_table, std::vector<VoxelColumn>& columns,
                         AssetBytes& pixels, std::vector<std::uint16_t>& depths);

} // namespace dfre
