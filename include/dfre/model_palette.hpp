#pragma once
#include "dfre/models.hpp"
#include "dfre/palette_quantizer.hpp"
#include "dfre/terrain.hpp"

namespace dfre {
using ModelGammaTable=std::array<std::uint8_t,256>;
// Original 486670 uses one color per 47E8E0 call, with flags 10.
void gather_model_palette_color(PaletteQuantizer6& quantizer,
    const std::array<std::uint8_t,3>& rgb,const ModelGammaTable& gamma,
    std::int32_t weight=65536);
void gather_model_texture_palette(PaletteQuantizer6& quantizer,
    const ModelTexture& texture,const ModelGammaTable& gamma);
struct ModelPaletteInput {
    const Model* model{};
    std::uint8_t bank{}; // Loader bank passed to original 486370.
};
struct ModelPaletteBank32 {
    AssetBytes gamma_rgb;
    AssetBytes display_rgb; // BFBB00: inverse gamma applied to BFBE00.
    AssetBytes matching_rgb; // 480600 applies forward gamma to display_rgb.
    std::vector<std::uint32_t> shades;
};
using ModelPaletteBanks32=std::array<ModelPaletteBank32,6>;
std::optional<std::size_t> model_material_texture(const Model& model,
    const ModelMaterial& material,unsigned texture_level);
ModelPaletteBanks32 build_model_palette_banks32(const std::vector<ModelPaletteInput>& models,
    const ModelGammaTable& gamma,const std::array<std::uint8_t,3>& gamma_light,
    unsigned texture_level=0,const TerrainColorControl& control={});
}
