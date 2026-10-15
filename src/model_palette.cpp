#include "dfre/model_palette.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace dfre {
void gather_model_palette_color(PaletteQuantizer6& quantizer,
    const std::array<std::uint8_t,3>& rgb,const ModelGammaTable& gamma,std::int32_t weight) {
    const std::array<std::uint8_t,3> color{gamma[rgb[0]],gamma[rgb[1]],gamma[rgb[2]]};
    quantizer.add_color(color,weight);
    // 47E8E0 repeats the first shade 31 times for each of flags 2 and 8.
    for(unsigned i=0;i<62;++i)quantizer.add_color(color,0x4000);
}
void gather_model_texture_palette(PaletteQuantizer6& quantizer,
    const ModelTexture& texture,const ModelGammaTable& gamma) {
    if(texture.pixels.empty() || texture.pixels.size()>std::size_t(std::numeric_limits<std::int32_t>::max()))
        throw std::invalid_argument("invalid model texture size");
    std::array<std::uint32_t,256> counts{};
    for(auto index:texture.pixels)++counts[index];
    for(std::size_t i=0;i<256;++i)gather_model_palette_color(quantizer,
        {texture.palette_rgb_pad[i*4],texture.palette_rgb_pad[i*4+1],texture.palette_rgb_pad[i*4+2]},
        gamma,palette_frequency_weight(counts[i],std::uint32_t(texture.pixels.size())));
}
std::optional<std::size_t> model_material_texture(const Model& model,
    const ModelMaterial& material,unsigned texture_level) {
    if(texture_level>3)throw std::invalid_argument("invalid model texture level");
    if(!(material.flags&1))return {};
    const auto key=material.raw[52+texture_level];
    for(std::size_t i=0;i<model.textures.size();++i)
        if((model.textures[i].flags&(1u<<texture_level)) && model.textures[i].sequence==key)return i;
    return {};
}
ModelPaletteBanks32 build_model_palette_banks32(const std::vector<ModelPaletteInput>& models,
    const ModelGammaTable& gamma,const std::array<std::uint8_t,3>& gamma_light,
    unsigned texture_level,const TerrainColorControl& control) {
    if(texture_level>3)throw std::invalid_argument("invalid model texture level");
    for(const auto& source:models)if(!source.model || source.bank>=6)
        throw std::invalid_argument("invalid model palette input");
    PaletteQuantizer6 quantizer;ModelPaletteBanks32 banks;
    for(std::size_t bank=0;bank<banks.size();++bank) {
        quantizer.clear();
        for(const auto& source:models)if(source.bank==bank)
            for(const auto& texture:source.model->textures)
                if((texture.flags&(1u<<texture_level)) && !(texture.flags&0x10))
                    gather_model_texture_palette(quantizer,texture,gamma);
        for(const auto& source:models)if(source.bank==bank)
            for(const auto& lod:source.model->lods)for(const auto& material:lod.materials)
                if(!model_material_texture(*source.model,material,texture_level))
                    gather_model_palette_color(quantizer,
                        {material.raw[40+texture_level*3],material.raw[41+texture_level*3],material.raw[42+texture_level*3]},gamma);
        const auto colors=quantizer.build();auto& output=banks[bank];output.gamma_rgb.resize(768);
        std::copy(colors.begin(),colors.end(),output.gamma_rgb.begin()+3);
        for(std::size_t channel=0;channel<3;++channel){output.gamma_rgb[channel]=gamma[0];output.gamma_rgb[765+channel]=gamma[255];}
        output.display_rgb.resize(768);output.matching_rgb.resize(768);
        for(std::size_t i=0;i<768;++i) {
            // 401C60 copies the original inverse table from 4CEBE0.
            output.display_rgb[i]=std::uint8_t(std::pow(double(output.gamma_rgb[i])/255.0,0.8)*255.0+0.5);
            output.matching_rgb[i]=gamma[output.display_rgb[i]];
        }
        // 4809B0 builds rows 0..31 with 480490, then rows 32..63 with
        // 4801B0, whose target is the scene light color at C22AD0.
        output.shades=build_terrain_shades32(output.gamma_rgb,gamma_light,control);
    }
    return banks;
}
}
