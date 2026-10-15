#include "dfre/model_bindings.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
unsigned read() {
    unsigned value;
    if (!(std::cin >> value)) throw std::invalid_argument("missing input");
    return value;
}

std::uint8_t byte() {
    const auto value = read();
    if (value > 255) throw std::invalid_argument("invalid byte");
    return std::uint8_t(value);
}

dfre::Model read_model() {
    dfre::Model model;
    const auto texture_count = read();
    if (texture_count > 2000) throw std::invalid_argument("too many textures");
    for (unsigned i = 0; i < texture_count; ++i) {
        std::array<std::uint32_t, 4> name{};
        for (auto& value : name) value = read();
        std::string text(reinterpret_cast<const char*>(name.data()), 16);
        const auto end = text.find('\0');
        if (end != std::string::npos) text.resize(end);
        dfre::ModelTexture texture;
        texture.name = text;
        const auto count = read();
        if (count > 1048576) throw std::invalid_argument("texture too large");
        texture.sequence = std::uint16_t(read());
        texture.flags = byte();
        texture.width = std::uint16_t(read());
        texture.height = std::uint16_t(read());
        for (unsigned j = 0; j < 256; ++j)
            for (unsigned channel = 0; channel < 3; ++channel)
                texture.palette_rgb_pad[std::size_t(j) * 4 + channel] = byte();
        texture.pixels.resize(count);
        for (auto& value : texture.pixels) value = byte();
        model.textures.push_back(std::move(texture));
    }
    const auto lods = read();
    if (lods > 64) throw std::invalid_argument("too many lods");
    model.lods.resize(lods);
    for (auto& lod : model.lods) {
        const auto materials = read();
        if (materials > 65536) throw std::invalid_argument("too many materials");
        for (unsigned i = 0; i < materials; ++i) {
            dfre::ModelMaterial material;
            for (auto& value : material.raw) value = byte();
            lod.materials.push_back(std::move(material));
        }
    }
    return model;
}
} // namespace

int main() {
    try {
        unsigned models, bank, level;
        while (std::cin >> models >> bank >> level) {
            if (models < 1 || models > 16 || bank > 5 || level > 3) return 2;
            dfre::ModelGammaTable gamma;
            for (auto& value : gamma) value = byte();
            dfre::ModelPaletteBank32 palette;
            palette.matching_rgb.resize(768);
            for (auto& value : palette.matching_rgb) value = byte();
            palette.shades.resize(64 * 256);
            for (auto& value : palette.shades) value = read();
            std::vector<dfre::Model> cases;
            for (unsigned i = 0; i < models; ++i) cases.push_back(read_model());
            std::vector<dfre::ModelPreparedModel32> prepared;
            for (const auto& model : cases)
                prepared.push_back(dfre::prepare_model_bindings32(model, std::uint8_t(bank), level, palette, gamma));
            bool first = true;
            auto emit = [&](long long value) {
                if (!first) std::cout << ' ';
                first = false;
                std::cout << value;
            };
            for (const auto& model : prepared)
                for (const auto& texture : model.textures) {
                    emit(texture.remapped ? 1 : 0);
                    emit(texture.most_frequent);
                    emit(texture.width_bits);
                    emit(texture.height_bits);
                    emit(static_cast<long long>(texture.pixels.size()));
                    for (const auto value : texture.pixels) emit(value);
                }
            for (const auto& model : prepared)
                for (const auto& lod : model.lods)
                    for (const auto& material : lod.materials) {
                        for (const auto value : material.raw) emit(value);
                        emit(material.texture ? static_cast<long long>(*material.texture) : -1);
                        emit(material.flat_index);
                        emit(material.bound_colour);
                    }
            std::cout << '\n';
        }
        return std::cin.eof() ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
