#include "dfre/mission_sound_registration.hpp"
#include <array>
#include <iostream>
#include <map>
namespace {using U=std::uint32_t;using H=std::uint64_t;
void add(H& h,U n){for(unsigned i=0;i<4;++i){h^=std::uint8_t(n>>(i*8));h*=1099511628211ull;}}
}
int main(){try{U seed;while(std::cin>>seed){
    constexpr U table=0x4f2ea8,first=0x3200000,second=first+0x100,base=0x6e0700;
    const U id=seed%512,actor=seed%17?0x3100000:0,definition=table+id*40;
    std::map<U,std::uint8_t> memory;
    auto put=[&](U at,U width,U n){for(U i=0;i<width;++i)memory[at+i]=std::uint8_t(n>>(i*8));};
    auto get=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(memory[at+i])<<(i*8);return n;};
    const U occupied=std::array<U,4>{0,1,1023,1024}[seed%4];
    for(U i=0;i<1024;++i){put(base+i*16,4,0xabc00000+i);put(base+i*16+4,4,i<occupied?7:0);put(base+i*16+8,4,seed+i);put(base+i*16+12,4,0xfedcba98);}
    put(definition,4,seed%7?first:0);put(definition+0x20,1,seed%6?2:0);put(first+0x18,4,seed%4?9:0);put(first+0x48,4,seed*97);put(second+0x48,4,seed*177);put(0x4f7ea4,4,0xffffffffu-seed);
    H reads=14695981039346656037ull,writes=reads,image=reads;U read_count=0,write_count=0,calls=0;bool fault=false;
    dfre::MissionSoundBankBindings b;
    b.read=[&](U at,U width){const auto n=get(at,width);add(reads,at);add(reads,width);add(reads,n);++read_count;return n;};
    b.write=[&](U at,U width,U n){if(at<0x400000)throw std::runtime_error("Unmapped actor");add(writes,at);add(writes,width);add(writes,n);++write_count;put(at,width,n);};
    b.call=[&](U at,const std::vector<U>& args){if(at!=0x43ee80||args!=std::vector<U>{id})throw std::runtime_error("Wrong ambient call");++calls;put(definition,4,second);return seed%5?1u:0u;};
    try{dfre::mission_sound_attach(b,actor,id,seed*11,seed*13);}catch(const std::runtime_error&){fault=true;}
    const auto loudness=dfre::mission_sound_loudness(b,id);
    for(U at=base;at<base+16384;at+=4)add(image,get(at,4));add(image,get(0x4f7ea4,4));add(image,get(actor+0x1c,4));
    std::cout<<"{\"image\":"<<image<<",\"reads\":"<<reads<<",\"writes\":"<<writes<<",\"read_count\":"<<read_count<<",\"write_count\":"<<write_count<<",\"calls\":"<<calls<<",\"fault\":"<<fault<<",\"loudness\":"<<loudness<<"}\n";
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
