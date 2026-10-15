#include "dfre/model_palette.hpp"
#include <iostream>
#include <stdexcept>
namespace {unsigned read(){unsigned value;if(!(std::cin>>value))throw std::invalid_argument("missing input");return value;}
std::uint8_t byte(){auto value=read();if(value>255)throw std::invalid_argument("invalid byte");return std::uint8_t(value);}}
int main(){try{
    std::size_t count;unsigned level;
    while(std::cin>>count>>level){
        if(count>600)return 2;
        dfre::ModelGammaTable gamma;for(auto& v:gamma)v=byte();
        std::array<std::uint8_t,3> light;for(auto& v:light)v=byte();
        std::vector<dfre::Model> models(count);std::vector<dfre::ModelPaletteInput> inputs;
        for(auto& model:models){
            const auto bank=byte();const auto textures=read(),materials=read();if(textures>2000 || materials>65536)return 2;
            inputs.push_back({&model,bank});
            for(unsigned i=0;i<textures;++i){dfre::ModelTexture texture;texture.flags=byte();texture.sequence=std::uint16_t(read());auto size=read();if(size>1048576)return 2;
                for(unsigned j=0;j<256;++j)for(unsigned k=0;k<3;++k)texture.palette_rgb_pad[j*4+k]=byte();
                texture.pixels.resize(size);for(auto& v:texture.pixels)v=byte();model.textures.push_back(std::move(texture));}
            model.lods.resize(1);
            for(unsigned i=0;i<materials;++i){dfre::ModelMaterial material;material.flags=read();for(unsigned j=40;j<56;++j)material.raw[j]=byte();model.lods[0].materials.push_back(material);}
        }
        auto banks=dfre::build_model_palette_banks32(inputs,gamma,light,level);
        bool first=true;for(const auto& bank:banks){
            for(const auto* palette:{&bank.gamma_rgb,&bank.display_rgb,&bank.matching_rgb})
                for(auto v:*palette){if(!first)std::cout<<' ';first=false;std::cout<<unsigned(v);}
            std::uint64_t hash=14695981039346656037ull;
            for(auto color:bank.shades)for(unsigned shift=0;shift<32;shift+=8)
                hash=(hash^std::uint8_t(color>>shift))*1099511628211ull;
            std::cout<<' '<<hash;
        }std::cout<<'\n';
    }return std::cin.eof()?0:2;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
