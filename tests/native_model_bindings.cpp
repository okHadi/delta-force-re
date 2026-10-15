#include "dfre/model_bindings.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

dfre::ModelGammaTable identity_gamma() {
    dfre::ModelGammaTable gamma{};
    for (unsigned i = 0; i < 256; ++i) gamma[i] = std::uint8_t(i);
    return gamma;
}

dfre::ModelPaletteBank32 make_bank() {
    dfre::ModelPaletteBank32 bank;
    bank.gamma_rgb.resize(768, 0);
    bank.display_rgb.resize(768, 0);
    bank.matching_rgb.resize(768, 0);
    bank.shades.resize(64 * 256);
    for (std::size_t i = 0; i < bank.shades.size(); ++i) bank.shades[i] = std::uint32_t(i);
    return bank;
}

void set_palette_entry(dfre::ModelPaletteBank32& bank, unsigned index, std::uint8_t r, std::uint8_t g,
    std::uint8_t b) {
    bank.gamma_rgb[index * 3] = r;
    bank.gamma_rgb[index * 3 + 1] = g;
    bank.gamma_rgb[index * 3 + 2] = b;
    // Identity gamma and display mapping keep matching_rgb equal to gamma_rgb here.
    bank.display_rgb[index * 3] = r;
    bank.display_rgb[index * 3 + 1] = g;
    bank.display_rgb[index * 3 + 2] = b;
    bank.matching_rgb[index * 3] = r;
    bank.matching_rgb[index * 3 + 1] = g;
    bank.matching_rgb[index * 3 + 2] = b;
}

dfre::ModelMaterial make_material(std::uint32_t flags, std::uint8_t key_level0, const std::array<std::uint8_t, 3>& flat) {
    dfre::ModelMaterial material;
    material.raw[0x10] = std::uint8_t(flags);
    material.raw[0x11] = std::uint8_t(flags >> 8);
    material.raw[0x12] = std::uint8_t(flags >> 16);
    material.raw[0x13] = std::uint8_t(flags >> 24);
    material.raw[0x34] = key_level0;
    material.raw[0x28] = flat[0];
    material.raw[0x29] = flat[1];
    material.raw[0x2a] = flat[2];
    return material;
}

dfre::ModelTexture make_texture(std::string name, std::uint16_t sequence, std::uint8_t flags, std::uint16_t width,
    std::uint16_t height, std::vector<std::uint8_t> pixels) {
    dfre::ModelTexture texture;
    texture.name = std::move(name);
    texture.sequence = sequence;
    texture.flags = flags;
    texture.width = width;
    texture.height = height;
    texture.pixels = std::move(pixels);
    return texture;
}

// Spread identifier palette: exact queries always remap to their own index, and the
// bank keeps the original candidate bound far below its 131,072 entry pool.
dfre::ModelPaletteBank32 make_spread_bank() {
    auto bank = make_bank();
    unsigned state = 2654435761u + 1;
    for (unsigned i = 1; i < 256; ++i)
        for (unsigned channel = 0; channel < 3; ++channel) {
            state = state * 1664525u + 1013904223u;
            bank.gamma_rgb[i * 3 + channel] = std::uint8_t(state >> 24);
            bank.display_rgb[i * 3 + channel] = std::uint8_t(state >> 24);
            bank.matching_rgb[i * 3 + channel] = std::uint8_t(state >> 24);
        }
    return bank;
}

// Source entries use the matching table colours, so an identity gamma keeps the remap identity.
void link_texture_palette(dfre::ModelTexture& texture, const dfre::ModelPaletteBank32& bank) {
    for (unsigned i = 0; i < 256; ++i)
        for (unsigned channel = 0; channel < 3; ++channel)
            texture.palette_rgb_pad[i * 4 + channel] = bank.matching_rgb[i * 3 + channel];
}
} // namespace

int main() {
    const auto gamma = identity_gamma();
    {
        dfre::Model model;
        model.textures.push_back(make_texture("a", 7, 0x01, 8, 8, {1}));
        model.textures.push_back(make_texture("b", 8, 0x04, 8, 8, {2}));
        model.textures.push_back(make_texture("c", 9, 0x05, 8, 8, {3}));
        const auto span = dfre::model_texture_span32(model, 0);
        check(span == std::vector<std::size_t>({0, 2}), "level 0 span changed");
        check(dfre::model_texture_span32(model, 2) == std::vector<std::size_t>({1, 2}), "level 2 span changed");
        check(dfre::model_texture_span32(model, 3).empty(), "level 3 span changed");
        try {
            dfre::model_texture_span32(model, 4);
            check(false, "level above three accepted");
        } catch (const std::invalid_argument&) {}
    }
    {
        // Bank palette entries 5, 7, and 9 are the only reachable matches.
        auto bank = make_spread_bank();
        set_palette_entry(bank, 5, 10, 20, 30);
        set_palette_entry(bank, 7, 10, 20, 31);
        set_palette_entry(bank, 9, 200, 100, 50);
        dfre::Model model;
        model.lods.resize(1);
        model.lods[0].materials.push_back(make_material(0, 0, {10, 20, 30}));   // Exact, entry 5.
        model.lods[0].materials.push_back(make_material(0, 0, {10, 20, 32}));   // Closer to entry 7.
        model.lods[0].materials.push_back(make_material(0, 0, {200, 100, 50})); // Exact, entry 9.
        const auto prepared = dfre::prepare_model_bindings32(model, 0, 0, bank, gamma);
        check(prepared.textures.empty(), "flat model loaded textures");
        check(prepared.lods[0].materials[0].flat_index == 5, "exact flat index changed");
        check(prepared.lods[0].materials[1].flat_index == 7, "nearest flat index changed");
        check(prepared.lods[0].materials[2].flat_index == 9, "second exact flat index changed");
        check(prepared.lods[0].materials[0].bound_colour == 5, "flat bound colour changed");
        check(!prepared.lods[0].materials[0].texture, "flat material has a texture");
        const auto view = prepared.raster_material(0, 0);
        check(view.texture == nullptr && view.shade_colours == &bank.shades, "flat raster binding changed");
        check(view.tiny_colour == std::uint32_t(32 * 256 + 5), "tiny colour row changed");
        check(model.lods[0].materials[0].raw[0x14] == 0, "source material changed");
    }
    {
        // Texture remap and most frequent texel, with the spread identifier palette.
        auto bank = make_spread_bank();
        dfre::Model model;
        auto texture = make_texture("tex0", 40, 0x21, 8, 16, {3, 3, 5, 0});
        link_texture_palette(texture, bank);
        model.textures.push_back(std::move(texture));
        model.lods.resize(1);
        model.lods[0].materials.push_back(make_material(1, 40, {0, 0, 0}));
        model.lods[0].materials.push_back(make_material(1, 41, {0, 0, 0}));
        const auto prepared = dfre::prepare_model_bindings32(model, 0, 0, bank, gamma);
        check(prepared.textures.size() == 1, "loaded texture count changed");
        const auto& prepared_texture = prepared.textures[0];
        check(prepared_texture.pixels == std::vector<std::uint8_t>({3, 3, 5, 0}), "texture remap changed");
        check(prepared_texture.palette_remap[1] == 1 && prepared_texture.palette_remap[3] == 3, "texture remap table changed");
        check(prepared_texture.palette_remap[255] == 255, "texture remap identity changed");
        check(prepared_texture.most_frequent == 3, "most frequent texel changed");
        check(prepared_texture.width_bits == 3 && prepared_texture.height_bits == 4, "texture dimension bits changed");
        const auto& matched = prepared.lods[0].materials[0];
        check(matched.texture && *matched.texture == 0, "matched material lost its texture");
        check(matched.raw[0x18] == 0 && matched.raw[0x19] == 0, "material texture index changed");
        check(matched.cutout() && matched.raw[0x11] == 0x02, "texture cutout flag changed");
        check(matched.raw[0x20] == 3 && matched.raw[0x21] == 0, "material width bits changed");
        check(matched.raw[0x22] == 4, "material height bits changed");
        check(matched.raw[0x24] == 7 && matched.raw[0x25] == 0, "material width mask changed");
        check(matched.raw[0x26] == 15, "material height mask changed");
        check(matched.flat_index == 3, "textured flat index changed");
        const auto& missing = prepared.lods[0].materials[1];
        check(!missing.texture && !missing.has_texture(), "missing texture fallback changed");
        check(missing.raw[0x10] == 0, "missing texture flag not cleared");
        const auto view = prepared.raster_material(0, 0);
        check(view.texture && view.texture->width_bits == 3 && view.texture->height_bits == 4, "raster texture view changed");
        check(view.texture->pixels == std::vector<std::uint8_t>({3, 3, 5, 0}), "raster texture pixels changed");
        check(view.texture->shade_colours == bank.shades, "raster texture shade rows changed");
        check(view.flat_index == 3 && view.tiny_colour == std::uint32_t(32 * 256 + 3), "tiny texture colour changed");
    }
    {
        // Sign extended sequence 0x8100 never matches a byte key; all zero pixels keep index zero.
        auto bank = make_spread_bank();
        dfre::Model model;
        auto texture = make_texture("high", 0x8100, 0x01, 64, 64, {0, 0, 0, 0});
        link_texture_palette(texture, bank);
        model.textures.push_back(std::move(texture));
        model.lods.resize(1);
        model.lods[0].materials.push_back(make_material(1, 0x00, {0, 0, 0}));
        const auto prepared = dfre::prepare_model_bindings32(model, 0, 0, bank, gamma);
        check(prepared.textures[0].most_frequent == 0, "empty texture most frequent changed");
        check(!prepared.lods[0].materials[0].has_texture(), "high sequence matched a byte key");
        check(prepared.lods[0].materials[0].raw[0x18] == 0 && prepared.lods[0].materials[0].raw[0x19] == 0,
            "cleared material kept a texture index");
    }
    {
        // Effect materials pack B, G, R and the R + 2G + B luminance byte, and set flag 0x4 above 127.
        auto bank = make_bank();
        dfre::Model model;
        model.lods.resize(1);
        model.lods[0].materials.push_back(make_material(0x8000, 0, {200, 100, 50}));
        model.lods[0].materials.push_back(make_material(0x8000, 0, {10, 10, 10}));
        const auto prepared = dfre::prepare_model_bindings32(model, 0, 0, bank, gamma);
        const auto& bright = prepared.lods[0].materials[0];
        check(bright.bound_colour == 0xc2c86432u, "effect colour packing changed");
        check(bright.raw[0x14] == 50 && bright.raw[0x15] == 100 && bright.raw[0x16] == 200 && bright.raw[0x17] == 0xc2,
            "effect colour bytes changed");
        check((bright.flags & 4) != 0, "bright effect flag not set");
        const auto& dark = prepared.lods[0].materials[1];
        check(dark.bound_colour == 0x280a0a0au, "dark effect colour changed");
        check((dark.flags & 4) == 0, "dark effect flag set");
        check(dark.effect() && bright.effect(), "effect flag helper changed");
    }
    {
        // 0x4000 trims the stored tick value with original truncation toward zero.
        auto bank = make_bank();
        dfre::Model model;
        model.lods.resize(1);
        for (const int stored : {237, 50, -50, 1, 16}) {
            auto material = make_material(0x4001, 0, {0, 0, 0});
            material.raw[0x1c] = std::uint8_t(stored);
            material.raw[0x1d] = std::uint8_t(stored >> 8);
            model.lods[0].materials.push_back(material);
        }
        const auto prepared = dfre::prepare_model_bindings32(model, 0, 0, bank, gamma);
        const std::array<std::uint16_t, 5> expected{146, 31, 0xffe1, 0, 9};
        for (std::size_t i = 0; i < expected.size(); ++i) {
            const auto value = std::uint16_t(prepared.lods[0].materials[i].raw[0x1c]) |
                std::uint16_t(prepared.lods[0].materials[i].raw[0x1d]) << 8;
            check(value == expected[i], "tick scale changed");
        }
        // Exhaustive proof for every stored 16-bit value: the exact integer form equals both
        // the double and the extended precision forms. The exact value is stored * 31 / 50;
        // its fractional part is a multiple of 0.02, so neither IEEE form can cross an integer
        // boundary. Windows x64 MSVC has long double == double and still passes.
        model.lods[0].materials.clear();
        for (int stored = -32768; stored <= 32767; ++stored) {
            auto material = make_material(0x4001, 0, {0, 0, 0});
            material.raw[0x1c] = std::uint8_t(stored);
            material.raw[0x1d] = std::uint8_t(stored >> 8);
            model.lods[0].materials.push_back(material);
        }
        const auto scaled = dfre::prepare_model_bindings32(model, 0, 0, bank, gamma);
        for (int stored = -32768; stored <= 32767; ++stored) {
            const auto index = std::size_t(stored + 32768);
            const auto& material = scaled.lods[0].materials[index];
            const auto value = std::int16_t(std::uint16_t(material.raw[0x1c]) |
                std::uint16_t(material.raw[0x1d]) << 8);
            const auto exact = std::int32_t((stored * 31) / 50);
            check(value == exact, "tick scale differs from the exact form");
            check(std::int32_t(std::trunc(double(stored) / 100.0 * 62.0)) == exact,
                "tick scale differs from the double form");
            check(std::int32_t(std::trunc(static_cast<long double>(stored) / 100.0L * 62.0L)) == exact,
                "tick scale differs from the extended form");
        }
    }
    {
        // Equal later name and size stay separate records: 0xc54484 is zero in the shipped
        // executable and the 485BA0 share cache never runs.
        auto bank = make_spread_bank();
        dfre::Model model;
        auto first = make_texture("share", 20, 0x01, 8, 8, {2, 2, 4});
        auto second = make_texture("SHARE", 21, 0x03, 8, 8, {9, 9, 9});
        link_texture_palette(first, bank);
        link_texture_palette(second, bank);
        model.textures.push_back(std::move(first));
        model.textures.push_back(std::move(second));
        model.lods.resize(1);
        model.lods[0].materials.push_back(make_material(1, 20, {0, 0, 0}));
        model.lods[0].materials.push_back(make_material(1, 21, {0, 0, 0}));
        const auto prepared = dfre::prepare_model_bindings32(model, 0, 0, bank, gamma);
        check((prepared.textures[1].flags & 0x80) == 0, "record marked shared with the cache off");
        check(prepared.textures[1].pixels == std::vector<std::uint8_t>({9, 9, 9}), "duplicate record remapped wrongly");
        check(prepared.textures[1].most_frequent == 9, "duplicate most frequent changed");
        check(prepared.textures[0].pixels == std::vector<std::uint8_t>({2, 2, 4}), "first record remapped wrongly");
        check(prepared.textures[0].most_frequent == 2, "first most frequent changed");
        check(prepared.lods[0].materials[1].flat_index == 9, "duplicate material flat index changed");
        check(prepared.lods[0].materials[1].texture && *prepared.lods[0].materials[1].texture == 1,
            "duplicate material texture slot changed");
    }
    {
        // Integration: prepared runtime material fields reach prepare_model_face routing.
        auto bank = make_spread_bank();
        dfre::Model model;
        auto cutout_texture = make_texture("cut", 30, 0x21, 8, 8, {1, 2, 3, 0});
        link_texture_palette(cutout_texture, bank);
        model.textures.push_back(std::move(cutout_texture));
        model.lods.resize(1);
        auto& materials = model.lods[0].materials;
        materials.push_back(make_material(1, 30, {10, 20, 30}));        // Cutout texture.
        materials.push_back(make_material(1, 31, {4, 5, 6}));           // Missing texture.
        materials.push_back(make_material(0x8001, 30, {200, 100, 50})); // Effect with texture.
        materials.push_back(make_material(0x4001, 30, {7, 8, 9}));      // Animated forward.
        materials.push_back(make_material(0, 30, {10, 20, 30}));        // Forward target.
        materials.push_back(make_material(0x4003, 30, {11, 12, 13}));   // Animated reverse.
        materials.push_back(make_material(0, 30, {14, 15, 16}));        // Reverse target.
        materials[3].raw[0x1f] = 2;   // Animation parameter two.
        materials[3].raw[0x1c] = 237; // Tick scales to 146.
        materials[5].raw[0x1f] = 127; // Reverse-side next material.
        const auto prepared = dfre::prepare_model_bindings32(model, 0, 0, bank, gamma);
        const auto applied = prepared.apply(0, model.lods[0]);
        check(applied.materials.size() == 7, "applied material count changed");
        try {
            prepared.apply(9, model.lods[0]);
            check(false, "prepared lod outside table accepted");
        } catch (const std::invalid_argument&) {}
        check((applied.materials[0].flags & 0x200) != 0, "cutout flag did not reach the applied lod");
        check((applied.materials[1].flags & 1) == 0, "missing texture flag1 not cleared in the applied lod");
        check((applied.materials[2].flags & 4) != 0, "effect bit4 did not reach the applied lod");
        check((applied.materials[2].flags & 0x8000) != 0, "effect flag lost in the applied lod");
        check(applied.materials[3].animation_parameter == 2, "animation parameter did not reach the applied lod");
        check(applied.materials[3].frame_count == model.lods[0].materials[3].frame_count, "frame count changed");
        const auto tick = std::uint16_t(applied.materials[3].raw[0x1c]) |
            std::uint16_t(applied.materials[3].raw[0x1d]) << 8;
        check(tick == 146, "scaled tick did not reach the applied lod");
        check(applied.materials[0].flat_rgb == std::array<std::uint8_t, 3>{10, 20, 30}, "flat rgb changed");
        check(applied.materials[2].raw[0x14] == 50 && applied.materials[2].raw[0x17] == 0xc2,
            "effect raw bytes did not reach the applied lod");
        check(applied.materials[0].texture_key == 30, "texture key changed");
        check(applied.vertices.size() == model.lods[0].vertices.size() &&
              applied.faces.size() == model.lods[0].faces.size() &&
              applied.meshes.size() == model.lods[0].meshes.size() &&
              applied.flags == model.lods[0].flags, "applied lod geometry changed");
        check(model.lods[0].materials[0].flags == 0 && (model.lods[0].materials[0].raw[0x11] & 0x02) == 0,
            "source materials changed");
        dfre::ModelFace face{};
        face.vertices = {0, 1, 2};
        face.normals = {0, 1, 2};
        std::vector<dfre::ModelProjectedVertex> projected(3);
        for (auto& point : projected) {
            point.camera[0] = 65536;
            point.projected = true;
        }
        projected[1].screen = {0, 2 * 65536};
        projected[2].screen = {2 * 65536, 0};
        std::vector<dfre::ModelNormal> normals{{{16384, 0, 0}, 0}, {{-16384, 0, 0}, 0}, {{0, 0, 0}, 0}};
        dfre::ModelLighting light{{4194304, 0, 0}, {0, 0, 0}, 0, 65536, 0};
        face.material = 0;
        auto assembled = dfre::prepare_model_face(face, projected, normals, 0, applied.materials, light);
        check(assembled && (assembled->flags & 0x20) != 0, "cutout flag did not reach face preparation");
        face.material = 1;
        assembled = dfre::prepare_model_face(face, projected, normals, 0, applied.materials, light);
        check(assembled && assembled->material == 1 && (assembled->flags & 0x10) == 0,
            "missing texture face routed as textured");
        face.material = 2;
        assembled = dfre::prepare_model_face(face, projected, normals, 0, applied.materials, light);
        check(assembled && (assembled->flags & 0x200) != 0, "effect flag did not reach face preparation");
        face.material = 3;
        std::array<std::uint32_t, 11> counters{};
        counters[2] = 1;
        assembled = dfre::prepare_model_face(face, projected, normals, 0, applied.materials, light, counters);
        check(assembled && assembled->material == 4, "animation counter did not advance the material");
        std::swap(projected[1], projected[2]);
        face.material = 5;
        assembled = dfre::prepare_model_face(face, projected, normals, 0, applied.materials, light);
        check(assembled && assembled->material == 6, "reverse parameter 127 did not reach face preparation");
    }
    {
        // Validation.
        auto bank = make_bank();
        dfre::Model model;
        try {
            dfre::prepare_model_bindings32(model, 6, 0, bank, gamma);
            check(false, "bank above five accepted");
        } catch (const std::invalid_argument&) {}
        try {
            dfre::prepare_model_bindings32(model, 0, 4, bank, gamma);
            check(false, "level above three accepted");
        } catch (const std::invalid_argument&) {}
        auto short_bank = bank;
        short_bank.matching_rgb.resize(3);
        try {
            dfre::prepare_model_bindings32(model, 0, 0, short_bank, gamma);
            check(false, "short bank palette accepted");
        } catch (const std::invalid_argument&) {}
        auto thin_bank = bank;
        thin_bank.shades.resize(1);
        try {
            dfre::prepare_model_bindings32(model, 0, 0, thin_bank, gamma);
            check(false, "short shade table accepted");
        } catch (const std::invalid_argument&) {}
        const auto prepared = dfre::prepare_model_bindings32(model, 0, 0, bank, gamma);
        try {
            prepared.material(0, 0);
            check(false, "missing material accepted");
        } catch (const std::invalid_argument&) {}
        dfre::ModelPreparedLod32 thin;
        thin.materials.resize(1);
        dfre::ModelLod source_lod;
        source_lod.materials.resize(2);
        try {
            dfre::apply_model_bindings32(source_lod, thin);
            check(false, "material count mismatch accepted");
        } catch (const std::invalid_argument&) {}
    }
    std::cout << "model bindings ok\n";
    return 0;
}
