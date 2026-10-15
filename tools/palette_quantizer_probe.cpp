#include "dfre/palette_quantizer.hpp"
#include <cstring>
#include <iostream>
#include <string>
namespace {std::uint64_t bits(double value){std::uint64_t out;std::memcpy(&out,&value,sizeof(out));return out;}}
int main(int argc,char** argv){try{
    if(argc>2 || (argc==2 && std::string(argv[1])!="--boxes"))return 2;
    dfre::PaletteQuantizer6 quantizer;std::size_t colors,count;
    while(std::cin>>colors>>count){
        if(count>1000000)return 2;quantizer.clear();
        for(std::size_t i=0;i<count;++i){unsigned r,g,b;std::int32_t weight;if(!(std::cin>>r>>g>>b>>weight) || r>255 || g>255 || b>255)return 2;quantizer.add_color({std::uint8_t(r),std::uint8_t(g),std::uint8_t(b)},weight);}
        const auto result=quantizer.build(colors);std::cout<<quantizer.active_boxes()<<' '<<quantizer.occupied_bins()<<' '<<quantizer.maximum_weight();
        for(auto value:result)std::cout<<' '<<unsigned(value);
        if(argc==2)for(std::size_t i=0;i<quantizer.active_boxes();++i){const auto& box=quantizer.boxes()[i];std::cout<<' '<<bits(box.variance);for(auto v:box.moments)std::cout<<' '<<bits(v);for(auto v:box.minimum)std::cout<<' '<<v;for(auto v:box.maximum)std::cout<<' '<<v;std::cout<<' '<<box.splittable;}
        std::cout<<'\n';
    }return std::cin.eof()?0:2;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
