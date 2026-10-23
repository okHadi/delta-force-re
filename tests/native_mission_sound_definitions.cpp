#include "dfre/mission_sound_definitions.hpp"
#include <array>
#include <algorithm>
#include <iostream>
int main(){try{
    using U=std::uint32_t;std::array<U,512*10> words{};unsigned loads=0,writes=0;
    for(U i=0;i<512;++i){words[i*10]=i;words[i*10+1]=0x10000+i;}
    std::swap_ranges(words.begin()+10,words.begin()+20,words.begin()+5110);
    dfre::MissionSoundAssetBindings b;
    b.call=[&](U site,const std::vector<U>& args){if(site!=0x43edb0||args!=std::vector<U>{123})throw std::runtime_error("Wrong ambient loader call");++loads;return 0;};
    b.read=[&](U at,U width){const auto index=(at-0x4f2ea4)/4;const auto n=words.at(index);return width==2?n&0xffff:n;};
    b.write=[&](U at,U width,U n){if(width!=4)throw std::runtime_error("Wrong definition store width");words.at((at-0x4f2ea4)/4)=n;++writes;};
    dfre::mission_sound_definitions_initialize(b,123);
    if(loads!=1||writes!=20||words[10]!=1||words[11]!=0x10001||words[5110]!=511||words[5111]!=0x101ff)throw std::runtime_error("Sound definitions differ");
    words[10]=0x8000;writes=0;dfre::mission_sound_definitions_initialize(b,123);if(writes)throw std::runtime_error("Missing signed definition changed storage");
    std::cout<<"sound definition checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
