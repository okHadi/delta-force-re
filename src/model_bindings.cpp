#include "dfre/model_bindings.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace dfre {
namespace {
constexpr std::size_t kMaterialSize = 76;

std::uint16_t read_word(const std::array<std::uint8_t, kMaterialSize>& raw, std::size_t offset) {
    return std::uint16_t(raw[offset]) | std::uint16_t(raw[offset + 1]) << 8;
}

std::uint32_t read_dword(const std::array<std::uint8_t, kMaterialSize>& raw, std::size_t offset) {
    return std::uint32_t(raw[offset]) | std::uint32_t(raw[offset + 1]) << 8 |
        std::uint32_t(raw[offset + 2]) << 16 | std::uint32_t(raw[offset + 3]) << 24;
}

void write_word(std::array<std::uint8_t, kMaterialSize>& raw, std::size_t offset, std::uint16_t value) {
    raw[offset] = std::uint8_t(value);
    raw[offset + 1] = std::uint8_t(value >> 8);
}

void write_dword(std::array<std::uint8_t, kMaterialSize>& raw, std::size_t offset, std::uint32_t value) {
    raw[offset] = std::uint8_t(value);
    raw[offset + 1] = std::uint8_t(value >> 8);
    raw[offset + 2] = std::uint8_t(value >> 16);
    raw[offset + 3] = std::uint8_t(value >> 24);
}

// Original 485E30 dimension loop: highest bit index with 2^i at or below the stored size.
std::uint8_t dimension_bits(std::uint16_t dimension) {
    const auto value = std::int32_t(std::uint32_t(dimension) << 16) >> 16;
    std::uint8_t bits = 0;
    for (unsigned i = 0; i < 16; ++i)
        if (value >= (1 << i)) bits = std::uint8_t(i);
    return bits;
}

std::uint16_t dimension_mask(std::uint8_t bits) {
    return std::uint16_t((std::uint32_t(1) << bits) - 1);
}

// Original 480A30 remaps one entry against the installed bank palette; one build covers 255.
// 480600 builds its search table from BFBB00 (display_rgb) through 500b0c, which is exactly
// the bank matching_rgb table, so that table is the matcher palette here.
std::vector<std::uint8_t> remap_source_entries(const AssetBytes& source,
    const ModelPaletteBank32& palette, const ModelGammaTable& gamma) {
    if (source.size() != 768) throw std::invalid_argument("model palette remap needs 768 source bytes");
    return remap_palette8(source, palette.matching_rgb, AssetBytes(gamma.begin(), gamma.end()));
}

// Original 486880 frequency loop: index 0 never wins, ties keep the lower index.
std::uint8_t most_frequent_texel(const std::vector<std::uint8_t>& pixels) {
    std::array<std::uint32_t, 256> counts{};
    for (const auto index : pixels) ++counts[index];
    counts[0] = 0;
    std::uint8_t best = 0;
    for (unsigned i = 1; i < 256; ++i)
        if (counts[best] < counts[i]) best = std::uint8_t(i);
    return best;
}

void require_palette(const ModelPaletteBank32& palette) {
    if (palette.matching_rgb.size() != 768 || palette.shades.size() != 64 * 256)
        throw std::invalid_argument("model palette bank needs 768 matching RGB bytes and 64 shade rows");
}
} // namespace

std::vector<std::size_t> model_texture_span32(const Model& model, unsigned texture_level) {
    if (texture_level > 3) throw std::invalid_argument("model texture level outside four levels");
    std::vector<std::size_t> span;
    for (std::size_t i = 0; i < model.textures.size(); ++i)
        if (model.textures[i].flags & (1u << texture_level)) span.push_back(i);
    return span;
}

const ModelPreparedMaterial32* ModelPreparedModel32::material(std::size_t lod, std::size_t index) const {
    if (lod >= lods.size() || index >= lods[lod].materials.size())
        throw std::invalid_argument("prepared material outside table");
    return &lods[lod].materials[index];
}

ModelLod apply_model_bindings32(const ModelLod& lod, const ModelPreparedLod32& prepared) {
    if (lod.materials.size() != prepared.materials.size())
        throw std::invalid_argument("prepared material list size differs from the lod");
    ModelLod result = lod;
    for (std::size_t i = 0; i < result.materials.size(); ++i) {
        const auto& raw = prepared.materials[i].raw;
        auto& material = result.materials[i];
        material.raw = raw;
        material.flags = read_dword(raw, 0x10);
        material.frame_count = raw[0x1e];
        material.animation_parameter = raw[0x1f];
        material.flat_rgb = {raw[40], raw[41], raw[42]};
        material.texture_key = raw[(material.flags & 0x8000) && !(material.flags & 1) ? 55 : 52];
    }
    return result;
}

ModelLod ModelPreparedModel32::apply(std::size_t lod_index, const ModelLod& source) const {
    if (lod_index >= lods.size()) throw std::invalid_argument("prepared lod outside table");
    return apply_model_bindings32(source, lods[lod_index]);
}

RasterMaterial32 ModelPreparedModel32::raster_material(std::size_t lod, std::size_t index) const {
    const auto* prepared = material(lod, index);
    if (!palette) throw std::invalid_argument("prepared model has no bank palette");
    RasterMaterial32 view;
    view.shade_colours = &palette->shades;
    view.flat_index = prepared->flat_index;
    // 4809B0 keeps the light table at 0xbfc104 and sets 0xbfc108 = light + 0x8000: dword row 32.
    view.tiny_colour = palette->shades[std::size_t(32) * 256 + prepared->flat_index];
    if (prepared->texture) {
        if (*prepared->texture >= textures.size()) throw std::invalid_argument("prepared texture slot outside table");
        if (raster_cache_.size() != textures.size()) raster_cache_.resize(textures.size());
        auto& cache = raster_cache_[*prepared->texture];
        if (!cache) {
            RasterTexture32 raster;
            raster.width_bits = textures[*prepared->texture].width_bits;
            raster.height_bits = textures[*prepared->texture].height_bits;
            raster.pixels = textures[*prepared->texture].pixels;
            raster.shade_colours = palette->shades;
            cache = std::move(raster);
        }
        view.texture = &*cache;
    }
    return view;
}

ModelPreparedModel32 prepare_model_bindings32(const Model& model, std::uint8_t bank, unsigned texture_level,
    const ModelPaletteBank32& palette, const ModelGammaTable& gamma) {
    if (bank >= 6) throw std::invalid_argument("model palette bank outside six banks");
    if (texture_level > 3) throw std::invalid_argument("model texture level outside four levels");
    require_palette(palette);
    ModelPreparedModel32 result;
    result.bank = bank;
    result.texture_level = texture_level;
    result.palette = &palette;
    const auto span = model_texture_span32(model, texture_level);
    result.textures.reserve(span.size());
    for (const auto index : span) {
        const auto& source = model.textures[index];
        if (source.pixels.size() > 0xffffffffu) throw std::invalid_argument("model texture size outside 32 bits");
        ModelPreparedTexture32 record;
        record.name = source.name;
        record.pixel_count = std::uint32_t(source.pixels.size());
        record.sequence = source.sequence;
        record.flags = source.flags;
        record.bank = bank;
        record.width = source.width;
        record.height = source.height;
        record.width_bits = dimension_bits(source.width);
        record.height_bits = dimension_bits(source.height);
        for (unsigned i = 0; i < 256; ++i)
            for (unsigned channel = 0; channel < 3; ++channel)
                record.palette_rgb[std::size_t(i) * 3 + channel] = source.palette_rgb_pad[std::size_t(i) * 4 + channel];
        record.pixels = source.pixels;
        result.textures.push_back(std::move(record));
    }
    // Original 485E30 writes; the texture span is this model's loaded slots.
    for (const auto& lod : model.lods) {
        ModelPreparedLod32 prepared;
        prepared.materials.reserve(lod.materials.size());
        for (const auto& source : lod.materials) {
            ModelPreparedMaterial32 material;
            material.raw = source.raw;
            if (material.raw[0x11] & 0x40) { // 0x4000 selects 62/100 of the stored tick value.
                const auto stored = std::int16_t(read_word(material.raw, 0x1c));
                // Original 49bae6 loads control word 0x1f1f: _frndint truncates toward zero.
                // The exact value is stored * 31 / 50. Its fractional part is a multiple of
                // 0.02, so no double rounding can cross an integer: double error is below
                // 1e-11 and ties (stored multiple of 25) are exactly representable. The
                // exhaustive check in tests/native_model_bindings.cpp proves this on any
                // IEEE-754 build, including Windows x64 where long double is double.
                const auto scaled = std::int32_t((int(stored) * 31) / 50);
                write_word(material.raw, 0x1c, std::uint16_t(scaled));
            }
            write_word(material.raw, 0x1a, bank);
            if (material.raw[0x10] & 1) {
                const auto key = material.raw[0x34 + texture_level];
                for (std::size_t slot = 0; slot < result.textures.size(); ++slot) {
                    const auto& texture = result.textures[slot];
                    const auto sequence = std::int32_t(std::uint32_t(texture.sequence) << 16) >> 16;
                    if (std::uint32_t(key) != std::uint32_t(sequence)) continue;
                    write_word(material.raw, 0x18, std::uint16_t(slot));
                    if (texture.flags & 0x20) material.raw[0x11] |= 0x02; // Runtime cutout flag 0x200.
                    write_word(material.raw, 0x20, texture.width_bits);
                    write_word(material.raw, 0x22, texture.height_bits);
                    write_word(material.raw, 0x24, dimension_mask(texture.width_bits));
                    write_word(material.raw, 0x26, dimension_mask(texture.height_bits));
                    material.texture = slot;
                    break;
                }
                if (!material.texture) material.raw[0x10] &= 0xfe; // Missing texture falls back to flat.
            }
            prepared.materials.push_back(std::move(material));
        }
        result.lods.push_back(std::move(prepared));
    }
    // Original 486880 texture pass; the bank palette is already installed by 480600.
    // Records with flags 0x10 or 0x80 are skipped. The 0x80 share path is unreachable in the
    // shipped executable, but a stored 0x80 byte would skip exactly like the original.
    for (auto& texture : result.textures) {
        if ((texture.flags & 0x10) || (texture.flags & 0x80)) continue;
        AssetBytes source(768, 0);
        std::copy(texture.palette_rgb.begin(), texture.palette_rgb.end(), source.begin());
        const auto table = remap_source_entries(source, palette, gamma);
        std::copy(table.begin(), table.end(), texture.palette_remap.begin());
        for (auto& pixel : texture.pixels) pixel = texture.palette_remap[pixel];
        texture.most_frequent = most_frequent_texel(texture.pixels);
        texture.remapped = true;
    }
    // Original 486880 material pass: flat colour remap, texture index, effect colour.
    std::unordered_map<std::uint32_t, std::size_t> flat_slots;
    std::vector<std::array<std::uint8_t, 3>> flat_colours;
    for (const auto& lod : result.lods)
        for (const auto& material : lod.materials) {
            if (read_word(material.raw, 0x1a) != bank) continue;
            if ((material.raw[0x11] & 0x80) || (material.raw[0x10] & 1)) continue;
            const auto offset = 0x28 + 3 * texture_level;
            const std::array<std::uint8_t, 3> colour{material.raw[offset], material.raw[offset + 1], material.raw[offset + 2]};
            const auto key = std::uint32_t(colour[0]) | std::uint32_t(colour[1]) << 8 | std::uint32_t(colour[2]) << 16;
            if (flat_slots.count(key)) continue;
            flat_slots.emplace(key, flat_colours.size());
            flat_colours.push_back(colour);
        }
    std::vector<std::uint8_t> flat_remaps(flat_colours.size(), 0);
    for (std::size_t base = 0; base < flat_colours.size(); base += 255) {
        const auto count = std::min<std::size_t>(255, flat_colours.size() - base);
        AssetBytes source(768, 0);
        for (std::size_t i = 0; i < count; ++i)
            for (unsigned channel = 0; channel < 3; ++channel)
                source[(i + 1) * 3 + channel] = flat_colours[base + i][channel];
        const auto table = remap_source_entries(source, palette, gamma);
        for (std::size_t i = 0; i < count; ++i) flat_remaps[base + i] = table[i + 1];
    }
    for (auto& lod : result.lods)
        for (auto& material : lod.materials) {
            if (read_word(material.raw, 0x1a) != bank) continue;
            if (!(material.raw[0x11] & 0x80)) {
                if (!(material.raw[0x10] & 1)) {
                    const auto offset = 0x28 + 3 * texture_level;
                    const auto key = std::uint32_t(material.raw[offset]) |
                        std::uint32_t(material.raw[offset + 1]) << 8 | std::uint32_t(material.raw[offset + 2]) << 16;
                    write_dword(material.raw, 0x14, flat_remaps[flat_slots.at(key)]);
                } else {
                    if (!material.texture || *material.texture >= result.textures.size())
                        throw std::invalid_argument("textured material lost its loaded texture");
                    write_dword(material.raw, 0x14, result.textures[*material.texture].most_frequent);
                }
            } else {
                const auto offset = 0x28 + 3 * texture_level;
                const auto red = material.raw[offset], green = material.raw[offset + 1], blue = material.raw[offset + 2];
                const auto luminance = std::uint32_t(red) + 2u * std::uint32_t(green) + std::uint32_t(blue);
                const auto packed = std::uint32_t(blue) | std::uint32_t(green) << 8 | std::uint32_t(red) << 16 |
                    luminance << 24;
                write_dword(material.raw, 0x14, packed);
                material.raw[0x10] = std::uint8_t((material.raw[0x10] & 0xfb) | (luminance > 0x7f ? 4 : 0));
            }
            material.flags = read_dword(material.raw, 0x10);
            material.bank = read_word(material.raw, 0x1a);
            material.flat_index = material.raw[0x14];
            material.bound_colour = read_dword(material.raw, 0x14);
        }
    return result;
}

} // namespace dfre
