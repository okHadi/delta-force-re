#include "dfre/terrain.hpp"

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <charconv>

namespace {
std::string fingerprint(const dfre::AssetBytes& bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (auto byte : bytes) hash = (hash ^ byte) * 0x100000001b3ull;
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

dfre::TerrainGeometry synthetic() {
    dfre::AssetBytes heights(1024 * 1024), characters(heights.size());
    std::vector<std::uint32_t> detail(65536);
    for (std::uint32_t y = 0; y < 1024; ++y) for (std::uint32_t x = 0; x < 1024; ++x) {
        heights[y * 1024 + x] = static_cast<std::uint8_t>(x * 13 + y * 7 + (x ^ y));
        characters[y * 1024 + x] = static_cast<std::uint8_t>(x + y * 3);
    }
    for (std::uint32_t tile = 0; tile < 256; ++tile) for (std::uint32_t at = 0; at < 256; ++at) {
        detail[tile * 256 + at] = ((tile * 17 + at * 3) & 255) << 16;
    }
    return {std::move(heights), std::move(characters), std::move(detail)};
}

void samples(const dfre::TerrainGeometry& geometry) {
    std::int64_t x, y;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string extra;
        if (!(input >> x >> y) || (input >> extra)) throw dfre::AssetError("Expected one integer coordinate pair per line");
        if (x < std::numeric_limits<std::int32_t>::min() || x > std::numeric_limits<std::int32_t>::max() ||
            y < std::numeric_limits<std::int32_t>::min() || y > std::numeric_limits<std::int32_t>::max()) {
            throw dfre::AssetError("Sample coordinates must fit signed 32-bit integers");
        }
        const auto ix = static_cast<std::int32_t>(x), iy = static_cast<std::int32_t>(y);
        const auto slope = geometry.slope(ix, iy);
        std::cout << geometry.base_height(ix, iy) << ' ' << geometry.surface_height(ix, iy) << ' '
                  << geometry.nearest_height(ix, iy) << ' ' << slope[0] << ' ' << slope[1] << '\n';
    }
    if (std::cin.bad()) throw dfre::AssetError("Cannot read sample coordinates");
}

void raster_fixture(std::uint32_t seed) {
    auto state = seed;
    auto next = [&]() { state = state * 1664525u + 1013904223u; return state; };
    dfre::VoxelPass8 pass;
    pass.lod = seed % 10; pass.stop_depth = 7 + next() % 24; pass.row_stride = 24;
    pass.top_offset = static_cast<std::int32_t>(next() % 4) * 24;
    pass.step_scale = 65536 + static_cast<std::int32_t>(next() & 32767);
    pass.acceleration_step = {next() % 65536 - 32768, next() % 65536 - 32768, 0u - (65536 + next() % 65536)};
    const auto width = 1024u >> pass.lod;
    dfre::AssetBytes heights(width * width), colors(heights.size());
    for (auto& value : heights) value = static_cast<std::uint8_t>(next() >> 24);
    for (auto& value : colors) value = static_cast<std::uint8_t>(next() >> 24);
    std::vector<std::uint32_t> table(64);
    for (auto& value : table) value = next();
    std::vector<dfre::VoxelColumn> columns(12);
    for (std::size_t at = 0; at < columns.size(); ++at) {
        auto& column = columns[at];
        column.finished = at % 5 == 0 ? 1 : 0;
        column.pixel_offset = 15 * 24 + static_cast<std::int32_t>(at * 2);
        column.position = {next(), next(), (next() & 255) << 20};
        column.step = {next() % 0x1000000 - 0x800000, next() % 0x1000000 - 0x800000, next() % 0x800000 - 0x400000};
        column.acceleration = {next() % 65536 - 32768, next() % 65536 - 32768, 0u - (0x100000 + next() % 65536)};
    }
    columns[1].position[2] = 0x11000000; columns[1].step[2] = 0;
    dfre::AssetBytes pixels(24 * 16, 0xaa);
    std::vector<std::uint16_t> depths(pixels.size(), 0xa55a);
    dfre::render_voxel_pass_8(pass, heights, colors, table, columns, pixels, depths);
    constexpr char hex[] = "0123456789abcdef";
    std::cout << "{\"pixels\":\"";
    for (auto value : pixels) std::cout << hex[value >> 4] << hex[value & 15];
    std::cout << "\",\"depths\":[";
    for (std::size_t i = 0; i < depths.size(); ++i) { if (i) std::cout << ','; std::cout << depths[i]; }
    std::cout << "],\"columns\":[";
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i) std::cout << ',';
        const auto& column = columns[i];
        std::cout << '[' << column.depth_index << ',' << column.finished << ',' << column.pixel_offset;
        for (const auto& array : {column.position, column.step, column.acceleration}) for (auto value : array) std::cout << ',' << value;
        std::cout << ']';
    }
    std::cout << "]}\n";
}

void camera_fixture(std::uint32_t seed) {
    auto state = seed;
    auto next = [&]() { state = state * 1664525u + 1013904223u; return state; };
    dfre::VoxelCamera camera;
    camera.width = 320 + 160 * (seed % 4); camera.height = 200 + 40 * (seed % 5);
    camera.row_stride = camera.width; camera.right = static_cast<std::int32_t>(camera.width) - 1;
    camera.bottom = static_cast<std::int32_t>(camera.height) - 1;
    camera.focal_distance = static_cast<std::int32_t>(camera.width) / 2;
    for (auto& position : camera.position) position = static_cast<std::int32_t>(next() & 0x7fffffff) - 0x40000000;
    camera.yaw = next(); camera.pitch = next();
    if (seed < 4) { camera.yaw = seed * 0x40000000u; camera.pitch = 0; }
    const auto result = dfre::initialize_voxel_columns(camera);
    std::cout << "{\"acceleration\":[" << result.acceleration_step[0] << ',' << result.acceleration_step[1] << ','
              << result.acceleration_step[2] << "],\"columns\":[";
    for (std::size_t i = 0; i < result.columns.size(); ++i) {
        if (i) std::cout << ',';
        const auto& column = result.columns[i];
        std::cout << '[' << column.depth_index << ',' << column.finished << ',' << column.pixel_offset;
        for (const auto& array : {column.position, column.step, column.acceleration}) for (auto value : array) std::cout << ',' << value;
        std::cout << ']';
    }
    std::cout << "]}\n";
}

void projection_fixture(std::uint32_t seed) {
    const auto result = dfre::build_voxel_projection(80 + static_cast<std::int32_t>(seed % 32) * 17,
        seed % 2 ? 3.2f : 2.2f, seed % 3 ? 2.2f : 3.2f, seed % 5 ? 1.0f : 0.75f);
    std::cout << std::setprecision(17) << "{\"levels\":[";
    for (std::size_t i = 0; i < result.levels.size(); ++i) {
        if (i) std::cout << ',';
        const auto& row = result.levels[i];
        std::cout << '[' << row.detail << ',' << row.distance << ',' << row.world_bound << ',' << row.sample_step
                  << ',' << row.lod << ',' << row.ray_step << ',' << row.sample_limit << ',' << row.depth_end
                  << ',' << row.next_step_scale << ']';
    }
    std::cout << "],\"depths\":[";
    for (std::size_t i = 0; i < result.depths.size(); ++i) { if (i) std::cout << ','; std::cout << result.depths[i]; }
    std::cout << "]}\n";
}

void detail_fixture(std::uint32_t seed) {
    const auto base = synthetic();
    auto detail = base.detail_words();
    for (unsigned tile = 0; tile < 256; ++tile) for (unsigned at = 0; at < 256; ++at) {
        detail[tile * 256 + at] |= ((tile + at) & 255) | (((tile * 3 + at * 7) & 255) << 8);
    }
    const dfre::TerrainGeometry geometry(base.height_bytes(), base.character_bytes(), std::move(detail));
    dfre::AssetBytes color(1024 * 1024);
    for (unsigned y = 0; y < 1024; ++y) for (unsigned x = 0; x < 1024; ++x) color[y * 1024 + x] = static_cast<std::uint8_t>(x * 5 + y * 11);
    dfre::TerrainBlendTables tables;
    tables.blend.resize(65536); tables.shade.resize(16384); tables.selectors.resize(256);
    for (unsigned a = 0; a < 256; ++a) for (unsigned b = 0; b < 256; ++b) tables.blend[a * 256 + b] = static_cast<std::uint8_t>(a * 13 + b * 17);
    for (unsigned s = 0; s < 64; ++s) for (unsigned c = 0; c < 256; ++c) tables.shade[s * 256 + c] = static_cast<std::uint8_t>(s * 7 + c * 3);
    for (unsigned i = 0; i < 256; ++i) tables.selectors[i] = (i * 7) % 12;
    const auto x = seed * 1664525u + 1013904223u, y = x * 1664525u + 1013904223u;
    const auto result = dfre::build_terrain_detail_tile(geometry, color, tables, x, y);
    std::cout << "{\"height\":[";
    for (unsigned i = 0; i < 256; ++i) { if (i) std::cout << ','; std::cout << result.height[i]; }
    std::cout << "],\"color\":[";
    for (unsigned i = 0; i < 256; ++i) { if (i) std::cout << ','; std::cout << unsigned(result.color[i]); }
    std::cout << "]}\n";
}

void bounds_fixture() {
    const auto base = synthetic();
    auto detail = base.detail_words();
    for (unsigned tile = 0; tile < 256; ++tile) for (unsigned at = 0; at < 256; ++at) detail[tile * 256 + at] = tile << 16;
    const dfre::TerrainGeometry geometry(base.height_bytes(), base.character_bytes(), std::move(detail));
    const auto bounds = dfre::build_terrain_height_bounds(geometry);
    std::cout << "[";
    for (std::size_t i = 0; i < bounds.size(); ++i) { if (i) std::cout << ','; std::cout << '"' << fingerprint(bounds[i]) << '"'; }
    std::cout << "]\n";
}

void palette_fixture(std::uint32_t seed) {
    dfre::AssetBytes palette(768);
    for (unsigned i = 0; i < 256; ++i) {
        palette[i * 3] = static_cast<std::uint8_t>(seed == 1 ? i : seed == 2 ? 0 : i * 37);
        palette[i * 3 + 1] = static_cast<std::uint8_t>(seed == 1 ? i : seed == 2 ? 0 : i * 67);
        palette[i * 3 + 2] = static_cast<std::uint8_t>(seed == 1 ? i : seed == 2 ? 0 : i * 101);
    }
    const auto tables = dfre::build_terrain_blend_tables(palette, {230, 210, 190}, std::vector<std::uint32_t>(256, 0));
    std::cout << "{\"blend\":\"" << fingerprint(tables.blend) << "\",\"shade\":\"" << fingerprint(tables.shade) << "\"}\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--synthetic") {
            samples(synthetic());
        } else if (argc == 2 && std::string_view(argv[1]) == "--bounds-fixture") {
            bounds_fixture();
        } else if (argc == 3 && (std::string_view(argv[1]) == "--raster-fixture" || std::string_view(argv[1]) == "--camera-fixture" || std::string_view(argv[1]) == "--projection-fixture" || std::string_view(argv[1]) == "--detail-fixture" || std::string_view(argv[1]) == "--palette-fixture")) {
            std::uint32_t seed = 0;
            const std::string_view input(argv[2]);
            const auto parsed = std::from_chars(input.data(), input.data() + input.size(), seed);
            if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size()) throw dfre::AssetError("Invalid raster fixture seed");
            if (std::string_view(argv[1]) == "--raster-fixture") raster_fixture(seed);
            else if (std::string_view(argv[1]) == "--camera-fixture") camera_fixture(seed);
            else if (std::string_view(argv[1]) == "--projection-fixture") projection_fixture(seed);
            else if (std::string_view(argv[1]) == "--detail-fixture") detail_fixture(seed);
            else palette_fixture(seed);
        } else if (argc == 3 && std::string_view(argv[1]) == "--blend-sidecar") {
            const auto selectors = dfre::load_terrain_blend_selectors(argv[2]);
            std::cout << "{\"selectors\":" << selectors.size() << ",\"minimum\":";
            auto minimum = selectors[0], maximum = selectors[0];
            for (auto value : selectors) { if (value < minimum) minimum = value; if (value > maximum) maximum = value; }
            std::cout << minimum << ",\"maximum\":" << maximum << "}\n";
        } else if (argc == 3) {
            const dfre::AssetStore assets(argv[1]);
            if (std::string_view(argv[2]) == "--all") {
                std::cout << "{\"missions\":[";
                bool first = true;
                std::size_t total = 0;
                for (const auto& name : assets.missions()) {
                    const auto runtime = dfre::load_terrain_runtime(assets, name);
                    if (!first) std::cout << ',';
                    first = false;
                    std::cout << "{\"mission\":\"" << name << "\",\"height_fnv1a64\":\""
                              << fingerprint(runtime.geometry.height_bytes()) << "\",\"character_fnv1a64\":\""
                              << fingerprint(runtime.geometry.character_bytes()) << "\",\"color_fnv1a64\":\""
                              << fingerprint(runtime.color.pixels) << "\",\"water_height\":" << runtime.water_height
                              << ",\"sky\":" << (runtime.sky ? "true" : "false") << '}';
                    ++total;
                }
                std::cout << "],\"loaded\":" << total << "}\n";
            } else samples(dfre::load_terrain_runtime(assets, argv[2]).geometry);
        } else {
            std::cerr << "terrain_probe --synthetic | GAME_FOLDER BMS_MEMBER | GAME_FOLDER --all\n";
            return 1;
        }
        return std::cout ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "terrain: " << error.what() << '\n';
        return 1;
    }
}
