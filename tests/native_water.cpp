#include "dfre/water.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Function> void rejects(Function function) {
    try { function(); }
    catch (const dfre::AssetError&) { return; }
    throw std::runtime_error("Invalid water data accepted");
}

std::uint64_t fingerprint(const dfre::AssetBytes& bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (auto byte : bytes) hash = (hash ^ byte) * 0x100000001b3ull;
    return hash;
}

dfre::AssetBytes words_bytes(const std::vector<std::uint32_t>& words) {
    dfre::AssetBytes bytes;
    bytes.reserve(words.size() * 4);
    for (auto word : words) for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(word >> shift));
    return bytes;
}

// Fixture data mirrors tools/check_water_original.py and tools/water_probe.cpp.
dfre::AssetBytes water_colors() {
    dfre::AssetBytes colors(17 * 3);
    for (unsigned i = 0; i < 17; ++i) for (unsigned channel = 0; channel < 3; ++channel) {
        colors[i * 3 + channel] = static_cast<std::uint8_t>(i * 23 + channel * 41 + 7);
    }
    return colors;
}

constexpr std::uint32_t fixture_width = 94, fixture_height = 48;

std::uint32_t fixture_depth(std::uint32_t seed, std::uint32_t row, std::uint32_t x) {
    if ((row + x + seed) % 9 == 0) return 0xffff;
    if ((row * 5 + x + seed) % 7 == 0) return (seed * 971 + row * 131 + x * 17) & 0x0fff;
    return (seed * 971 + row * 131 + x * 17) & 0xffff;
}

void wave_table() {
    // Full table from original 0x489440 with the default 0xBD5270 shift 4.
    const auto table = dfre::build_water_wave_table(4);
    dfre::AssetBytes bytes;
    bytes.reserve(table.size() * 4);
    for (auto value : table) for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(std::uint32_t(value) >> shift));
    require(fingerprint(bytes) == 0xee3c8f7a1c1f1adaull, "Water wave table differs from the original");
    require(table[0] == 0 && table[1] == 1608 && table[255] == 262139 && table[256] == 262143 &&
        table[257] == 262139, "Water wave table samples differ from the original");
    const auto wrapped = dfre::build_water_wave_table(23);
    require(wrapped[1] == -13176712, "Wrapped wave scale must follow the original signed shift");
}

void clut_table() {
    const auto palette = dfre::build_water_palette32(water_colors(), 0x10000, {});
    require(palette.blends.size() == 64 * 256, "Water CLUT size differs");
    require(fingerprint(words_bytes(palette.blends)) == 0x74b22f5defe52125ull, "Water CLUT differs from the original");
    require(palette.blends[0] == 788934744 && palette.blends[3] == 788934744, "Water CLUT samples differ");
    // Full opacity selects pattern[4 * texel] for every luminance.
    for (unsigned texel = 0; texel < 64; ++texel) {
        require(palette.blends[texel * 256 + 0] == palette.blends[texel * 256 + 255],
            "Full-opacity water CLUT must ignore the stored luminance");
    }
    // Zero opacity selects pattern[luminance] for every texel column.
    const auto opaque = dfre::build_water_palette32(water_colors(), 0, {});
    for (unsigned luminance = 0; luminance < 256; ++luminance) {
        require(opaque.blends[luminance] == opaque.blends[63 * 256 + luminance],
            "Zero-opacity water CLUT must ignore the water texel");
    }
    rejects([&] { dfre::build_water_palette32(water_colors(), 0x10001, {}); });
}

void pass_fixture() {
    // Mirrors tools/water_probe.cpp case 0; the differential tool proves this
    // matches original 0x48D040 + 0x48D570 pixel for pixel.
    const std::uint32_t seed = 0;
    dfre::WaterScene32 scene;
    scene.level = 15 << 14;
    scene.opacity = 0x10000;
    scene.palette = dfre::build_water_palette32(water_colors(), scene.opacity, {});
    dfre::AssetBytes base(256 * 256);
    for (unsigned y = 0; y < 256; ++y) for (unsigned x = 0; x < 256; ++x)
        base[y * 256 + x] = static_cast<std::uint8_t>((x * 3 + y * 5 + seed * 7) & 63);
    scene.texels[0] = std::move(base);
    for (unsigned level = 1; level < scene.texels.size(); ++level)
        scene.texels[level] = dfre::terrain_sky_water_mip(scene.texels[level - 1], 256u >> (level - 1));
    dfre::VoxelCamera camera;
    camera.width = camera.row_stride = fixture_width;
    camera.height = fixture_height;
    camera.right = fixture_width - 1;
    camera.bottom = fixture_height - 1;
    camera.position = {5000, -3000, 10 * 65536};
    camera.yaw = 0x12345678;
    camera.pitch = 0xFF1E0000;
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(camera.row_stride) * camera.height);
    std::vector<std::uint16_t> depths(pixels.size());
    for (std::uint32_t row = 0; row < fixture_height; ++row) for (std::uint32_t x = 0; x < fixture_width; ++x) {
        const auto at = static_cast<std::size_t>(row) * fixture_width + x;
        pixels[at] = ((row * 37 + x * 11 + seed * 97) & 0xffffff) | 0xa3000000u;
        depths[at] = static_cast<std::uint16_t>(fixture_depth(seed, row, x));
    }
    dfre::WaterPass32 pass;
    pass.time = 5;
    pass.timer = 123;
    pass.row_depths.assign(fixture_height, dfre::water_row_no_terrain);
    const auto before = pixels;
    const auto before_depths = depths;
    dfre::render_water_frame32(scene, camera, pass, pixels, depths);
    require(depths == before_depths, "Water pass must not write the depth buffer");
    require(fingerprint(words_bytes(pixels)) == 0xd6a48392a00dd4edull, "Water pass pixels differ from the recorded original run");
    std::size_t writes = 0;
    for (std::size_t i = 0; i < pixels.size(); ++i) if (pixels[i] != before[i]) ++writes;
    require(writes == 866, "Water pass write count differs");

    // Disabled water and a plane above the camera are no-ops for 0x48D040.
    auto disabled = scene;
    disabled.level = 0;
    auto other = before;
    dfre::render_water_frame32(disabled, camera, pass, other, depths);
    require(other == before, "Disabled water must not touch pixels");
    auto above = scene;
    above.level = camera.position[2] + 1;
    dfre::render_water_pass32(above, camera, pass, other, depths);
    require(other == before, "0x48D040 must not touch pixels when the plane is at or above the camera");

    // A zero row budget skips every row of 0x48D040.
    auto blocked = pass;
    blocked.row_depths.assign(fixture_height, 0);
    dfre::render_water_pass32(scene, camera, blocked, other, depths);
    require(other == before, "Zero row depths must skip every water row");
}

void companion_fixture() {
    // Mirrors tools/water_probe.cpp case 14; the differential tool proves this
    // matches the original 0x48D040 then 0x48D570 frame order pixel for pixel.
    const std::uint32_t seed = 14;
    dfre::WaterScene32 scene;
    scene.level = 40 << 14;
    scene.opacity = 0x8000;
    scene.palette = dfre::build_water_palette32(water_colors(), scene.opacity, {});
    dfre::AssetBytes base(256 * 256);
    for (unsigned y = 0; y < 256; ++y) for (unsigned x = 0; x < 256; ++x)
        base[y * 256 + x] = static_cast<std::uint8_t>((x * 3 + y * 5 + seed * 7) & 63);
    scene.texels[0] = std::move(base);
    for (unsigned level = 1; level < scene.texels.size(); ++level)
        scene.texels[level] = dfre::terrain_sky_water_mip(scene.texels[level - 1], 256u >> (level - 1));
    dfre::VoxelCamera camera;
    camera.width = camera.row_stride = fixture_width;
    camera.height = fixture_height;
    camera.right = fixture_width - 1;
    camera.bottom = fixture_height - 1;
    camera.position = {5000 + seed * 101, -static_cast<std::int32_t>(3000 + seed * 57), 10 * 65536};
    camera.yaw = 0x12345678;
    camera.pitch = 0xFF1E0000;
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(camera.row_stride) * camera.height);
    std::vector<std::uint16_t> depths(pixels.size());
    for (std::uint32_t row = 0; row < fixture_height; ++row) for (std::uint32_t x = 0; x < fixture_width; ++x) {
        const auto at = static_cast<std::size_t>(row) * fixture_width + x;
        pixels[at] = ((row * 37 + x * 11 + seed * 97) & 0xffffff) | 0xa3000000u;
        depths[at] = static_cast<std::uint16_t>(fixture_depth(seed, row, x));
    }
    dfre::WaterPass32 pass;
    pass.time = 11;
    pass.timer = 211;
    pass.row_depths.assign(fixture_height, dfre::water_row_no_terrain);
    const auto before = pixels;
    const auto before_depths = depths;
    dfre::render_water_frame32(scene, camera, pass, pixels, depths);
    require(depths == before_depths, "Water companion must not write the depth buffer");
    require(fingerprint(words_bytes(pixels)) == 0xabd7e87cd675829dull, "Water companion pixels differ from the recorded original run");
    std::size_t writes = 0;
    for (std::size_t i = 0; i < pixels.size(); ++i) if (pixels[i] != before[i]) ++writes;
    require(writes == 4512, "Water companion write count differs");
    for (const auto& value : pixels) {
        require(std::find(scene.palette.blends.begin(), scene.palette.blends.end(), value) != scene.palette.blends.end(),
            "Water companion pixels must come from the blend table");
    }
    auto below = scene;
    below.level = camera.position[2] - 1;
    auto other = before;
    dfre::render_water_companion32(below, camera, pass, other, depths);
    require(other == before, "Companion must not touch pixels when the plane is below the camera");
}

void pass_edits() {
    const auto colors = water_colors();
    dfre::WaterScene32 scene;
    scene.level = 15 << 14;
    scene.opacity = 0x10000;
    scene.palette = dfre::build_water_palette32(colors, scene.opacity, {});
    for (unsigned level = 0; level < scene.texels.size(); ++level) {
        const auto width = 256u >> level;
        scene.texels[level].assign(static_cast<std::size_t>(width) * width, static_cast<std::uint8_t>(level));
    }
    dfre::VoxelCamera camera;
    camera.width = camera.row_stride = 32;
    camera.height = 24;
    camera.right = 31;
    camera.bottom = 23;
    camera.position = {0, 0, 10 * 65536};
    camera.pitch = 0xFF800000u;
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(camera.row_stride) * camera.height, 0x11aabbcc);
    std::vector<std::uint16_t> depths(pixels.size(), 0xffff);
    dfre::WaterPass32 pass;
    pass.row_depths.assign(camera.height, dfre::water_row_no_terrain);
    dfre::render_water_pass32(scene, camera, pass, pixels, depths);
    std::size_t writes = 0;
    for (auto value : pixels) {
        if (value == 0x11aabbcc) continue;
        ++writes;
        const auto allowed = std::find(scene.palette.blends.begin(), scene.palette.blends.end(), value);
        require(allowed != scene.palette.blends.end(), "Water pixels must come from the blend table");
    }
    require(writes > 0, "Tuned water fixture must draw pixels");

    std::vector<std::uint16_t> short_depths(1, 0xffff);
    rejects([&] { dfre::render_water_pass32(scene, camera, pass, pixels, short_depths); });
    auto short_rows = pass;
    short_rows.row_depths.pop_back();
    rejects([&] { dfre::render_water_pass32(scene, camera, short_rows, pixels, depths); });
    auto broken = scene;
    broken.texels[3].assign(16, 0);
    rejects([&] { dfre::render_water_pass32(broken, camera, pass, pixels, depths); });
    auto bad_camera = camera;
    bad_camera.focal_distance = 0;
    rejects([&] { dfre::render_water_pass32(scene, bad_camera, pass, pixels, depths); });
}

void row_depths() {
    // Mirrors tools/water_probe.cpp --row-depths-fixture seed 3; the
    // differential tool proves this matches original 0x489260.
    dfre::VoxelCamera camera;
    camera.width = camera.row_stride = 96;
    camera.height = 48;
    camera.right = 95;
    const auto seed = 3u;
    const auto bottom = 24 + seed % 24, top = seed % 8;
    camera.bottom = static_cast<std::int32_t>(bottom);
    camera.top = static_cast<std::int32_t>(top);
    std::vector<dfre::WaterTerrainRowDepth> entries;
    for (std::uint32_t i = 0, count = seed % 5; i < count; ++i) {
        entries.push_back({static_cast<std::int32_t>((seed * 7 + i * 11) % (bottom + 1)),
            0x1000 + (seed * 971 + i * 131) % 0x7d00000});
    }
    const auto rows = dfre::build_water_row_depths(entries, camera);
    require(rows.size() == 48, "Row depth count differs");
    require(fingerprint(words_bytes(rows)) == 0x4306ee70fb05f11dull, "Row depths differ from the recorded original run");
    require(rows[0] == dfre::water_row_no_terrain && rows[47] == dfre::water_row_no_terrain,
        "Rows outside the covered region must stay unset");
    // Entry 0 covers rows above its top row down from the window bottom.
    const auto first = entries.front();
    for (std::int32_t row = first.top_row + 1; row <= camera.bottom; ++row) {
        require(rows[static_cast<std::size_t>(row)] == first.depth, "First entry must cover rows above its top row");
    }
    // An empty entry list marks the whole window as without terrain.
    const auto empty = dfre::build_water_row_depths({}, camera);
    require(std::all_of(empty.begin(), empty.end(),
        [](std::uint32_t value) { return value == dfre::water_row_no_terrain; }), "Empty entries must clear the window");
    rejects([&] {
        dfre::build_water_row_depths({{-1, 0x1000}}, camera);
    });
}

void scene_preparation() {
    dfre::AssetBytes heights(1024 * 1024), characters(1024 * 1024);
    std::vector<std::uint32_t> detail(65536);
    dfre::TerrainGeometry geometry(std::move(heights), std::move(characters), std::move(detail));
    dfre::PcxImage water, water_palette;
    water.width = water.height = 256;
    water.pixels.resize(256 * 256);
    for (std::size_t i = 0; i < water.pixels.size(); ++i) water.pixels[i] = static_cast<std::uint8_t>(i % 64);
    water_palette.palette.resize(768);
    for (std::size_t i = 0; i < water_palette.palette.size(); ++i) water_palette.palette[i] = static_cast<std::uint8_t>(i * 3);
    dfre::TerrainScene32 scene{dfre::TerrainRuntime{std::move(geometry), {}, {}, {}, {}, {}, std::move(water),
        std::move(water_palette), 15 << 14, 32}, {}, {}, {}, {}, {}, 1236, 65536};
    scene.terrain.config.fields.push_back({std::string("water_opacity"), {std::string("0.3")}});
    scene.terrain.config.fields.push_back({std::string("turbidity"), {std::string("0.25")}});
    const auto prepared = dfre::prepare_water_scene32(scene);
    require(prepared.level == 15 << 14, "Prepared water level differs");
    require(prepared.opacity == 19661, "Water opacity must parse as fixed 16.16");
    require(prepared.scroll_shift == 0, "Water scroll shift uses the low turbidity byte");
    require(prepared.texels[7].size() == 4 && prepared.palette.blends.size() == 64 * 256,
        "Prepared water assets are incomplete");

    auto disabled = scene;
    disabled.terrain.water_height = 0;
    require(dfre::prepare_water_scene32(disabled).level == 0, "Zero water height must stay disabled");
    auto missing = scene;
    missing.terrain.water.reset();
    rejects([&] { dfre::prepare_water_scene32(missing); });
    auto wrong_bits = scene;
    wrong_bits.terrain.pixel_bits = 8;
    rejects([&] { dfre::prepare_water_scene32(wrong_bits); });
}
} // namespace

int main() {
    try {
        wave_table();
        clut_table();
        pass_fixture();
        companion_fixture();
        pass_edits();
        row_depths();
        scene_preparation();
        std::cout << "native_water passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
