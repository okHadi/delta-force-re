#include "dfre/palette_quantizer.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::uint64_t hash(const std::vector<std::uint8_t>& bytes){
    std::uint64_t value=14695981039346656037ull;
    for(auto byte:bytes)value=(value^byte)*1099511628211ull;
    return value;
}
}
int main(){try{
    dfre::PaletteQuantizer6 quantizer;
    check(hash(quantizer.build(4))==0x4c4b959e935c8225ull,"Four-color grayscale palette differs");
    quantizer.clear();
    check(hash(quantizer.build())==0xc675084c4fdc66cdull,"Empty bank retained boxes differ");
    check(quantizer.active_boxes()==64 && quantizer.occupied_bins()==64 && quantizer.maximum_weight()==64,"Grayscale histogram differs");
    quantizer.clear();quantizer.add_color({255,0,0});
    check(hash(quantizer.build(1))==0x9be30a196b51e8bbull,"Single-color box midpoint differs");
    quantizer.clear();
    for(unsigned i=0;i<512;++i)quantizer.add_color({std::uint8_t(i),std::uint8_t(i*37),std::uint8_t(i*113)});
    check(hash(quantizer.build())==0x3206777204f34761ull,"Full original palette differs");
    check(quantizer.active_boxes()==254 && quantizer.occupied_bins()==316 && quantizer.maximum_weight()==96,"Full palette histogram differs");
    quantizer.clear();
    check(hash(quantizer.build())==0x44b3e290982645d1ull,"Previous bank boxes were not retained");
    bool rejected=false;try{quantizer.build();}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"A second build requires a reset");
    quantizer.clear();rejected=false;try{quantizer.build(257);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Palette size limit was not checked");
    std::cout<<"palette quantizer checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
