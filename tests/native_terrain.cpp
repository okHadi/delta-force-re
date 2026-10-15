#include "dfre/terrain.hpp"

#include <iostream>
#include <limits>
#include <cstring>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Function> void rejects(Function function) {
    try { function(); }
    catch (const dfre::AssetError&) { return; }
    throw std::runtime_error("Invalid terrain data accepted");
}

void projection_goldens() {
    // Numeric outputs from original 0x489440; regenerate with the differential tool's --golden flag.
    constexpr std::uint64_t expected[] = {
        0x916fb2ae431aaaf1ull,
        0xab49ff008a1622f9ull,
        0x7eb9782db8a8da0full,
        0xd9a99472aec21120ull,
        0x6011c01325fd23faull,
        0x3825d5298c58769dull,
        0x19aad99a250b577dull,
        0xa6896658dc5b996aull,
        0x9ab919f298af562eull,
        0x67752e4298cc295dull,
        0xcaa61f13fd74867dull,
        0x47aa410c185c1a04ull,
        0xe2a51e039884ddbdull,
        0xf563323d9698fc6cull,
        0x4116802be35256eeull,
        0xad1d050be3a1d195ull,
        0x212026bd3ebd9818ull,
        0xb5b195410f1a769bull,
        0xdd3e9f13b6b13fa1ull,
        0x74d0953e2955ef14ull,
        0x1c940a6876a8cc37ull,
        0xe286e129bddf2f2bull,
        0x00c5b84a70c352b1ull,
        0x66d4a9ab82c5c3e4ull,
        0x111416da8dc26832ull,
        0x1076da38a893b533ull,
        0x0286dd49f9c2567dull,
        0xf7ede90d59e277ceull,
        0xf884b2fafe05a10cull,
        0x0b7ac947f2533f14ull,
        0x8791d62227c83630ull,
        0xf83e4aa9707cd073ull,
        0xd384b7bb810958d1ull,
        0xa63338f84eb6b632ull,
        0x7eb9782db8a8da0full,
        0xf3c6820ad6d98a1aull,
        0x2452e4628caa70f3ull,
        0xdf053cd140109244ull,
        0xda3813b169a03b1full,
        0x593955b55e197edaull,
        0x5a4a39dced98840dull,
        0xc758572f53acfdedull,
        0xd5f8332ab7e703b7ull,
        0x47aa410c185c1a04ull,
        0x2ca6f56aa0a3c05dull,
        0x51e6fe46b5e91969ull,
        0x4116802be35256eeull,
        0xa615e67b3656af98ull,
        0xc58ae5258712b7d4ull,
        0xb5b195410f1a769bull,
        0x9391002c0f418aadull,
        0x3dc0fc439040d882ull,
        0x7bb6b4c40747e1a7ull,
        0x3471ea70d3eab664ull,
        0x754aa8cc9b5454bbull,
        0x6b714d1cb914c76bull,
        0x5ef8482ab765f76bull,
        0x70442c5d1f434c07ull,
        0x0286dd49f9c2567dull,
        0xa6b03e736528edf6ull,
        0xbc76701ad1d8db33ull,
        0x0b7ac947f2533f14ull,
        0x5d1a15256143dba1ull,
        0x0115022a1d5684e1ull,
    };
    for (std::uint32_t seed = 0; seed < 64; ++seed) {
        const auto result = dfre::build_voxel_projection(80 + static_cast<std::int32_t>(seed % 32) * 17,
            seed % 2 ? 3.2f : 2.2f, seed % 3 ? 2.2f : 3.2f, seed % 5 ? 1.0f : 0.75f);
        std::uint64_t hash = 0xcbf29ce484222325ull;
        auto word = [&](std::uint64_t value, unsigned bytes) {
            for (unsigned i = 0; i < bytes; ++i) { hash = (hash ^ (value & 255)) * 0x100000001b3ull; value >>= 8; }
        };
        auto real = [&](double value) { std::uint64_t bits; std::memcpy(&bits, &value, 8); word(bits, 8); };
        for (const auto& row : result.levels) {
            word(row.detail, 4); real(row.distance); word(static_cast<std::uint32_t>(row.world_bound), 4);
            real(row.sample_step); word(row.lod, 4); word(static_cast<std::uint32_t>(row.ray_step), 4);
            word(static_cast<std::uint32_t>(row.sample_limit), 4); word(row.depth_end, 4);
            word(static_cast<std::uint32_t>(row.next_step_scale), 4);
        }
        for (auto depth : result.depths) word(depth, 4);
        require(hash == expected[seed], "Original projection numeric hash failed");
    }
}

void palette_goldens() {
    constexpr std::uint64_t expected[][2] = {{0x888410c7d934916bull, 0x228de15de1ff1c8dull},
        {0x7b0865d6d8e301a5ull, 0x6d2dda673267e1feull}, {0xc5cba50f333a0124ull, 0x8746332021b7b825ull}};
    auto fingerprint = [](const dfre::AssetBytes& bytes) {
        std::uint64_t hash = 0xcbf29ce484222325ull;
        for (auto value : bytes) hash = (hash ^ value) * 0x100000001b3ull;
        return hash;
    };
    for (unsigned seed = 0; seed < 3; ++seed) {
        dfre::AssetBytes palette(768);
        for (unsigned i = 0; i < 256; ++i) for (unsigned channel = 0; channel < 3; ++channel) {
            constexpr unsigned multipliers[] = {37, 67, 101};
            palette[i * 3 + channel] = static_cast<std::uint8_t>(seed == 1 ? i : seed == 2 ? 0 : i * multipliers[channel]);
        }
        const auto result = dfre::build_terrain_blend_tables(palette, {230, 210, 190}, std::vector<std::uint32_t>(256, 0));
        require(fingerprint(result.blend) == expected[seed][0] && fingerprint(result.shade) == expected[seed][1],
                "Original palette table numeric hash failed");
        for (unsigned i = 0; i < 256; ++i) require(result.blend[i] == i && result.blend[i * 256] == i,
                "Palette index zero must preserve the other blend input");
    }
    rejects([&] { dfre::build_terrain_blend_tables({}, {0, 0, 0}, std::vector<std::uint32_t>(256)); });
    rejects([&] { dfre::build_terrain_blend_tables(dfre::AssetBytes(768), {0, 0, 0}, {}); });
    rejects([&] { auto selectors = std::vector<std::uint32_t>(256, 0); selectors[0] = 12;
        dfre::build_terrain_blend_tables(dfre::AssetBytes(768), {0, 0, 0}, selectors); });
}

void shades32_goldens() {
    constexpr std::uint64_t expected[] = {0x4c99a9c26266bb02ull, 0xb91f132617737cadull, 0xbe84ec9c21575725ull,
        0x9b00226843bee316ull, 0x1eff5a0362238858ull, 0x91f973d98f658725ull,
        0xd53dd503197c6a08ull, 0x6686dc7f85a14cb2ull, 0x5936385eace80125ull};
    for (unsigned seed = 0; seed < 9; ++seed) {
        dfre::AssetBytes palette(768);
        for (unsigned i = 0; i < 256; ++i) for (unsigned channel = 0; channel < 3; ++channel) {
            constexpr unsigned multipliers[] = {37, 67, 101};
            palette[i * 3 + channel] = static_cast<std::uint8_t>(seed % 3 == 1 ? i : seed % 3 == 2 ? 0 : i * multipliers[channel]);
        }
        dfre::TerrainColorControl control;
        if (seed / 3 == 1) { control.brightness = 64; control.saturation = 0; control.channels = {255, 64, 192}; }
        if (seed / 3 == 2) { control.brightness = 255; control.saturation = 255; control.channels = {255, 128, 1}; }
        const auto shades = dfre::build_terrain_shades32(palette, {230, 210, 190}, control);
        std::uint64_t hash = 0xcbf29ce484222325ull;
        for (auto value : shades) for (unsigned shift = 0; shift < 32; shift += 8) {
            hash = (hash ^ ((value >> shift) & 255)) * 0x100000001b3ull;
        }
        require(hash == expected[seed], "Original 32-bit shade numeric hash failed");
    }
    const auto white = dfre::build_terrain_shades32(dfre::AssetBytes(768, 255), {255, 255, 255});
    require(white[32 * 256] == 0xfefefefe && white[0] == 0,
            "Original neutral channel rounding or luminance packing failed");
    rejects([&] { dfre::build_terrain_shades32({}, {0, 0, 0}); });
    rejects([&] { dfre::TerrainColorControl control; control.brightness = -1;
        dfre::build_terrain_shades32(dfre::AssetBytes(768), {0, 0, 0}, control); });
    rejects([&] { dfre::TerrainColorControl control; control.saturation = 256;
        dfre::build_terrain_shades32(dfre::AssetBytes(768), {0, 0, 0}, control); });
    rejects([&] { dfre::TerrainColorControl control; control.channels[2] = 256;
        dfre::build_terrain_shades32(dfre::AssetBytes(768), {0, 0, 0}, control); });
}
} // namespace

int main() {
    try {
        dfre::AssetBytes height(1024 * 1024, 40), character(height.size(), 7);
        std::vector<std::uint32_t> detail(65536, 0);
        dfre::TerrainGeometry flat(height, character, detail);
        for (auto x : {0, -1, 65535, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
            require(flat.base_height(x, x) == 655360 && flat.surface_height(x, x) == 655360, "Flat terrain failed");
            require(flat.slope(x, x) == std::array<std::int32_t, 2>{0, 0}, "Flat terrain slope failed");
        }
        height.assign(height.size(), 0);
        height[0] = 10; height[1] = 20; height[1024] = 30; height[1025] = 40;
        dfre::TerrainGeometry ramp(height, character, detail);
        require(ramp.base_height(0, 0) == 10 * 16384, "Grid height scale failed");
        require(ramp.base_height(32768, 32768) == 25 * 16384, "Bilinear midpoint failed");
        require(ramp.base_height(1024 * 65536, 0) == ramp.base_height(0, 0), "Positive wrap failed");
        require(ramp.base_height(-65536, 0) == 0, "Negative wrap failed");
        require(ramp.base_height(-32768, 0) == 5 * 16384, "Negative interpolation failed");
        require(ramp.nearest_height(65535, 65535) == 10 * 16384, "Nearest height changed with fraction");
        height[0] = 0; height[1] = 255; height[1024] = 255; height[1025] = 0;
        dfre::TerrainGeometry steep(height, character, detail);
        require(steep.base_height(1, 1) == 0, "Small-fraction rounding failed");
        require(steep.base_height(65535, 1) == 4177792, "Signed interpolation rounding failed");
        height.assign(height.size(), 0);
        detail[7 * 256] = 128u << 16;
        detail[7 * 256 + 1] = 129u << 16;
        detail[7 * 256 + 2] = 127u << 16;
        detail[7 * 256 + 16] = 255u << 16;
        dfre::TerrainGeometry detailed(height, character, detail);
        require(detailed.surface_height(0, 0) == 0, "Neutral detail changed height");
        require(detailed.surface_height(4096, 0) == 1024, "Positive detail height failed");
        require(detailed.surface_height(8192, 0) == 0, "Negative detail must stay inactive");
        require(detailed.surface_height(0, 4096) == 130048, "Detail row order failed");
        height[2] = 50; height[1022] = 10; height[2 * 1024] = 80; height[1022 * 1024] = 20;
        dfre::TerrainGeometry slopes(height, character, detail);
        require(slopes.slope(0, 0) == std::array<std::int32_t, 2>{40 * 4096, 60 * 4096}, "Wrapped slope failed");
        rejects([&] { dfre::TerrainGeometry bad({}, character, detail); });
        rejects([&] { dfre::TerrainGeometry bad(height, {}, detail); });
        rejects([&] { dfre::TerrainGeometry bad(height, character, {}); });
        dfre::AssetBytes palette_rgb(768), sky_rgb(768, 0), gamma(256);
        for (unsigned i = 0; i < palette_rgb.size(); ++i) palette_rgb[i] = static_cast<std::uint8_t>(i);
        for (unsigned i = 0; i < gamma.size(); ++i) gamma[i] = static_cast<std::uint8_t>(255 - i);
        sky_rgb[0] = 10; sky_rgb[1] = 20; sky_rgb[2] = 30;
        sky_rgb[48] = 100; sky_rgb[49] = 110; sky_rgb[50] = 120;
        const auto mission_palette = dfre::make_terrain_palette8(palette_rgb, sky_rgb, gamma);
        require(mission_palette.raw_rgb == palette_rgb && mission_palette.gamma_rgb[0] == 255 &&
                mission_palette.gamma_rgb[255] == 0 && mission_palette.gamma_rgb[256] == 255,
                "Original gamma palette mapping failed");
        require(mission_palette.fog == std::array<std::uint8_t, 3>{245, 235, 225} &&
                mission_palette.light == std::array<std::uint8_t, 3>{155, 145, 135},
                "Original sky fog and light palette selection failed");
        rejects([&] { dfre::make_terrain_palette8({}, sky_rgb, gamma); });
        rejects([&] { dfre::make_terrain_palette8(palette_rgb, {}, gamma); });
        rejects([&] { dfre::make_terrain_palette8(palette_rgb, sky_rgb, {}); });
        dfre::TerrainRuntime color_runtime{flat, {}, {}, {}, {}, {}, {}, {}, 0, 32};
        color_runtime.color.palette.resize(768); color_runtime.detail_palette.resize(768);
        color_runtime.sky_palette.palette.resize(768);
        dfre::AssetBytes table_gamma(256);
        for (unsigned i = 0; i < 256; ++i) {
            table_gamma[i] = static_cast<std::uint8_t>(i * i / 255);
            for (unsigned channel = 0; channel < 3; ++channel) {
                constexpr unsigned multipliers[] = {37, 67, 101}, detail_multipliers[] = {17, 29, 43};
                color_runtime.color.palette[i * 3 + channel] = static_cast<std::uint8_t>(i * multipliers[channel]);
                color_runtime.detail_palette[i * 3 + channel] = static_cast<std::uint8_t>(i * detail_multipliers[channel]);
                color_runtime.sky_palette.palette[i * 3 + channel] = static_cast<std::uint8_t>(i * 11 + channel * 31);
            }
        }
        const auto color_tables = dfre::build_terrain_palettes32(color_runtime, table_gamma);
        auto words_hash = [](const std::vector<std::uint32_t>& words) {
            std::uint64_t hash = 0xcbf29ce484222325ull;
            for (auto word : words) for (unsigned shift = 0; shift < 32; shift += 8) {
                hash = (hash ^ ((word >> shift) & 255)) * 0x100000001b3ull;
            }
            return hash;
        };
        std::vector<std::uint32_t> coarse_words;
        for (const auto& level : color_tables.coarse) {
            require(level.size() == 512, "Original coarse palette dimensions failed");
            for (unsigned i = 0; i < 256; ++i) require(level[i * 2] == level[i * 2 + 1], "Original coarse palette pair failed");
            coarse_words.insert(coarse_words.end(), level.begin(), level.end());
        }
        require(words_hash(coarse_words) == 0x6d14f15b40c1a315ull && words_hash(color_tables.detail) == 0xe34e5efbe8564de5ull &&
                words_hash(color_tables.sky) == 0x65bc340d1e951c71ull && color_tables.light == 3484471278u,
                "Original terrain color table numeric hashes failed");
        rejects([&] { dfre::build_terrain_palettes32(color_runtime, {}); });
        const dfre::AssetBytes values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        require(dfre::terrain_height_mip(values, 4) == dfre::AssetBytes({6, 8, 14, 16}), "Height mip must use maximum");
        require(dfre::terrain_sky_water_mip(values, 4) == dfre::AssetBytes({3, 5, 11, 13}), "Sky/water mip rounding failed");
        require(dfre::terrain_sky_water_mip(dfre::AssetBytes(4, 0), 2) == dfre::AssetBytes({0}), "Zero mip underflowed");
        for (auto width : {0u, 1u, 3u, 2048u}) rejects([&] { dfre::terrain_height_mip(values, width); });
        rejects([&] { dfre::terrain_sky_water_mip({}, 4); });
        dfre::AssetBytes selector_bytes(1024, 0);
        selector_bytes[0] = 11;
        const auto selectors = dfre::decode_terrain_blend_selectors(selector_bytes);
        require(selectors.size() == 256 && selectors[0] == 11 && selectors[1] == 0, "Blend selector decoding failed");
        rejects([&] { dfre::decode_terrain_blend_selectors({}); });
        rejects([&] { auto bad = selector_bytes; bad.pop_back(); dfre::decode_terrain_blend_selectors(bad); });
        rejects([&] { auto bad = selector_bytes; bad[0] = 12; dfre::decode_terrain_blend_selectors(bad); });
        rejects([&] { auto bad = selector_bytes; bad[3] = 1; dfre::decode_terrain_blend_selectors(bad); });
        dfre::TerrainBlendTables tables{dfre::AssetBytes(65536, 0), dfre::AssetBytes(16384, 3),
            std::vector<std::uint32_t>(256, 0)};
        auto render_detail = std::vector<std::uint32_t>(65536, 128u << 16);
        render_detail[7 * 256 + 1] = 129u << 16;
        render_detail[7 * 256 + 2] = 130u << 16;
        render_detail[7 * 256 + 3] |= 5;
        render_detail[7 * 256 + 4] |= (4u << 8) | 4;
        tables.blend[3 * 256 + 4] = 99;
        const dfre::TerrainGeometry render_geometry(dfre::AssetBytes(1024 * 1024, 40), character, render_detail);
        const auto tile = dfre::build_terrain_detail_tile(render_geometry, dfre::AssetBytes(1024 * 1024, 17), tables, 0, 0);
        require(tile.height[0] == 640 && tile.height[1] == 640 && tile.height[2] == 641,
                "Original detail render height scale failed");
        require(tile.color[0] == 17 && tile.color[3] == 5 && tile.color[4] == 99,
                "Original detail paint and shade lookup failed");
        dfre::AssetBytes identity_gamma(256), gray_palette(768);
        for (unsigned i = 0; i < 256; ++i) {
            identity_gamma[i] = static_cast<std::uint8_t>(i);
            gray_palette[i * 3] = gray_palette[i * 3 + 1] = gray_palette[i * 3 + 2] = static_cast<std::uint8_t>(i);
        }
        auto detail32 = std::vector<std::uint32_t>(65536, 128u << 16);
        detail32[0] |= 5; detail32[1] |= 5;
        detail32[16] |= 255u << 8;
        detail32[17] = 255u << 16;
        const dfre::TerrainGeometry geometry32(dfre::AssetBytes(1024 * 1024, 40), dfre::AssetBytes(1024 * 1024), detail32);
        const auto mips32 = dfre::build_terrain_detail_mips32(geometry32, gray_palette, identity_gamma);
        require(mips32[0] == detail32 && mips32[1][0] == (5u | (63u << 8) | (255u << 16)),
                "Detail mip paint count, shade average or height maximum failed");
        require(mips32[4].size() == 256 && (mips32[2][0] & 255) == 0,
                "Sparse detail paint must clear at the next mip level");
        dfre::TerrainPalettes32 flat_colors;
        flat_colors.coarse[0].assign(512, 0x80706050); flat_colors.detail.assign(256, 0x12345678);
        flat_colors.light = 0x00ffffff;
        for (unsigned lod = 0; lod < 4; ++lod) {
            const auto output = dfre::build_terrain_detail_tile32(geometry32, dfre::AssetBytes(1024 * 1024), mips32,
                flat_colors, 0xffffffff, 0xffffffff, lod);
            require(output.height[255] == 640 && output.color[255] == 0x80706050,
                    "Flat 32-bit detail tile or wrapped tile origin failed");
        }
        const auto painted32 = dfre::build_terrain_detail_tile32(geometry32, dfre::AssetBytes(1024 * 1024), mips32,
            flat_colors, 0, 0, 0);
        require(painted32.color[0] == 0x12345678 && painted32.height[17] == 703,
                "Unshaded paint replacement or positive visual detail scale failed");
        rejects([&] { dfre::build_terrain_detail_mips32(geometry32, {}, identity_gamma); });
        rejects([&] { dfre::build_terrain_detail_tile32(geometry32, {}, mips32, flat_colors, 0, 0, 0); });
        rejects([&] { dfre::build_terrain_detail_tile32(geometry32, dfre::AssetBytes(1024 * 1024), mips32, flat_colors, 0, 0, 4); });
        rejects([&] { auto bad = mips32; bad[2].pop_back(); dfre::build_terrain_detail_tile32(geometry32,
            dfre::AssetBytes(1024 * 1024), bad, flat_colors, 0, 0, 2); });
        const auto color_mips = dfre::build_terrain_color_mips32(dfre::AssetBytes(1024 * 1024, 42), gray_palette, identity_gamma);
        require(color_mips.size() == 10 && color_mips.back() == dfre::AssetBytes(4, 42),
                "Original color mip dimensions or palette match failed");
        rejects([&] { dfre::build_terrain_color_mips32({}, gray_palette, identity_gamma); });
        rejects([&] { dfre::build_terrain_color_mips32(dfre::AssetBytes(1024 * 1024), {}, identity_gamma); });
        const auto bounds = dfre::build_terrain_height_bounds(render_geometry);
        require(bounds.size() == 10 && bounds[0].size() == 1024 * 1024 && bounds.back().size() == 4,
                "Original height-bound mip dimensions failed");
        require(bounds[0][0] == 41 && bounds.back()[0] == 41,
                "Original positive detail bound rounding failed");
        rejects([&] { auto bad = tables; bad.selectors[0] = 12; dfre::build_terrain_detail_tile(render_geometry, dfre::AssetBytes(1024 * 1024), bad, 0, 0); });
        rejects([&] { dfre::build_terrain_detail_tile(render_geometry, {}, tables, 0, 0); });
        dfre::VoxelPass8 pass;
        pass.lod = 9; pass.row_stride = 4;
        std::vector<dfre::VoxelColumn> columns(2);
        columns[0].pixel_offset = 12; columns[1].pixel_offset = 14;
        dfre::AssetBytes pixels(16, 0);
        std::vector<std::uint16_t> depths(16, 0);
        dfre::render_voxel_pass_8(pass, dfre::AssetBytes(4, 1), dfre::AssetBytes(4, 42),
                                  {0x12341234, 0x56785678}, columns, pixels, depths);
        require(pixels == dfre::AssetBytes(16, 42) && depths == std::vector<std::uint16_t>(16, 0x1234),
                "Voxel pixel pairs or packed depths failed");
        require(columns[0].finished == 1 && columns[0].pixel_offset == -4 && columns[1].pixel_offset == -2,
                "Voxel top clipping failed");
        columns.assign(1, {}); columns[0].pixel_offset = 12; columns[0].position[2] = 0x10000000;
        pixels.assign(16, 0);
        dfre::render_voxel_pass_8(pass, dfre::AssetBytes(4, 1), dfre::AssetBytes(4, 42),
                                  {0x12341234, 0x56785678}, columns, pixels, depths);
        require(columns[0].depth_index == 1 && columns[0].finished == 0 && pixels == dfre::AssetBytes(16, 0),
                "Voxel depth-stop guard behavior failed");
        columns.assign(1, {}); columns[0].pixel_offset = 12;
        columns[0].position[2] = 0x10000001; columns[0].step[0] = 123; pass.step_scale = 131072;
        dfre::render_voxel_pass_8(pass, dfre::AssetBytes(4, 1), dfre::AssetBytes(4, 42),
                                  {0x12341234, 0x56785678}, columns, pixels, depths);
        require(columns[0].finished == 1 && columns[0].depth_index == -1 && columns[0].step[0] == 123,
                "High upward voxel ray must stop before state scaling");
        rejects([&] { auto bad = pass; bad.lod = 10; dfre::render_voxel_pass_8(bad, {}, {}, {}, columns, pixels, depths); });
        rejects([&] { dfre::render_voxel_pass_8(pass, {}, {}, {1, 2}, columns, pixels, depths); });
        rejects([&] { dfre::render_voxel_pass_8(pass, {1, 1, 1, 1}, {1, 1, 1, 1}, {1}, columns, pixels, depths); });
        const auto horizon = dfre::build_terrain_horizon32(dfre::VoxelCamera{});
        require(horizon.row == 245 && horizon.gradient[245] == 0 && horizon.gradient[244] == 2 &&
                horizon.gradient[0] == 255 && horizon.gradient[246] == 0,
                "Original 32-bit sky horizon or gradient increment failed");
        auto downward_camera = dfre::VoxelCamera{}; downward_camera.pitch = 0xe0000000;
        require(dfre::build_terrain_horizon32(downward_camera).row == -1,
                "Downward pitch must hide the sky horizon");
        auto upward_camera = dfre::VoxelCamera{}; upward_camera.pitch = 0x40000000;
        require(dfre::build_terrain_horizon32(upward_camera).row == 725,
                "Steep upward pitch must use the original height limit");
        rejects([&] { auto bad = dfre::VoxelCamera{}; bad.field_of_view = 0; dfre::build_terrain_horizon32(bad); });
        rejects([&] { dfre::build_terrain_horizon32(dfre::VoxelCamera{}, -1); });
        const auto rays = dfre::initialize_voxel_columns(dfre::VoxelCamera{});
        require(rays.columns.size() == 320 && rays.columns[0].pixel_offset == 479 * 640 &&
                rays.columns[0].depth_index == 0 && rays.columns[0].step[0] == 262080 &&
                rays.columns[160].step[1] == 0, "Original camera columns failed");
        require(rays.acceleration_step[0] == 0 && rays.acceleration_step[1] == 0 &&
                rays.acceleration_step[2] == std::uint32_t(0) - 819, "Original vertical acceleration failed");
        rejects([&] { auto bad = dfre::VoxelCamera{}; bad.focal_distance = 0; dfre::initialize_voxel_columns(bad); });
        rejects([&] { auto bad = dfre::VoxelCamera{}; bad.width = 2048; dfre::initialize_voxel_columns(bad); });
        rejects([&] { auto bad = dfre::VoxelCamera{}; bad.right = 638; dfre::initialize_voxel_columns(bad); });
        projection_goldens();
        palette_goldens();
        shades32_goldens();
        const auto projection = dfre::build_voxel_projection(320);
        require(projection.levels.size() == 14 && projection.depths.size() == 1022,
                "Original LOD record or depth count failed");
        require(projection.levels[0].detail && !projection.levels[5].detail &&
                projection.levels[0].ray_step == 204 && projection.levels[1].next_step_scale == 131393 &&
                projection.levels[5].depth_end == 438 && projection.levels.back().next_step_scale == 0,
                "Original LOD fixed-point scaling failed");
        require(projection.levels.back().world_bound == std::numeric_limits<std::int32_t>::min(),
                "Original inactive LOD overflow result failed");
        require(projection.depths[0] == 0x00010001 && projection.depths[1] == 0x00050005,
                "Original packed depth values failed");
        rejects([&] { dfre::build_voxel_projection(0); });
        rejects([&] { dfre::build_voxel_projection(320, std::numeric_limits<float>::quiet_NaN()); });
        rejects([&] { dfre::build_voxel_projection(320, 0.0f); });
        rejects([&] { dfre::build_voxel_projection(320, 2.2f, 2.2f, 0.0f); });
        rejects([&] { dfre::build_voxel_projection(32767, 1.0f, 1.0f, 0.5f); });
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
