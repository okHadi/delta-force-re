#include "dfre/water.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
std::string fingerprint(const dfre::AssetBytes& bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (auto byte : bytes) hash = (hash ^ byte) * 0x100000001b3ull;
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

dfre::AssetBytes words_bytes(const std::vector<std::uint32_t>& words) {
    dfre::AssetBytes bytes;
    bytes.reserve(words.size() * 4);
    for (auto word : words) for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(word >> shift));
    return bytes;
}

// Fixture data mirrors tools/check_water_original.py exactly.
dfre::AssetBytes water_colors() {
    dfre::AssetBytes colors(17 * 3);
    for (unsigned i = 0; i < 17; ++i) for (unsigned channel = 0; channel < 3; ++channel) {
        colors[i * 3 + channel] = static_cast<std::uint8_t>(i * 23 + channel * 41 + 7);
    }
    return colors;
}

dfre::TerrainColorControl fixture_control(std::uint32_t seed) {
    dfre::TerrainColorControl control;
    if (seed / 3 == 1) { control.brightness = 64; control.saturation = 0; control.channels = {255, 64, 192}; }
    if (seed / 3 == 2) { control.brightness = 255; control.saturation = 255; control.channels = {255, 128, 1}; }
    return control;
}

std::uint32_t clut_opacity(std::uint32_t seed) {
    static const std::uint32_t values[] = {0x10000, 0x4000, 0, 0xC000, 0x2000, 0x10000};
    return values[seed % 6];
}

void wave32_fixture(std::uint32_t seed) {
    const auto table = dfre::build_water_wave_table(seed);
    dfre::AssetBytes bytes;
    bytes.reserve(table.size() * 4);
    for (auto value : table) for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(std::uint32_t(value) >> shift));
    std::cout << "{\"shift\":" << seed << ",\"table\":\"" << fingerprint(bytes) << "\"}\n";
}

void clut32_fixture(std::uint32_t seed) {
    const auto colors = water_colors();
    const auto palette = dfre::build_water_palette32(colors, clut_opacity(seed), fixture_control(seed));
    std::cout << "{\"seed\":" << seed << ",\"clut\":\"" << fingerprint(words_bytes(palette.blends)) << "\",\"first\":[";
    for (unsigned i = 0; i < 8; ++i) { if (i) std::cout << ','; std::cout << palette.blends[i]; }
    std::cout << "]}\n";
}

// Pass fixture cases mirror tools/check_water_original.py.
struct WaterCase {
    std::uint32_t seed, yaw, pitch, time, timer, level, camz, row_mode;
    int top, bottom, left, right;
};

const WaterCase water_cases[] = {
    {0, 0x12345678, 0xFF1E0000, 5, 123, 15, 10, 0, 0, 47, 0, 93},
    {1, 0x12345678, 0x02000000, 0, 131, 15, 10, 0, 0, 47, 0, 93},
    {2, 0x0A5A5A5B, 0xFE800000, 9, 139, 28, 12, 1, 0, 47, 0, 93},
    {3, 0x0A5A5A5B, 0xFD400000, 17, 149, 10, 20, 0, 4, 43, 8, 85},
    {4, 0x7fffffff, 0x01000000, 33, 157, 5, 9, 2, 2, 45, 16, 79},
    {5, 0x20000000, 0x00000000, 1, 163, 15, 11, 1, 0, 47, 0, 63},
    {6, 0x40000000, 0xFF000000, 64, 167, 15, 10, 1, 0, 47, 0, 93},
    {7, 0x12345678, 0xFC000000, 5, 173, 15, 10, 2, 6, 41, 8, 85},
    {8, 0x30000000, 0x00800000, 200, 179, 15, 10, 1, 0, 47, 0, 93},
    {9, 0x12345678, 0x04000000, 7, 181, 15, 10, 0, 0, 47, 2, 93},
    {10, 0x12345678, 0x04000000, 5, 191, 0, 10, 0, 0, 47, 0, 93},
    {11, 0x12345678, 0x04000000, 5, 193, 15, 3, 0, 0, 47, 0, 93},
    {12, 0x5555AAAA, 0xFF800000, 1234567, 197, 15, 10, 0, 0, 47, 0, 93},
    {13, 0x13572468, 0xFEC00000, 97, 199, 28, 12, 1, 0, 47, 0, 93},
    {14, 0x12345678, 0xFF1E0000, 11, 211, 40, 10, 0, 0, 47, 0, 93},
    {15, 0x0A5A5A5B, 0x01000000, 23, 223, 40, 10, 1, 0, 47, 0, 93},
    {16, 0x5555AAAA, 0x02000000, 41, 227, 60, 12, 0, 4, 43, 8, 85},
    {17, 0x13572468, 0xFE000000, 59, 229, 15, 15, 2, 2, 45, 16, 79},
    {18, 0x12345678, 0xFF800000, 67, 233, 10, 10, 0, 0, 47, 0, 93},
    {19, 0x2468ACE0, 0x00800000, 83, 239, 60, 12, 1, 0, 47, 2, 93},
};

constexpr std::uint32_t fixture_width = 94, fixture_height = 48;

std::uint32_t fixture_depth(std::uint32_t seed, std::uint32_t row, std::uint32_t x) {
    if ((row + x + seed) % 9 == 0) return 0xffff;
    if ((row * 5 + x + seed) % 7 == 0) return (seed * 971 + row * 131 + x * 17) & 0x0fff;
    return (seed * 971 + row * 131 + x * 17) & 0xffff;
}

std::uint32_t fixture_row_depth(const WaterCase& item, std::uint32_t row) {
    if (item.row_mode == 1) return (row + item.seed) % 5 ? 0x7d00000 : 1 + (row * 13 + item.seed) % 400;
    if (item.row_mode == 2) return (row + item.seed) % 3 ? 0x7d00000 : (row * 997 + item.seed) % 0x2fff000;
    return 0x7d00000;
}

void water32_fixture(std::uint32_t seed) {
    const auto& item = water_cases[seed % 20];
    dfre::WaterScene32 scene;
    scene.level = static_cast<std::int32_t>(item.level << 14);
    static const std::uint32_t water32_opacities[] = {0x10000, 0xC000, 0x8000, 0x4000};
    scene.opacity = water32_opacities[seed % 4];
    const auto control = fixture_control(seed);
    scene.palette = dfre::build_water_palette32(water_colors(), scene.opacity, control);
    dfre::AssetBytes base(256 * 256);
    for (unsigned y = 0; y < 256; ++y) for (unsigned x = 0; x < 256; ++x)
        base[y * 256 + x] = static_cast<std::uint8_t>((x * 3 + y * 5 + item.seed * 7) & 63);
    scene.texels[0] = std::move(base);
    for (unsigned level = 1; level < scene.texels.size(); ++level)
        scene.texels[level] = dfre::terrain_sky_water_mip(scene.texels[level - 1], 256u >> (level - 1));
    dfre::VoxelCamera camera;
    camera.width = camera.row_stride = fixture_width;
    camera.height = fixture_height;
    camera.left = item.left; camera.top = item.top; camera.right = item.right; camera.bottom = item.bottom;
    camera.position = {static_cast<std::int32_t>(5000 + item.seed * 101),
        static_cast<std::int32_t>(0) - static_cast<std::int32_t>(3000 + item.seed * 57),
        static_cast<std::int32_t>(item.camz * 65536)};
    camera.yaw = item.yaw;
    camera.pitch = item.pitch;
    camera.focal_distance = 320;
    camera.field_of_view = 90;
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(camera.row_stride) * camera.height);
    std::vector<std::uint16_t> depths(pixels.size());
    for (std::uint32_t row = 0; row < fixture_height; ++row) for (std::uint32_t x = 0; x < fixture_width; ++x) {
        const auto at = static_cast<std::size_t>(row) * fixture_width + x;
        pixels[at] = ((row * 37 + x * 11 + item.seed * 97) & 0xffffff) | 0xa3000000u;
        depths[at] = static_cast<std::uint16_t>(fixture_depth(item.seed, row, x));
    }
    dfre::WaterPass32 pass;
    pass.time = item.time;
    pass.timer = item.timer;
    pass.row_depths.resize(fixture_height);
    for (std::uint32_t row = 0; row < fixture_height; ++row) pass.row_depths[row] = fixture_row_depth(item, row);
    const auto before = pixels;
    dfre::render_water_frame32(scene, camera, pass, pixels, depths);
    std::size_t writes = 0;
    for (std::size_t i = 0; i < pixels.size(); ++i) if (pixels[i] != before[i]) ++writes;
    dfre::AssetBytes depth_bytes;
    depth_bytes.reserve(depths.size() * 2);
    for (auto value : depths) {
        depth_bytes.push_back(static_cast<std::uint8_t>(value));
        depth_bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    }
    std::cout << "{\"seed\":" << item.seed << ",\"pixels\":\"" << fingerprint(words_bytes(pixels))
              << "\",\"depths\":\"" << fingerprint(depth_bytes)
              << "\",\"writes\":" << writes << "}\n";
}

void row_depths_fixture(std::uint32_t seed) {
    // Mirrors tools/check_water_original.py: original 0x489260 cache fields.
    const auto bottom = 24 + seed % 24, top = seed % 8;
    dfre::VoxelCamera camera;
    camera.width = camera.row_stride = 96;
    camera.height = 48;
    camera.right = 95;
    camera.bottom = bottom;
    camera.top = top;
    std::vector<dfre::WaterTerrainRowDepth> entries;
    for (std::uint32_t i = 0, count = seed % 5; i < count; ++i) {
        dfre::WaterTerrainRowDepth entry;
        entry.top_row = static_cast<std::int32_t>((seed * 7 + i * 11) % (bottom + 1));
        entry.depth = 0x1000 + (seed * 971 + i * 131) % 0x7d00000;
        entries.push_back(entry);
    }
    const auto rows = dfre::build_water_row_depths(entries, camera);
    std::cout << "{\"seed\":" << seed << ",\"rows\":\"" << fingerprint(words_bytes(rows)) << "\",\"count\":"
              << entries.size() << ",\"top\":" << top << ",\"bottom\":" << bottom << "}\n";
}

void frame32(const std::string& directory, const std::string& mission, const std::string& output) {
    const dfre::AssetStore assets(directory);
    const auto gamma = dfre::load_terrain_gamma(std::filesystem::path(directory) / "INTRO.KDV");
    auto runtime = dfre::load_terrain_runtime(assets, mission, 32);
    const auto scene = dfre::prepare_terrain_scene32(std::move(runtime), gamma);
    auto water = dfre::prepare_water_scene32(scene);
    dfre::VoxelCamera camera;
    // Demo camera over the lowest terrain area, above the water plane.
    std::int32_t lowest = std::numeric_limits<std::int32_t>::max();
    std::array<std::int32_t, 2> position{};
    for (std::int32_t y = 0; y < 1024; y += 16) for (std::int32_t x = 0; x < 1024; x += 16) {
        const auto height = scene.terrain.geometry.nearest_height(x << 16, y << 16);
        if (height < lowest) { lowest = height; position = {x << 16, y << 16}; }
    }
    camera.position[0] = position[0]; camera.position[1] = position[1];
    camera.position[2] = std::max(lowest, water.level) + 2 * 65536;
    camera.pitch = 0xFF800000u;
    auto frame = dfre::render_terrain_frame32(scene, camera);
    // Probe-local inferred row depths for the demo image only. Exact row
    // depths come from terrain cache entries through build_water_row_depths.
    std::vector<std::uint32_t> row_depths(camera.height, dfre::water_row_no_terrain);
    for (int row = camera.top; row <= camera.bottom; ++row) {
        std::uint16_t nearest = 0xffff;
        const auto* depth_row = frame.depths.data() + std::size_t(row) * camera.row_stride + camera.left;
        for (int x = camera.left; x <= camera.right; ++x) nearest = std::min(nearest, depth_row[x - camera.left]);
        row_depths[static_cast<std::size_t>(row)] = std::uint32_t(nearest) << 10;
    }
    const auto before = frame.pixels;
    dfre::WaterPass32 pass;
    pass.row_depths = row_depths;
    dfre::render_water_frame32(water, camera, pass, frame.pixels, frame.depths);
    std::size_t writes = 0;
    for (std::size_t i = 0; i < frame.pixels.size(); ++i) if (frame.pixels[i] != before[i]) ++writes;
    if (output.empty()) {
        std::cout << "{\"mission\":\"" << mission << "\",\"level\":" << water.level << ",\"writes\":" << writes
                  << ",\"pixels\":\"" << fingerprint(words_bytes(frame.pixels)) << "\"}\n";
        return;
    }
    const auto absolute = std::filesystem::absolute(output);
    const auto relative = std::filesystem::relative(std::filesystem::canonical(absolute.parent_path()),
        std::filesystem::canonical(".local"));
    if ((!relative.empty() && *relative.begin() == "..") || std::filesystem::exists(absolute))
        throw dfre::AssetError("Frame export requires a new file under .local");
    std::ofstream image(absolute, std::ios::binary);
    if (!image) throw dfre::AssetError("Cannot create water image");
    image << "P6\n" << camera.width << ' ' << camera.height << "\n255\n";
    for (auto value : frame.pixels) {
        const char rgb[]{static_cast<char>(value >> 16), static_cast<char>(value >> 8), static_cast<char>(value)};
        image.write(rgb, 3);
    }
    if (!image) throw dfre::AssetError("Cannot write water image");
    std::cout << "{\"mission\":\"" << mission << "\",\"level\":" << water.level << ",\"writes\":" << writes
              << ",\"pixels\":\"" << fingerprint(words_bytes(frame.pixels)) << "\",\"image\":\"" << absolute.string() << "\"}\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto argument = [&](int index) { return argc > index ? std::string_view(argv[index]) : std::string_view{}; };
        if (argc == 3 && (argument(1) == "--wave32-fixture" || argument(1) == "--clut32-fixture" ||
                          argument(1) == "--water32-fixture" || argument(1) == "--row-depths-fixture")) {
            std::uint32_t seed = 0;
            const std::string_view input(argv[2]);
            const auto parsed = std::from_chars(input.data(), input.data() + input.size(), seed);
            if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size())
                throw dfre::AssetError("Invalid water fixture seed");
            if (argument(1) == "--wave32-fixture") wave32_fixture(seed);
            else if (argument(1) == "--clut32-fixture") clut32_fixture(seed);
            else if (argument(1) == "--row-depths-fixture") row_depths_fixture(seed);
            else water32_fixture(seed);
            return 0;
        }
        if ((argc == 4 || argc == 5) && argument(1) == "--frame32") {
            frame32(argv[2], argv[3], argc == 5 ? argv[4] : std::string{});
            return 0;
        }
        throw dfre::AssetError("Usage: water_probe --wave32-fixture|--clut32-fixture|--water32-fixture|--row-depths-fixture <seed> | "
                               "--frame32 <game-dir> <mission.bms> [new-image.ppm]");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
