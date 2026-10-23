#include "dfre/mission_sound_definitions.hpp"
#include <array>
#include <iostream>
namespace {using U=std::uint32_t;using H=std::uint64_t;
void add(H& h,U n){for(unsigned i=0;i<4;++i){h^=std::uint8_t(n>>(i*8));h*=1099511628211ull;}}
}
int main(){try{U seed;while(std::cin>>seed){
    std::array<U,5120> words{};for(U i=0;i<512;++i)for(U j=0;j<10;++j)words[i*10+j]=j?0xabc00000u+seed*5120+i*10+j:i;
    for(U k=0;k<3;++k){const U a=1+(seed*37+k*17)%511,c=1+(seed*13+k*71)%511;for(U j=0;j<10;++j)std::swap(words[a*10+j],words[c*10+j]);}
    if(seed%4==1)words[(1+seed%511)*10]=0x200;if(seed%4==2)words[(1+seed%511)*10]=0xffff;if(seed%4==3)words[(1+seed%511)*10]=0;
    H reads=14695981039346656037ull,writes=reads,image=reads;U read_count=0,write_count=0,calls=0;
    dfre::MissionSoundAssetBindings b;
    b.call=[&](U at,const std::vector<U>& args){if(at!=0x43edb0||args!=std::vector<U>{seed})throw std::runtime_error("Wrong loader call");++calls;words[9]^=seed;return 0;};
    b.read=[&](U at,U width){const U n=words.at((at-0x4f2ea4)/4)&(width==2?0xffffu:0xffffffffu);add(reads,at);add(reads,width);add(reads,n);++read_count;return n;};
    b.write=[&](U at,U width,U n){if(width!=4)throw std::runtime_error("Wrong store width");words.at((at-0x4f2ea4)/4)=n;add(writes,at);add(writes,width);add(writes,n);++write_count;};
    dfre::mission_sound_definitions_initialize(b,seed);for(auto n:words)add(image,n);
    std::cout<<"{\"image\":"<<image<<",\"reads\":"<<reads<<",\"writes\":"<<writes<<",\"read_count\":"<<read_count<<",\"write_count\":"<<write_count<<",\"calls\":"<<calls<<"}\n";
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
