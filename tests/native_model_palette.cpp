#include "dfre/model_palette.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint64_t hash(const dfre::AssetBytes& bytes){std::uint64_t value=14695981039346656037ull;for(auto byte:bytes)value=(value^byte)*1099511628211ull;return value;}
}
int main(){try{
    check(dfre::palette_frequency_weight(1,1)==65536,"Single sample frequency differs");
    check(dfre::palette_frequency_weight(3,3)==65535,"Original extended precision frequency differs");
    check(dfre::palette_frequency_weight(0,3)==0,"Unused color frequency differs");
    dfre::ModelGammaTable gamma;for(unsigned i=0;i<256;++i)gamma[i]=std::uint8_t(i*i/255);
    dfre::Model model;dfre::ModelTexture texture;texture.flags=1;texture.sequence=7;texture.pixels.assign(16,1);
    for(unsigned i=0;i<256;++i)for(unsigned c=0;c<3;++c)texture.palette_rgb_pad[i*4+c]=std::uint8_t(i*7+c*19);
    model.textures.push_back(texture);model.lods.resize(1);
    dfre::ModelMaterial material;for(unsigned i=0;i<4;++i){material.raw[40+i*3]=100;material.raw[41+i*3]=30;material.raw[42+i*3]=22;material.raw[52+i]=7;}
    model.lods[0].materials.push_back(material);material.flags=1;model.lods[0].materials.push_back(material);
    const auto banks=dfre::build_model_palette_banks32({{&model,2}},gamma,{100,100,100});
    const std::array<std::uint64_t,6> expected{0x1d1d5910289808a6ull,0x1d1d5910289808a6ull,
        0xb45d580387294602ull,0xb4acf4711974375eull,0xb4acf4711974375eull,0xb4acf4711974375eull};
    for(unsigned i=0;i<6;++i){check(hash(banks[i].gamma_rgb)==expected[i],"Original shared palette bank differs");check(banks[i].shades.size()==64*256,"Palette shade table size differs");}
    model.textures[0].sequence=263;
    texture.flags=2;model.textures.push_back(texture);texture.flags=1;model.textures.push_back(texture);
    check(dfre::model_material_texture(model,material,0)==std::optional<std::size_t>(2),"Texture selection requires the full sequence and level");
    check(dfre::model_material_texture(model,material,1)==std::optional<std::size_t>(1),"Texture level selection differs");
    check(!dfre::model_material_texture(model,material,2),"Missing texture must use flat color");
    bool rejected=false;try{dfre::build_model_palette_banks32({{nullptr,0}},gamma,{0,0,0});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Invalid palette model was accepted");
    std::cout<<"model palette checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
