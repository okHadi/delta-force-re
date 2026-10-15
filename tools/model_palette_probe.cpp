#include "dfre/model_palette.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
unsigned byte(){unsigned value;if(!(std::cin>>value) || value>255)throw std::invalid_argument("invalid byte");return value;}
std::uint64_t bits(double value){std::uint64_t out;std::memcpy(&out,&value,sizeof(out));return out;}
}
int main(){try{
    dfre::PaletteQuantizer6 quantizer;std::size_t colors,flat_count,texture_count;
    while(std::cin>>colors>>flat_count>>texture_count){
        if(flat_count>65536 || texture_count>1024)return 2;
        dfre::ModelGammaTable gamma;for(auto& v:gamma)v=std::uint8_t(byte());
        quantizer.clear();
        for(std::size_t i=0;i<flat_count;++i){std::array<std::uint8_t,3> rgb;for(auto& v:rgb)v=std::uint8_t(byte());std::int32_t weight;if(!(std::cin>>weight))return 2;dfre::gather_model_palette_color(quantizer,rgb,gamma,weight);}
        for(std::size_t i=0;i<texture_count;++i){
            dfre::ModelTexture texture;std::size_t size;if(!(std::cin>>size) || size>1048576)return 2;
            for(std::size_t j=0;j<256;++j)for(std::size_t k=0;k<3;++k)texture.palette_rgb_pad[j*4+k]=std::uint8_t(byte());
            texture.pixels.resize(size);for(auto& v:texture.pixels)v=std::uint8_t(byte());
            dfre::gather_model_texture_palette(quantizer,texture,gamma);
        }
        auto result=quantizer.build(colors);std::cout<<quantizer.active_boxes()<<' '<<quantizer.occupied_bins()<<' '<<quantizer.maximum_weight();
        for(auto value:result)std::cout<<' '<<unsigned(value);
        for(std::size_t i=0;i<quantizer.active_boxes();++i){const auto& box=quantizer.boxes()[i];std::cout<<' '<<bits(box.variance);for(auto v:box.moments)std::cout<<' '<<bits(v);for(auto v:box.minimum)std::cout<<' '<<v;for(auto v:box.maximum)std::cout<<' '<<v;std::cout<<' '<<box.splittable;}
        std::cout<<'\n';
    }return std::cin.eof()?0:2;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
