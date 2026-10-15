#include "dfre/water.hpp"
#include "dfre/models.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <locale>
#include <sstream>

namespace dfre {
namespace {
constexpr std::size_t water_clut_entries = 64 * 256;
constexpr std::int32_t water_max_step = 0x3ff0000;

std::int32_t signed_word(std::uint32_t bits) {
    std::int32_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t unsigned_word(std::int32_t value) {
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::int32_t add32(std::int32_t a, std::int32_t b) {
    return signed_word(unsigned_word(a) + unsigned_word(b));
}

std::int32_t sub32(std::int32_t a, std::int32_t b) {
    return signed_word(unsigned_word(a) - unsigned_word(b));
}

// Original SHRD x 22 pattern: 64-bit product shifted right (floor), low 32 bits kept.
std::int32_t mul22(std::int32_t a, std::int32_t b) {
    const auto product = std::int64_t(a) * b;
    const auto scale = std::int64_t(1) << 22;
    const auto shifted = product >= 0 ? product / scale : -((-product + scale - 1) / scale);
    return signed_word(static_cast<std::uint32_t>(shifted));
}

// Original SAR: floor division by a power of two.
std::int32_t shift_floor(std::int32_t value, unsigned bits) {
    const auto scale = std::int64_t(1) << bits;
    return static_cast<std::int32_t>(value >= 0 ? value / scale : -((-std::int64_t(value) + scale - 1) / scale));
}

// Original CDQ/XOR/SUB absolute value, including INT32_MIN.
std::int32_t abs32(std::int32_t value) {
    const auto wide = value < 0 ? -std::int64_t(value) : std::int64_t(value);
    return signed_word(static_cast<std::uint32_t>(wide));
}

char lower(char value) { return value >= 'A' && value <= 'Z' ? static_cast<char>(value + 'a' - 'A') : value; }

bool same(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
        [](char x, char y) { return lower(x) == lower(y); });
}

const TerrainField* find_field(const TerrainConfig& config, std::string_view name) {
    const TerrainField* result = nullptr;
    for (const auto& item : config.fields) if (same(item.name, name)) {
        if (result || item.values.size() != 1) throw AssetError("Invalid terrain field: " + std::string(name));
        result = &item;
    }
    return result;
}

// Original FUN_0049bae6 + ROUND near 0x47b1a0: fixed 16.16 from a config float.
std::uint32_t fixed16_16(const TerrainField& field) {
    double number = 0;
    std::istringstream stream(field.values[0]);
    stream.imbue(std::locale::classic());
    stream >> number;
    if (!stream || stream.peek() != std::char_traits<char>::eof() || !std::isfinite(number))
        throw AssetError("Invalid terrain fixed-point field: " + field.name);
    const auto fixed = std::nearbyint(number * 65536.0);
    if (fixed < std::numeric_limits<std::int32_t>::min() || fixed > std::numeric_limits<std::int32_t>::max())
        throw AssetError("Terrain fixed-point field overflows: " + field.name);
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(fixed));
}

// Original 0x480A70 channel tables from brightness and filter.
std::array<std::array<std::uint8_t, 256>, 3> build_water_channels(const TerrainColorControl& control) {
    if (control.brightness < 0 || control.brightness > 255 || control.saturation < 0 || control.saturation > 255 ||
        std::any_of(control.channels.begin(), control.channels.end(),
            [](int value) { return value < 0 || value > 255; })) {
        throw AssetError("Invalid water 32-bit color controls");
    }
    const int factor = control.brightness >= 128 ? (control.brightness - 128) * 2 :
        std::min(255, (128 - control.brightness) * 2);
    const bool positive = control.brightness >= 128;
    std::array<std::array<std::uint8_t, 256>, 3> channels{};
    for (int i = 0; i < 256; ++i) {
        const int curve = std::max(0, static_cast<int>(std::sqrt(255.0 * i) - i + 0.5));
        const int delta = (curve * factor) >> 8;
        const int base = positive ? i + delta : std::max(0, i - delta);
        for (unsigned channel = 0; channel < 3; ++channel) {
            channels[channel][i] = static_cast<std::uint8_t>(std::min(255, ((base * control.channels[channel]) >> 8) * 2));
        }
    }
    return channels;
}

// Original 0x480BF0: saturation around the packed luminance, then channel tables.
std::array<std::uint8_t, 3> water_correct(const std::array<std::uint8_t, 3>& rgb, int saturation,
    const std::array<std::array<std::uint8_t, 256>, 3>& channels) {
    std::array<std::uint8_t, 3> result{};
    const int luminance = (int(rgb[0]) + 2 * int(rgb[1]) + rgb[2]) >> 2;
    for (unsigned channel = 0; channel < 3; ++channel) {
        const int delta = int(rgb[channel]) - luminance;
        const int scaled = static_cast<int>(std::int64_t(delta) * saturation >> 7);
        const int value = std::clamp(luminance + scaled, 0, 255);
        result[channel] = channels[channel][static_cast<unsigned>(value)];
    }
    return result;
}

// Original packing: luminance in the high byte, then red, green, blue.
std::uint32_t pack_water_rgb(const std::array<std::uint8_t, 3>& rgb) {
    return std::uint32_t((std::uint32_t(rgb[0]) + std::uint32_t(rgb[1]) * 2 + rgb[2]) >> 2) << 24 |
        std::uint32_t(rgb[0]) << 16 | std::uint32_t(rgb[1]) << 8 | rgb[2];
}

void require_water_texels(const AssetBytes& texels, unsigned level) {
    const auto width = 256u >> level;
    if (texels.size() != static_cast<std::uint64_t>(width) * width ||
        std::any_of(texels.begin(), texels.end(), [](std::uint8_t value) { return value > 63; })) {
        throw AssetError("Water texel mip must be a 6-bit square map");
    }
}
} // namespace

std::array<std::int32_t, 1280> build_water_wave_table(unsigned scale_shift) {
    // Original 0x489440: angle accumulates the double 2*pi/1024 constant and
    // truncates sin(angle) * 2^(22 - 0xBD5270). The shift wraps like x86 SHL and
    // the original FILD reads bit 31 as a sign bit.
    const auto bits = (22 - static_cast<int>(scale_shift)) & 31;
    const double scale = bits == 31 ? -2147483648.0 : std::ldexp(1.0, bits);
    const double step = 0.0061359232421875;
    std::array<std::int32_t, 1280> result{};
    double angle = 0.0;
    for (auto& value : result) {
        value = static_cast<std::int32_t>(std::trunc(std::sin(angle) * scale));
        angle += step;
    }
    return result;
}

WaterPalette32 build_water_palette32(const AssetBytes& water_palette_rgb, std::uint32_t opacity,
    const TerrainColorControl& control) {
    if (water_palette_rgb.size() < 48) throw AssetError("Water palette requires 16 RGB colors");
    if (opacity > 0x10000) throw AssetError("Water opacity must be a 16.16 fraction");
    const auto channels = build_water_channels(control);
    const auto correct = [&](const std::array<std::uint8_t, 3>& rgb) {
        return water_correct(rgb, control.saturation, channels);
    };
    // Original 0x47C6A0 water section: 16 gradient rows of 16 steps through the
    // first 16 palette colors, row 0 repeating the first value.
    std::array<std::uint32_t, 256> pattern{};
    for (unsigned row = 0; row < 15; ++row) {
        std::array<std::int32_t, 3> value{}, step{};
        for (unsigned channel = 0; channel < 3; ++channel) {
            const auto from = std::int32_t(water_palette_rgb[row * 3 + channel]) << 16;
            const auto to = std::int32_t(water_palette_rgb[(row + 1) * 3 + channel]) << 16;
            value[channel] = from;
            step[channel] = shift_floor(sub32(to, from), 4);
        }
        for (unsigned column = 0; column < 16; ++column) {
            const std::array<std::uint8_t, 3> rgb{
                static_cast<std::uint8_t>(value[0] >> 16),
                static_cast<std::uint8_t>(value[1] >> 16),
                static_cast<std::uint8_t>(value[2] >> 16)};
            const auto packed = pack_water_rgb(correct(rgb));
            if (row == 0 && column == 0) std::fill_n(pattern.begin(), 16, packed);
            pattern[(row + 1) * 16 + column] = packed;
            for (unsigned channel = 0; channel < 3; ++channel) value[channel] += step[channel];
        }
    }
    WaterPalette32 result;
    result.blends.resize(water_clut_entries);
    for (unsigned luminance = 0; luminance < 256; ++luminance) {
        const auto row = static_cast<std::int32_t>(
            (std::int64_t(0x10000 - opacity) * luminance) >> 16);
        for (std::uint32_t texel = 0; texel < 64; ++texel) {
            const auto column = static_cast<std::int32_t>(
                (std::int64_t(opacity) * (4 * texel)) >> 16);
            const auto index = row + column;
            if (index < 0 || index > 255) throw AssetError("Water blend index is outside the ramp");
            result.blends[texel * 256 + luminance] = pattern[static_cast<std::size_t>(index)];
        }
    }
    return result;
}

WaterScene32 prepare_water_scene32(const TerrainScene32& scene, const TerrainColorControl& control) {
    const auto& terrain = scene.terrain;
    if (terrain.pixel_bits != 32) throw AssetError("Water scene preparation requires 32-bit runtime assets");
    WaterScene32 result;
    result.horizon_scale = scene.horizon_scale;
    result.level = terrain.water_height;
    if (result.level == 0) return result;
    if (!terrain.water || !terrain.water_palette) throw AssetError("Water runtime assets are incomplete");
    if (terrain.water->width != 256 || terrain.water->height != 256 || terrain.water->pixels.size() != 256 * 256)
        throw AssetError("Water map must be a 256-square image");
    result.texels[0] = terrain.water->pixels;
    for (unsigned level = 1; level < result.texels.size(); ++level)
        result.texels[level] = terrain_sky_water_mip(result.texels[level - 1], 256u >> (level - 1));
    for (unsigned level = 0; level < result.texels.size(); ++level) require_water_texels(result.texels[level], level);
    if (const auto* opacity = find_field(terrain.config, "water_opacity")) result.opacity = fixed16_16(*opacity);
    if (const auto* turbidity = find_field(terrain.config, "turbidity"))
        result.scroll_shift = fixed16_16(*turbidity) & 0xff;
    if (result.opacity > 0x10000) throw AssetError("Terrain water opacity must be 0 through 1");
    if (terrain.water_palette->palette.size() != 768) throw AssetError("Water palette requires an RGB palette");
    const AssetBytes colors(terrain.water_palette->palette.begin(), terrain.water_palette->palette.begin() + 48);
    result.palette = build_water_palette32(colors, result.opacity, control);
    return result;
}

namespace {
void require_water_camera(const VoxelCamera& camera) {
    if (camera.width == 0 || camera.width > 1024 || camera.height == 0 || camera.height > 768 ||
        camera.row_stride < camera.width || camera.left < 0 || camera.top < 0 || camera.right < camera.left ||
        camera.bottom < camera.top || camera.right >= static_cast<std::int32_t>(camera.width) ||
        camera.bottom >= static_cast<std::int32_t>(camera.height) || camera.focal_distance <= 0 ||
        camera.focal_distance > 32767) {
        throw AssetError("Invalid original water camera bounds");
    }
}

void require_water_scene(const WaterScene32& scene) {
    for (unsigned level = 0; level < scene.texels.size(); ++level) require_water_texels(scene.texels[level], level);
    if (scene.palette.blends.size() != water_clut_entries) throw AssetError("Water blend table is incomplete");
}

// Shared row mapping of 0x48D040 and 0x48D570 after the plane distance step.
struct WaterRow32 {
    std::int32_t world_x = 0, world_y = 0, block_x = 0, block_y = 0;
    unsigned mip = 0;
    std::uint16_t depth = 0;
};

WaterRow32 map_water_row(const VoxelCamera& camera, std::int32_t w0, std::int32_t w1, std::int32_t s,
    std::int32_t p, std::int32_t scale, std::int32_t step) {
    WaterRow32 result;
    result.world_x = signed_word(unsigned_word(add32(mul22(s, scale), camera.position[0])) << 4);
    result.world_y = signed_word(unsigned_word(add32(mul22(p, scale), camera.position[1])) << 4);
    const auto level_scale = signed_word(unsigned_word(step / camera.focal_distance) << 4);
    unsigned mip = 0;
    for (auto value = level_scale; value > 0x10000; value >>= 1) ++mip;
    result.mip = std::min(mip, 7u);
    result.block_x = signed_word(unsigned_word(mul22(level_scale, w1)) << 5);
    result.block_y = signed_word((unsigned_word(0) - unsigned_word(mul22(level_scale, w0))) << 5);
    result.depth = static_cast<std::uint16_t>(unsigned_word(step) >> 10);
    return result;
}

// Original cosine window at 0x500D58 (0xC92AA0 + 256 entries).
std::int32_t water_cos_sample(const std::array<std::int32_t, 1280>& wave, std::uint32_t scroll, unsigned mip,
    std::int32_t coordinate, std::int32_t value) {
    const auto index = (unsigned_word(unsigned_word(coordinate) << 8) + scroll) >> 22 & 0x3ff;
    return shift_floor(add32(wave[index + 256], value), mip);
}

// Original sine table at 0xC92AA0.
std::int32_t water_sin_sample(const std::array<std::int32_t, 1280>& wave, std::uint32_t scroll, unsigned mip,
    std::int32_t coordinate, std::int32_t value) {
    const auto index = (unsigned_word(unsigned_word(coordinate) << 8) + scroll) >> 22 & 0x3ff;
    return shift_floor(add32(wave[index], value), mip);
}

// Original 32-bit span helpers 0x48CD20 (0x48D040) and 0x48CF30 (0x48D570).
// The companion writes the flat blend column 0 where the plane is behind the
// existing pixel; the main pass leaves such pixels alone.
void draw_water_span(const WaterScene32& scene, const WaterRow32& mapping, bool flat,
    const std::uint16_t* depth_row, std::uint32_t* screen, int count,
    std::int32_t u0, std::int32_t v0, std::int32_t du, std::int32_t dv) {
    const auto& texels = scene.texels[mapping.mip];
    const auto side_mask = 0xffu >> mapping.mip;
    auto u = u0, v = v0;
    for (int i = 0; i < count; ++i) {
        if (mapping.depth < depth_row[i]) {
            const auto texel_u = shift_floor(u, 16) & side_mask;
            const auto row_offset = unsigned_word(shift_floor(v, 16) & side_mask) << (8 - mapping.mip);
            const auto texel = texels[row_offset + texel_u];
            const auto luminance = static_cast<unsigned>((screen[i] >> 24) & 0xff);
            screen[i] = scene.palette.blends[(texel << 8) | luminance];
        } else if (flat) {
            const auto luminance = static_cast<unsigned>((screen[i] >> 24) & 0xff);
            screen[i] = scene.palette.blends[luminance];
        }
        u = add32(u, du);
        v = add32(v, dv);
    }
}

void draw_water_row(const WaterScene32& scene, const VoxelCamera& camera, const std::array<std::int32_t, 1280>& wave,
    std::uint32_t scroll, bool flat, int row, WaterRow32 mapping, std::vector<std::uint32_t>& pixels,
    std::vector<std::uint16_t>& depths) {
    const auto span = camera.right - camera.left + 1;
    auto* screen = pixels.data() + std::size_t(row) * camera.row_stride + camera.left;
    const auto* depth_row = depths.data() + std::size_t(row) * camera.row_stride + camera.left;
    auto u0 = water_cos_sample(wave, scroll, mapping.mip, mapping.world_y, mapping.world_x);
    auto v0 = water_sin_sample(wave, scroll, mapping.mip, mapping.world_x, mapping.world_y);
    for (int column = 0; column < span; column += 32) {
        const int count = std::min<int>(32, span - column);
        mapping.world_x = add32(mapping.world_x, mapping.block_x);
        mapping.world_y = add32(mapping.world_y, mapping.block_y);
        const auto u1 = water_cos_sample(wave, scroll, mapping.mip, mapping.world_y, mapping.world_x);
        const auto v1 = water_sin_sample(wave, scroll, mapping.mip, mapping.world_x, mapping.world_y);
        const auto du = shift_floor(sub32(u1, u0), 5);
        const auto dv = shift_floor(sub32(v1, v0), 5);
        draw_water_span(scene, mapping, flat, depth_row + column, screen + column, count, u0, v0, du, dv);
        u0 = u1;
        v0 = v1;
    }
}
} // namespace

std::vector<std::uint32_t> build_water_row_depths(const std::vector<WaterTerrainRowDepth>& entries,
    const VoxelCamera& camera) {
    require_water_camera(camera);
    for (const auto& entry : entries) {
        if (entry.top_row < 0) throw AssetError("Water row depth entry is outside the frame");
    }
    // Original 0x489260: each cache entry covers rows above its top row until
    // an earlier entry already claimed them, then rows above the covered
    // region become water_row_no_terrain.
    std::vector<std::uint32_t> result(camera.height, water_row_no_terrain);
    auto row = camera.bottom;
    for (const auto& entry : entries) {
        if (entry.top_row >= row) continue;
        while (row > entry.top_row) {
            result[static_cast<std::size_t>(row)] = entry.depth;
            --row;
        }
    }
    for (; row >= camera.top; --row) result[static_cast<std::size_t>(row)] = water_row_no_terrain;
    return result;
}

void render_water_pass32(const WaterScene32& scene, const VoxelCamera& camera, const WaterPass32& pass,
    std::vector<std::uint32_t>& pixels, std::vector<std::uint16_t>& depths) {
    require_water_camera(camera);
    if (pixels.size() != depths.size() || pixels.size() != std::size_t(camera.row_stride) * camera.height)
        throw AssetError("Water frame buffers do not match the camera");
    if (pass.row_depths.size() < camera.height) throw AssetError("Water row depths are incomplete");
    if (scene.level == 0) return;
    if (scene.level >= camera.position[2]) return;
    require_water_scene(scene);
    const auto horizon = build_terrain_horizon32(camera, scene.horizon_scale);
    const auto rays = build_terrain_sky_rays(camera);
    const auto wave = build_water_wave_table(scene.wave_scale_shift);
    const auto start_row = std::max(horizon.row, camera.top);
    if (start_row > camera.bottom) return;
    const auto yaw = fixed_sine_cosine(camera.yaw), pitch = fixed_sine_cosine(camera.pitch);
    const auto w0 = yaw[1], w1 = yaw[0], w2 = pitch[1], w3 = pitch[0];
    const auto a = mul22(w0, w2), b = mul22(w0, w3), c = mul22(w1, w2), d = mul22(w1, w3);
    const auto scroll = std::uint32_t(pass.time << (scene.scroll_shift & 31));
    for (auto row = camera.bottom; row >= start_row; --row) {
        const auto& ray = rays[static_cast<std::size_t>(row)];
        const auto t0 = ray[0], t1 = ray[1], t2 = ray[2];
        const auto den = add32(mul22(w3, t0), mul22(w2, t2));
        if (abs32(den) <= 0x80) return;
        const auto delta = sub32(camera.position[2], scene.level);
        const auto den2 = abs32(den) >> 6;
        if (den2 == 0) return;
        // Original IDIV by den2 = |den| >> 6; the original faults when the
        // quotient leaves the signed 32-bit range and the port wraps instead.
        const auto quote = static_cast<std::int32_t>(static_cast<std::uint32_t>(
            (std::int64_t(1) << 32) / den2));
        const auto scale = static_cast<std::int32_t>(static_cast<std::uint32_t>((std::int64_t(quote) * delta) >> 16));
        const auto step = mul22(scale, t0);
        if (step >= water_max_step) return;
        if (step > static_cast<std::int32_t>(pass.row_depths[static_cast<std::size_t>(row)])) continue;
        const auto s = sub32(sub32(mul22(a, t0), mul22(w1, t1)), mul22(b, t2));
        const auto p = sub32(add32(mul22(c, t0), mul22(w0, t1)), mul22(d, t2));
        draw_water_row(scene, camera, wave, scroll, false, row,
            map_water_row(camera, w0, w1, s, p, scale, step), pixels, depths);
    }
}

void render_water_companion32(const WaterScene32& scene, const VoxelCamera& camera, const WaterPass32& pass,
    std::vector<std::uint32_t>& pixels, std::vector<std::uint16_t>& depths) {
    require_water_camera(camera);
    if (pixels.size() != depths.size() || pixels.size() != std::size_t(camera.row_stride) * camera.height)
        throw AssetError("Water frame buffers do not match the camera");
    if (scene.level == 0) return;
    if (camera.position[2] > scene.level) return;
    require_water_scene(scene);
    const auto rays = build_terrain_sky_rays(camera);
    const auto wave = build_water_wave_table(scene.wave_scale_shift);
    const auto yaw = fixed_sine_cosine(camera.yaw), pitch = fixed_sine_cosine(camera.pitch);
    const auto w0 = yaw[1], w1 = yaw[0], w2 = pitch[1], w3 = pitch[0];
    const auto a = mul22(w0, w2), b = mul22(w0, w3), c = mul22(w1, w2), d = mul22(w1, w3);
    const auto scroll = std::uint32_t(pass.timer << (scene.scroll_shift & 31));
    auto row = camera.top;
    for (; row <= camera.bottom; ++row) {
        const auto& ray = rays[static_cast<std::size_t>(row)];
        const auto t0 = ray[0], t1 = ray[1], t2 = ray[2];
        const auto den = add32(mul22(w2, t2), mul22(w3, t0));
        if (den < 0x101) break;
        const auto delta = sub32(scene.level, camera.position[2]);
        const auto den2 = shift_floor(den, 6);
        if (den2 == 0) break;
        const auto quote = static_cast<std::int32_t>(static_cast<std::uint32_t>(
            (std::int64_t(1) << 32) / den2));
        const auto scale = static_cast<std::int32_t>(static_cast<std::uint32_t>((std::int64_t(quote) * delta) >> 16));
        const auto step = mul22(scale, t0);
        if (step >= water_max_step) break;
        const auto s = sub32(sub32(mul22(a, t0), mul22(w1, t1)), mul22(b, t2));
        const auto p = sub32(add32(mul22(c, t0), mul22(w0, t1)), mul22(d, t2));
        draw_water_row(scene, camera, wave, scroll, true, row,
            map_water_row(camera, w0, w1, s, p, scale, step), pixels, depths);
    }
    // Original tail: rows past an early stop keep the flat blend column.
    const auto span = camera.right - camera.left + 1;
    for (; row <= camera.bottom; ++row) {
        auto* screen = pixels.data() + std::size_t(row) * camera.row_stride + camera.left;
        for (int i = 0; i < span; ++i) {
            const auto luminance = static_cast<unsigned>((screen[i] >> 24) & 0xff);
            screen[i] = scene.palette.blends[luminance];
        }
    }
}

void render_water_frame32(const WaterScene32& scene, const VoxelCamera& camera, const WaterPass32& pass,
    std::vector<std::uint32_t>& pixels, std::vector<std::uint16_t>& depths) {
    render_water_pass32(scene, camera, pass, pixels, depths);
    render_water_companion32(scene, camera, pass, pixels, depths);
}

} // namespace dfre
