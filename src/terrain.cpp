#include "dfre/terrain.hpp"
#include "dfre/filename.hpp"
#include "dfre/models.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace dfre {
namespace {
constexpr std::size_t map_pixels = 1024 * 1024;
constexpr std::size_t detail_pixels = 256 * 256;

std::uint32_t cell(std::int32_t value) { return (static_cast<std::uint32_t>(value) >> 16) & 1023; }

std::int32_t floor_shift16(std::int64_t value) {
    return static_cast<std::int32_t>(value >= 0 ? value / 65536 : -((-value + 65535) / 65536));
}

void flip_rows(PcxImage& image, std::uint32_t block_height) {
    if (image.width == 0 || block_height == 0 || image.height % block_height ||
        image.pixels.size() != static_cast<std::uint64_t>(image.width) * image.height) {
        throw AssetError("Invalid terrain image rows");
    }
    for (std::uint32_t block = 0; block < image.height; block += block_height) {
        for (std::uint32_t y = 0; y < block_height / 2; ++y) {
            const auto top = static_cast<std::size_t>(block + y) * image.width;
            const auto bottom = static_cast<std::size_t>(block + block_height - 1 - y) * image.width;
            std::swap_ranges(image.pixels.begin() + static_cast<std::ptrdiff_t>(top),
                             image.pixels.begin() + static_cast<std::ptrdiff_t>(top + image.width),
                             image.pixels.begin() + static_cast<std::ptrdiff_t>(bottom));
        }
    }
}

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : c; }

bool same(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
        [](char x, char y) { return lower(x) == lower(y); });
}

std::string field(const TerrainConfig& config, std::string_view name) {
    const TerrainField* result = nullptr;
    for (const auto& item : config.fields) if (same(item.name, name)) {
        if (result || item.values.size() != 1) throw AssetError("Invalid terrain field: " + std::string(name));
        result = &item;
    }
    if (!result) throw AssetError("Missing terrain field: " + std::string(name));
    return result->values[0];
}

std::int32_t integer(const TerrainConfig& config, std::string_view name) {
    const auto text = field(config, name);
    std::int32_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw AssetError("Invalid terrain integer: " + std::string(name));
    }
    return value;
}

std::string image_name(const TerrainConfig& config, std::string_view name) {
    return set_asset_extension(field(config, name), "PCX");
}

std::optional<PcxImage> optional_image(const AssetStore& assets, const std::string& name) {
    std::size_t count = 0;
    for (const auto& archive : assets.archives()) for (const auto& entry : archive.entries()) {
        if (!entry.deleted && same(entry.name, name)) ++count;
    }
    if (count == 0) return std::nullopt;
    if (count != 1) throw AssetError("Duplicate terrain image: " + name);
    return assets.load_image(name).image;
}

std::vector<std::uint32_t> detail_words(const AssetStore& assets, const TerrainConfig& config, AssetBytes& palette) {
    std::vector<std::uint32_t> result(detail_pixels, 0);
    const char* names[] = {"detail_color", "detail_shade", "detail_elev"};
    for (unsigned part = 0; part < 3; ++part) {
        auto image = assets.load_image(image_name(config, names[part])).image;
        if (image.width != 16 || image.height == 0 || image.height > 4096 || image.height % 16) {
            throw AssetError("Expected terrain detail atlas width 16 and complete 16-row tiles");
        }
        flip_rows(image, 16);
        if (part == 0) palette = image.palette;
        for (std::size_t i = 0; i < image.pixels.size(); ++i) result[i] |= std::uint32_t(image.pixels[i]) << (part * 8);
    }
    return result;
}

void require_square(const AssetBytes& source, std::uint32_t width) {
    if (width < 2 || width > 1024 || width % 2 || source.size() != static_cast<std::uint64_t>(width) * width) {
        throw AssetError("Expected an even square terrain mip source");
    }
}
} // namespace

TerrainGeometry::TerrainGeometry(AssetBytes height, AssetBytes character, std::vector<std::uint32_t> detail)
    : height_(std::move(height)), character_(std::move(character)), detail_(std::move(detail)) {
    if (height_.size() != map_pixels || character_.size() != map_pixels || detail_.size() != detail_pixels) {
        throw AssetError("Expected 1024-square terrain maps and 65536 detail words");
    }
}

std::int32_t TerrainGeometry::base_height(std::int32_t x, std::int32_t y) const {
    const auto ix = cell(x), iy = cell(y);
    const auto next_x = (ix + 1) & 1023, next_y = (iy + 1) & 1023;
    const auto fx = static_cast<std::uint32_t>(x) & 65535, fy = static_cast<std::uint32_t>(y) & 65535;
    const std::int32_t a = height_[iy * 1024 + ix] * 256;
    const std::int32_t b = height_[iy * 1024 + next_x] * 256;
    const auto left = a + floor_shift16(std::int64_t(fy) * (height_[next_y * 1024 + ix] * 256 - a));
    const auto right = b + floor_shift16(std::int64_t(fy) * (height_[next_y * 1024 + next_x] * 256 - b));
    return (left + floor_shift16(std::int64_t(fx) * (right - left))) * 64;
}

std::int32_t TerrainGeometry::surface_height(std::int32_t x, std::int32_t y) const {
    const auto index = cell(y) * 1024 + cell(x);
    const auto local = ((static_cast<std::uint32_t>(x) & 65535) >> 12) +
                       ((static_cast<std::uint32_t>(y) & 0xf000) >> 8);
    const auto elevation = (detail_[std::uint32_t(character_[index]) * 256 + local] >> 16) & 255;
    return base_height(x, y) + (elevation > 128 ? static_cast<std::int32_t>(elevation - 128) * 1024 : 0);
}

std::int32_t TerrainGeometry::nearest_height(std::int32_t x, std::int32_t y) const {
    return height_[cell(y) * 1024 + cell(x)] * 16384;
}

std::array<std::int32_t, 2> TerrainGeometry::slope(std::int32_t x, std::int32_t y) const {
    const auto ix = cell(x), iy = cell(y);
    return {(static_cast<std::int32_t>(height_[iy * 1024 + ((ix + 2) & 1023)]) -
             height_[iy * 1024 + ((ix - 2) & 1023)]) * 4096,
            (static_cast<std::int32_t>(height_[((iy + 2) & 1023) * 1024 + ix]) -
             height_[((iy - 2) & 1023) * 1024 + ix]) * 4096};
}

TerrainRuntime load_terrain_runtime(const AssetStore& assets, std::string_view mission, unsigned pixel_bits) {
    if (pixel_bits != 8 && pixel_bits != 32) throw AssetError("Terrain pixel depth must be 8 or 32");
    auto data = assets.load_mission(mission);
    for (auto* image : {&data.color.image, &data.elevation.image, &data.detail.image}) {
        if (image->width != 1024 || image->height != 1024) throw AssetError("Terrain maps must be 1024-square");
        flip_rows(*image, 1024);
    }
    AssetBytes palette;
    auto details = detail_words(assets, data.config, palette);
    TerrainGeometry geometry(std::move(data.elevation.image.pixels), std::move(data.detail.image.pixels), std::move(details));
    auto sky_palette = assets.load_image(image_name(data.config, "sky_palette")).image;
    if (sky_palette.palette.size() != 768) throw AssetError("Terrain sky gradient requires an RGB palette");
    auto sky = optional_image(assets, image_name(data.config, "sky_map"));
    if (sky) {
        if (sky->width != 512) throw AssetError("Terrain sky map width must be 512");
        for (auto& value : sky->pixels) {
            value = value < 128 ? 0 : static_cast<std::uint8_t>(value - 128);
            value >>= pixel_bits == 8 ? 2 : 1;
            if (value == (pixel_bits == 8 ? 31 : 63)) --value;
        }
    }
    const bool has_water_height = std::any_of(data.config.fields.begin(), data.config.fields.end(),
        [](const TerrainField& item) { return same(item.name, "water_height"); });
    const auto configured_water = has_water_height ? std::int64_t(integer(data.config, "water_height")) * 16384 : 0;
    if (configured_water < std::numeric_limits<std::int32_t>::min() ||
        configured_water > std::numeric_limits<std::int32_t>::max()) throw AssetError("Terrain water height overflows");
    auto water_height = static_cast<std::int32_t>(configured_water);
    std::optional<PcxImage> water_palette, water;
    if (water_height != 0) {
        water_palette = optional_image(assets, image_name(data.config, "water_palette"));
        if (water_palette && water_palette->palette.size() == 768) {
            water = optional_image(assets, image_name(data.config, "water_map"));
            if (water) {
                if (water->width != 256) throw AssetError("Terrain water map width must be 256");
                for (auto& value : water->pixels) value >>= pixel_bits == 8 ? 4 : 2;
            }
        }
        if (!water || !water_palette) water_height = 0;
    }
    return {std::move(geometry), std::move(data.config), std::move(data.color.image), std::move(palette),
            std::move(sky_palette), std::move(sky), std::move(water), std::move(water_palette), water_height, pixel_bits};
}

AssetBytes load_terrain_gamma(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw AssetError("Cannot open terrain gamma file");
    AssetBytes gamma(256);
    if (!input.read(reinterpret_cast<char*>(gamma.data()), 256)) throw AssetError("Terrain gamma file requires 256 bytes");
    return gamma;
}

TerrainPalette8 make_terrain_palette8(const AssetBytes& palette, const AssetBytes& sky_palette, const AssetBytes& gamma) {
    if (palette.size() != 768 || sky_palette.size() != 768 || gamma.size() != 256) {
        throw AssetError("Terrain palette requires two 256-color RGB palettes and a 256-byte gamma table");
    }
    TerrainPalette8 result;
    result.raw_rgb = palette;
    result.gamma_rgb.resize(palette.size());
    std::transform(palette.begin(), palette.end(), result.gamma_rgb.begin(), [&](std::uint8_t value) { return gamma[value]; });
    for (unsigned channel = 0; channel < 3; ++channel) {
        result.fog[channel] = gamma[sky_palette[channel]];
        result.light[channel] = gamma[sky_palette[48 + channel]];
    }
    return result;
}

TerrainPalette8 load_terrain_palette8(const AssetStore& assets, std::string_view mission,
    const TerrainRuntime& terrain, const AssetBytes& gamma) {
    const auto data = assets.read(mission);
    mission_terrain_name(data.bytes);
    if (data.bytes.size() < 268) throw AssetError("Truncated mission palette header");
    const auto name = (data.bytes[122] & 0x10) ? std::string("night.PAL") : set_asset_extension(mission, "PAL");
    const auto image = assets.load_image(name);
    auto result = make_terrain_palette8(image.image.palette, terrain.sky_palette.palette, gamma);
    result.source = image.source;
    return result;
}

AssetBytes terrain_height_mip(const AssetBytes& source, std::uint32_t width) {
    require_square(source, width);
    AssetBytes result;
    result.reserve(source.size() / 4);
    for (std::uint32_t y = 0; y < width; y += 2) for (std::uint32_t x = 0; x < width; x += 2) {
        const auto at = static_cast<std::size_t>(y) * width + x;
        result.push_back(std::max({source[at], source[at + 1], source[at + width], source[at + width + 1]}));
    }
    return result;
}

AssetBytes terrain_sky_water_mip(const AssetBytes& source, std::uint32_t width) {
    require_square(source, width);
    AssetBytes result;
    result.reserve(source.size() / 4);
    for (std::uint32_t y = 0; y < width; y += 2) for (std::uint32_t x = 0; x < width; x += 2) {
        const auto at = static_cast<std::size_t>(y) * width + x;
        const unsigned average = (source[at] + source[at + 1] + source[at + width] + source[at + width + 1] + 2) >> 2;
        result.push_back(static_cast<std::uint8_t>(average ? average - 1 : 0));
    }
    return result;
}

std::vector<std::uint32_t> decode_terrain_blend_selectors(const AssetBytes& bytes) {
    if (bytes.size() != 1024) throw AssetError("Expected a 1024-byte terrain blend pattern");
    std::vector<std::uint32_t> result;
    result.reserve(256);
    for (std::size_t i = 0; i < bytes.size(); i += 4) {
        const auto value = std::uint32_t(bytes[i]) | (std::uint32_t(bytes[i + 1]) << 8) |
            (std::uint32_t(bytes[i + 2]) << 16) | (std::uint32_t(bytes[i + 3]) << 24);
        if (value > 11) throw AssetError("Terrain blend selector must be 0 through 11");
        result.push_back(value);
    }
    return result;
}

std::vector<AssetBytes> build_terrain_height_bounds(const TerrainGeometry& geometry) {
    std::array<unsigned, 256> tile_max{};
    for (std::size_t tile = 0; tile < 256; ++tile) for (std::size_t i = 0; i < 256; ++i) {
        tile_max[tile] = std::max(tile_max[tile], unsigned((geometry.detail_words()[tile * 256 + i] >> 16) & 255));
    }
    AssetBytes bound(map_pixels);
    const auto& height = geometry.height_bytes();
    const auto& character = geometry.character_bytes();
    auto upper = [&](std::size_t at) {
        const auto detail = tile_max[character[at]];
        return std::min(255u, unsigned(height[at]) + (detail > 128 ? (detail - 128 + 15) / 16 : 0));
    };
    for (unsigned y = 0; y < 1024; ++y) for (unsigned x = 0; x < 1024; ++x) {
        const auto next_x = (x + 1) & 1023, next_y = (y + 1) & 1023;
        bound[y * 1024 + x] = static_cast<std::uint8_t>(std::max({upper(y * 1024 + x), upper(y * 1024 + next_x),
            upper(next_y * 1024 + x), upper(next_y * 1024 + next_x)}));
    }
    std::vector<AssetBytes> result;
    result.push_back(std::move(bound));
    for (unsigned width = 1024; width > 2; width >>= 1) result.push_back(terrain_height_mip(result.back(), width));
    return result;
}

std::vector<std::uint32_t> load_terrain_blend_selectors(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw AssetError("Cannot open terrain blend pattern");
    AssetBytes bytes(1024);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), 1024) || input.peek() != std::char_traits<char>::eof()) {
        throw AssetError("Expected a 1024-byte terrain blend pattern file");
    }
    if (input.bad()) throw AssetError("Cannot read terrain blend pattern");
    return decode_terrain_blend_selectors(bytes);
}

TerrainBlendTables build_terrain_blend_tables(const AssetBytes& gamma_palette,
    const std::array<std::uint8_t, 3>& light_color, std::vector<std::uint32_t> selectors) {
    if (gamma_palette.size() != 768 || selectors.size() != 256 ||
        std::any_of(selectors.begin(), selectors.end(), [](std::uint32_t value) { return value > 11; })) {
        throw AssetError("Invalid terrain palette or blend selectors");
    }
    std::array<std::uint16_t, 4096> starts{};
    std::array<std::uint8_t, 4096> counts{};
    AssetBytes candidates(0x20000, 0);
    std::size_t candidate_count = 0;
    auto distance = [&](int r, int g, int b, unsigned index) {
        const int dr = r - gamma_palette[index * 3], dg = g - gamma_palette[index * 3 + 1],
            db = b - gamma_palette[index * 3 + 2];
        return dr * dr * 900 + dg * dg * 2025 + db * db * 625;
    };
    for (unsigned r = 0; r < 16; ++r) for (unsigned g = 0; g < 16; ++g) for (unsigned b = 0; b < 16; ++b) {
        const auto bin = r * 256 + g * 16 + b;
        std::array<int, 256> distances{};
        int minimum = std::numeric_limits<int>::max();
        for (unsigned i = 1; i < 256; ++i) {
            distances[i] = distance(int(r * 16 + 8), int(g * 16 + 8), int(b * 16 + 8), i);
            minimum = std::min(minimum, distances[i] + 0x4633e);
        }
        starts[bin] = static_cast<std::uint16_t>(candidate_count);
        for (unsigned i = 1; i < 256; ++i) if (std::max(0, distances[i] - 0x4633e) <= minimum && candidate_count < candidates.size()) {
            candidates[candidate_count++] = static_cast<std::uint8_t>(i);
            ++counts[bin];
        }
    }
    auto nearest = [&](unsigned r, unsigned g, unsigned b) {
        const auto bin = (r >> 4) * 256 + (g >> 4) * 16 + (b >> 4);
        int best_distance = 0x0fffffff;
        std::uint8_t best = 0;
        for (unsigned i = 0; i < counts[bin]; ++i) {
            const auto index = candidates[std::size_t(starts[bin]) + i];
            const auto current = distance(int(r), int(g), int(b), index);
            if (current < best_distance) { best = index; best_distance = current; }
        }
        return best;
    };
    TerrainBlendTables result{AssetBytes(65536), AssetBytes(16384), std::move(selectors)};
    for (unsigned a = 0; a < 256; ++a) for (unsigned b = 0; b < 256; ++b) {
        auto mixed = nearest((unsigned(gamma_palette[a * 3]) + gamma_palette[b * 3]) >> 1,
            (unsigned(gamma_palette[a * 3 + 1]) + gamma_palette[b * 3 + 1]) >> 1,
            (unsigned(gamma_palette[a * 3 + 2]) + gamma_palette[b * 3 + 2]) >> 1);
        if (mixed == a || mixed == b) mixed = static_cast<std::uint8_t>(std::min(a, b));
        result.blend[a * 256 + b] = a == 0 ? static_cast<std::uint8_t>(b) : b == 0 ? static_cast<std::uint8_t>(a) : mixed;
    }
    for (unsigned color = 0; color < 256; ++color) {
        for (unsigned mode = 0; mode < 2; ++mode) {
            std::array<std::int32_t, 3> value{}, step{};
            for (unsigned channel = 0; channel < 3; ++channel) {
                const auto from = gamma_palette[color * 3 + channel];
                const int to = mode ? light_color[channel] : 0;
                value[channel] = int(from) * 65536;
                step[channel] = (to - from) * 0x842;
            }
            for (unsigned shade = 0; shade < 32; ++shade) {
                const auto row = mode ? shade + 32 : 31 - shade;
                result.shade[row * 256 + color] = nearest(unsigned(value[0]) >> 16, unsigned(value[1]) >> 16, unsigned(value[2]) >> 16);
                for (unsigned channel = 0; channel < 3; ++channel) value[channel] += step[channel];
            }
        }
    }
    return result;
}

std::vector<std::uint32_t> build_terrain_shades32(const AssetBytes& gamma_palette,
    const std::array<std::uint8_t, 3>& light_color, const TerrainColorControl& control) {
    if (gamma_palette.size() != 768 || control.brightness < 0 || control.brightness > 255 ||
        control.saturation < 0 || control.saturation > 255 || std::any_of(control.channels.begin(), control.channels.end(),
        [](int value) { return value < 0 || value > 255; })) throw AssetError("Invalid terrain 32-bit color controls");
    std::array<std::uint8_t, 256> inverse{};
    std::array<std::array<std::uint8_t, 256>, 3> channels{};
    for (unsigned i = 0; i < 256; ++i) {
        inverse[i] = static_cast<std::uint8_t>(std::pow(double(i) / 255.0, 0.8) * 255.0 + 0.5);
        const int curve = static_cast<int>(std::sqrt(255.0 * i) - i + 0.5);
        const int factor = control.brightness >= 128 ? (control.brightness - 128) * 2 : std::min(255, (128 - control.brightness) * 2);
        const int base = std::max(0, int(i) + (control.brightness >= 128 ? 1 : -1) * ((curve * factor) >> 8));
        for (unsigned channel = 0; channel < 3; ++channel) {
            channels[channel][i] = static_cast<std::uint8_t>(std::min(255, ((base * control.channels[channel]) >> 8) * 2));
        }
    }
    auto floor128 = [](int value) { return value >= 0 ? value / 128 : -((-value + 127) / 128); };
    auto pack = [&](const std::array<std::uint8_t, 3>& rgb) {
        const int luminance = (int(rgb[0]) + int(rgb[1]) * 2 + rgb[2]) >> 2;
        std::array<unsigned, 3> corrected{};
        for (unsigned channel = 0; channel < 3; ++channel) {
            const int value = std::clamp(luminance + floor128((int(rgb[channel]) - luminance) * control.saturation), 0, 255);
            corrected[channel] = channels[channel][static_cast<unsigned>(value)];
        }
        const auto light = (corrected[0] + corrected[1] * 2 + corrected[2]) >> 2;
        return (light << 24) | (corrected[0] << 16) | (corrected[1] << 8) | corrected[2];
    };
    std::vector<std::uint32_t> result(64 * 256);
    for (unsigned color = 0; color < 256; ++color) for (unsigned mode = 0; mode < 2; ++mode) {
        std::array<int, 3> value{}, step{};
        for (unsigned channel = 0; channel < 3; ++channel) {
            const auto from = gamma_palette[color * 3 + channel];
            value[channel] = int(from) * 65536;
            step[channel] = ((mode ? int(light_color[channel]) : 0) - from) * 0x842;
        }
        for (unsigned shade = 0; shade < 32; ++shade) {
            const auto row = mode ? shade + 32 : 31 - shade;
            std::array<std::uint8_t, 3> rgb{};
            for (unsigned channel = 0; channel < 3; ++channel) {
                rgb[channel] = inverse[static_cast<unsigned>(value[channel]) >> 16];
                value[channel] += step[channel];
            }
            result[row * 256 + color] = pack(rgb);
        }
    }
    return result;
}

TerrainDetailTile build_terrain_detail_tile(const TerrainGeometry& geometry, const AssetBytes& color,
    const TerrainBlendTables& tables, std::uint32_t x, std::uint32_t y) {
    if (color.size() != map_pixels || tables.blend.size() != 65536 || tables.shade.size() != 16384 ||
        tables.selectors.size() != 256 || std::any_of(tables.selectors.begin(), tables.selectors.end(),
        [](std::uint32_t value) { return value > 11; })) throw AssetError("Invalid terrain blend tables");
    const auto ix = (x >> 22) & 1023, iy = (y >> 22) & 1023;
    const std::array<std::size_t, 4> cells{iy * 1024 + ix, iy * 1024 + ((ix + 1) & 1023),
        ((iy + 1) & 1023) * 1024 + ix, ((iy + 1) & 1023) * 1024 + ((ix + 1) & 1023)};
    const auto& height = geometry.height_bytes();
    std::array<std::uint32_t, 12> colors{};
    for (unsigned i = 0; i < 4; ++i) colors[i] = color[cells[i]];
    auto mix = [&](std::uint32_t a, std::uint32_t b) { return tables.blend[a * 256 + b]; };
    colors[4] = mix(colors[0], colors[1]); colors[5] = mix(colors[0], colors[2]);
    colors[6] = mix(colors[0], colors[3]); colors[7] = mix(colors[1], colors[2]);
    colors[8] = mix(colors[1], colors[3]); colors[9] = mix(colors[2], colors[3]);
    colors[10] = mix(colors[4], colors[0]); colors[11] = mix(colors[5], colors[0]);
    const auto tile = static_cast<std::size_t>(geometry.character_bytes()[cells[0]]) * 256;
    const auto left0 = std::int32_t(height[cells[0]]) * 0x100000;
    const auto right0 = std::int32_t(height[cells[1]]) * 0x100000;
    const auto left_delta = std::int32_t(height[cells[2]]) * 0x100000 - left0;
    const auto right_delta = std::int32_t(height[cells[3]]) * 0x100000 - right0;
    auto shift = [](std::int32_t value, unsigned bits) {
        const auto scale = std::int32_t(1) << bits;
        return value >= 0 ? value / scale : -static_cast<std::int32_t>((-std::int64_t(value) + scale - 1) / scale);
    };
    TerrainDetailTile result;
    for (unsigned row = 0; row < 16; ++row) {
        const auto left = left0 + shift(left_delta, 5) + static_cast<std::int32_t>(row) * shift(left_delta, 4);
        const auto right = right0 + shift(right_delta, 5) + static_cast<std::int32_t>(row) * shift(right_delta, 4);
        const auto delta = right - left;
        for (unsigned column = 0; column < 16; ++column) {
            const auto at = row * 16 + column;
            const auto detail = geometry.detail_words()[tile + at];
            std::int32_t elevation = left + shift(delta, 5) + static_cast<std::int32_t>(column) * shift(delta, 4);
            if ((detail & 0xff0000) > 0x7fffff) elevation += static_cast<std::int32_t>(((detail & 0xff0000) - 0x800000) >> 1);
            result.height[at] = static_cast<std::uint16_t>(static_cast<std::uint32_t>(elevation) >> 16);
            auto value = colors[tables.selectors[at]];
            const auto shade = (detail & 0xff00) >> 10;
            const auto paint = detail & 255;
            if (shade) value = mix(tables.shade[shade * 256 + value], paint);
            else if (paint) value = paint;
            result.color[at] = static_cast<std::uint8_t>(value);
        }
    }
    return result;
}

VoxelProjection build_voxel_projection(std::int32_t focal_distance, float detail_density,
                                       float coarse_density, float depth_factor) {
    if (focal_distance <= 0 || focal_distance > 32767 || !std::isfinite(detail_density) ||
        !std::isfinite(coarse_density) || !std::isfinite(depth_factor) || detail_density < 1 ||
        coarse_density < 1 || detail_density > 32 || coarse_density > 32 || depth_factor <= 0 || depth_factor > 1) {
        throw AssetError("Invalid original voxel projection parameters");
    }
    auto truncate32 = [](long double value) {
        if (value < -2147483648.0L || value >= 2147483648.0L) return std::numeric_limits<std::int32_t>::min();
        return static_cast<std::int32_t>(value);
    };
    VoxelProjection result;
    for (unsigned i = 0; i < 14; ++i) {
        VoxelLod level;
        level.detail = i < 5;
        level.lod = i < 2 ? 0 : i < 5 ? i - 1 : i - 5;
        level.sample_step = i < 2 ? 1 : i < 5 ? double(1u << (i - 1)) : double(16u << (i - 5));
        const long double multiplier = i < 2 ? (i + 1) * 0.5L : level.sample_step;
        level.distance = static_cast<double>(static_cast<long double>(focal_distance) * multiplier /
                                             (i < 5 ? detail_density : coarse_density));
        level.world_bound = truncate32(static_cast<long double>(level.distance) * 4096);
        level.ray_step = truncate32(static_cast<long double>(level.sample_step) / focal_distance * 65536);
        level.sample_limit = truncate32(static_cast<long double>(level.distance) / level.sample_step);
        result.levels.push_back(level);
    }
    for (std::size_t i = 0; i < result.levels.size(); ++i) {
        auto& level = result.levels[i];
        const auto next = i + 1 < result.levels.size() ? result.levels[i + 1].ray_step : 0;
        if (level.ray_step == 0) throw AssetError("Voxel projection step is below fixed-point precision");
        level.next_step_scale = static_cast<std::int32_t>((std::int64_t(next) * 65536) / level.ray_step);
        double distance = i == 0 ? 0 : static_cast<double>(static_cast<long double>(depth_factor) * result.levels[i - 1].distance);
        const float depth_scale = depth_factor * 4.0f;
        while (distance < level.distance) {
            if (result.depths.size() >= 32768) throw AssetError("Voxel projection depth table is too large");
            const long double scaled = static_cast<long double>(depth_scale) * distance;
            const std::uint32_t value = scaled > 65535 ? 65535 : static_cast<std::uint32_t>(scaled + level.sample_step);
            result.depths.push_back(value | (value << 16));
            distance = static_cast<double>(static_cast<long double>(distance) + level.sample_step);
        }
        level.depth_end = static_cast<std::uint32_t>(result.depths.size());
    }
    return result;
}

VoxelColumns initialize_voxel_columns(const VoxelCamera& camera) {
    if (camera.width == 0 || camera.width > 1024 || camera.height == 0 || camera.height > 768 ||
        camera.row_stride < camera.width || camera.row_stride > 65535 || camera.left < 0 || camera.top < 0 ||
        camera.right < camera.left || camera.bottom < camera.top ||
        camera.right >= static_cast<std::int32_t>(camera.width) || camera.bottom >= static_cast<std::int32_t>(camera.height) ||
        camera.left % 2 || camera.right % 2 != 1 || camera.focal_distance <= 0 || camera.focal_distance > 32767) {
        throw AssetError("Invalid original voxel camera bounds or focal distance");
    }
    auto shift = [](std::int64_t value, unsigned bits) {
        const auto scale = std::int64_t(1) << bits;
        return static_cast<std::int32_t>(value >= 0 ? value / scale : -((-value + scale - 1) / scale));
    };
    auto mul20 = [&](std::int32_t a, std::int32_t b) { return shift(std::int64_t(a) * b, 20); };
    auto mul16 = [&](std::int32_t a, std::int32_t b) { return shift(std::int64_t(a) * b, 16); };
    const auto yaw = fixed_sine_cosine(camera.yaw), pitch = fixed_sine_cosine(camera.pitch);
    const auto ys = shift(yaw[0], 2), yc = shift(yaw[1], 2);
    const auto ps = shift(pitch[0], 2), pc = shift(pitch[1], 2);
    const auto yc_ps = mul20(yc, ps), ys_ps = mul20(ys, ps);
    const auto inverse = static_cast<std::int32_t>((std::int64_t(1) << 36) / (std::int64_t(camera.focal_distance) * 65536));
    const auto center_x = camera.left + ((camera.right - camera.left + 1) >> 1);
    const auto center_y = camera.top + ((camera.bottom - camera.top + 1) >> 1);
    const auto forward = mul16(camera.focal_distance * 65536, inverse);
    const auto vertical = mul16((center_y - camera.bottom) * 65536, inverse);
    VoxelColumns result;
    result.columns.reserve(static_cast<std::size_t>(camera.right - camera.left + 1) / 2);
    for (auto x = camera.left; x <= camera.right; x += 2) {
        const auto horizontal = mul16((center_x - x) * 65536, inverse);
        VoxelColumn column;
        column.depth_index = 0;
        column.pixel_offset = camera.bottom * static_cast<std::int32_t>(camera.row_stride) + x;
        for (unsigned axis = 0; axis < 3; ++axis) column.position[axis] = static_cast<std::uint32_t>(camera.position[axis]) << 6;
        column.step = {
            static_cast<std::uint32_t>(shift(std::int64_t(mul20(mul20(yc, pc), forward)) - mul20(ys, horizontal) - mul20(yc_ps, vertical) + 2, 2)),
            static_cast<std::uint32_t>(shift(std::int64_t(mul20(mul20(ys, pc), forward)) + mul20(yc, horizontal) - mul20(ys_ps, vertical) + 2, 2)),
            static_cast<std::uint32_t>(shift(std::int64_t(mul20(pc, vertical)) + mul20(ps, forward) + 2, 2))};
        result.columns.push_back(column);
    }
    result.acceleration_step = {
        static_cast<std::uint32_t>(shift(std::int64_t(mul20(yc_ps, inverse)) + 2, 2)),
        static_cast<std::uint32_t>(shift(std::int64_t(mul20(ys_ps, inverse)) + 2, 2)),
        static_cast<std::uint32_t>(shift(std::int64_t(mul20(-pc, inverse)) + 2, 2))};
    return result;
}

void render_voxel_pass_8(const VoxelPass8& pass, const AssetBytes& height, const AssetBytes& color,
                         const std::vector<std::uint32_t>& depth_table, std::vector<VoxelColumn>& columns,
                         AssetBytes& pixels, std::vector<std::uint16_t>& depths) {
    if (pass.lod > 9) throw AssetError("Voxel mip level must be 0 through 9");
    const auto width = 1024u >> pass.lod;
    if (height.size() != static_cast<std::uint64_t>(width) * width || color.size() != height.size()) {
        throw AssetError("Voxel pass maps do not match the mip level");
    }
    if (depth_table.size() < 2 || pass.stop_depth >= depth_table.size() - 1 ||
        pass.stop_depth >= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max() - 1)) {
        throw AssetError("Voxel depth table requires a guard entry after the stop index");
    }
    if (pixels.empty() || pixels.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
        depths.size() != pixels.size() || pass.row_stride < 2 || pixels.size() % pass.row_stride ||
        pass.top_offset < 0 || static_cast<std::size_t>(pass.top_offset) >= pixels.size()) {
        throw AssetError("Invalid voxel output buffer or clipping range");
    }
    auto signed_bits = [](std::uint32_t bits) {
        std::int32_t value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    };
    const auto mask = 0x3ffu << (pass.lod + 22);
    for (auto& column : columns) {
        if (column.finished) continue;
        if (column.depth_index < -1 || column.depth_index > static_cast<std::int32_t>(pass.stop_depth)) {
            throw AssetError("Voxel column depth index is outside the pass range");
        }
        if (signed_bits(column.position[2]) > 0x10000000 && signed_bits(column.step[2]) >= 0) {
            column.finished = 1;
            continue;
        }
        bool clipped = false;
        do {
            ++column.depth_index;
            for (unsigned axis = 0; axis < 3; ++axis) {
                column.acceleration[axis] += pass.acceleration_step[axis];
                column.position[axis] += column.step[axis];
            }
            for (;;) {
                const auto index = ((column.position[0] & mask) >> (pass.lod + 22)) +
                                   ((column.position[1] & mask) >> (pass.lod * 2 + 12));
                if (signed_bits(column.position[2]) >= static_cast<std::int32_t>(height[index]) * 0x100000) break;
                if (column.pixel_offset < 0 || static_cast<std::size_t>(column.pixel_offset) + 1 >= pixels.size() ||
                    static_cast<std::uint32_t>(column.pixel_offset) % pass.row_stride == pass.row_stride - 1) {
                    throw AssetError("Voxel column pixel pair is outside the output buffer");
                }
                const auto at = static_cast<std::size_t>(column.pixel_offset);
                pixels[at] = pixels[at + 1] = color[index];
                const auto depth = depth_table[static_cast<std::size_t>(column.depth_index)];
                depths[at] = static_cast<std::uint16_t>(depth);
                depths[at + 1] = static_cast<std::uint16_t>(depth >> 16);
                for (unsigned axis = 0; axis < 3; ++axis) {
                    column.position[axis] -= column.acceleration[axis];
                    column.step[axis] -= pass.acceleration_step[axis];
                }
                column.pixel_offset -= static_cast<std::int32_t>(pass.row_stride);
                if (column.pixel_offset < pass.top_offset) {
                    clipped = true;
                    break;
                }
            }
            if (clipped) break;
        } while (column.depth_index <= static_cast<std::int32_t>(pass.stop_depth));
        column.finished = clipped ? 1 : 0;
        for (auto& step : column.step) {
            const auto product = std::int64_t(signed_bits(step)) * pass.step_scale;
            step = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 16);
        }
    }
}

} // namespace dfre
