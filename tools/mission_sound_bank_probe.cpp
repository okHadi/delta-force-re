#include "dfre/mission_sound_bank.hpp"
#include <array>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using U=std::uint32_t;
constexpr U graph=0x3100000,param=0x2007fd0,contexts=0x6e0518,scratch=0xd099c0;
struct Region{U base;std::vector<std::uint8_t> bytes;};
struct Memory{
    std::vector<Region> regions;
    U read(U at,U width)const{for(const auto& r:regions)if(at>=r.base&&std::uint64_t(at-r.base)+width<=r.bytes.size()){U n=0;for(U i=0;i<width;++i)n|=U(r.bytes[at-r.base+i])<<(i*8);return n;}throw std::runtime_error("Probe address is not mapped");}
    void write(U at,U width,U value){for(auto& r:regions)if(at>=r.base&&std::uint64_t(at-r.base)+width<=r.bytes.size()){for(U i=0;i<width;++i)r.bytes[at-r.base+i]=std::uint8_t(value>>(i*8));return;}throw std::runtime_error("Probe write is not mapped");}
};
template<class A> std::string hex(const A& bytes){std::ostringstream s;for(auto n:bytes)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(n);return s.str();}
int run(){std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);
    auto next=[&]{std::uint64_t n;if(!(input>>n))throw std::runtime_error("Short sound bank row");return U(n);};
    const auto mode=next(),id=next(),bank=next(),params=next(),context=next(),mutation=next();Memory memory;
    const std::array<std::pair<U,U>,6> layout{{{0x4f2ea8+id*0x28,40},{0x5014fc,0x408},{contexts,0x190},{scratch,0xa0},{param,32},{graph,0x2000}}};
    for(const auto& entry:layout){std::string h;if(!(input>>h)||h.size()!=entry.second*2)throw std::runtime_error("Bad sound bank image");Region region{entry.first,std::vector<std::uint8_t>(entry.second)};for(U i=0;i<entry.second;++i)region.bytes[i]=std::uint8_t(std::stoul(h.substr(i*2,2),nullptr,16));memory.regions.push_back(std::move(region));}
    auto read=[&](U at,U width=4){return memory.read(at,width);};auto put=[&](U at,U n,U width=4){memory.write(at,width,n);};
    dfre::MissionSoundBankBindings bindings;bindings.read=[&](U at,U width){return read(at,width);};bindings.write=[&](U at,U width,U n){put(at,n,width);};std::ostringstream trace;U calls=0,current_asset=0,last_voice=0;
    bindings.call=[&](U site,const std::vector<U>& args){++calls;trace<<site;for(auto a:args)trace<<','<<a;trace<<';';
        if(site==0x49d050){current_asset=args.at(0);put(current_asset+0x23,1,1);put(current_asset+0x24,0x70000000u+current_asset-graph);
            if(mutation&1){put(params,0x18000);put(params+4,7);put(params+8,255);put(graph+0x108,read(graph+0x108)^1);put(graph+0x1c,read(graph+0x1c)+0x100);put(context+0xc0,read(context+0xc0)^0x80);put(0x501500,read(0x501500)^0x20);}}
        if(site==0x499020&&(mutation&2)){put(current_asset+0x24,read(current_asset+0x24)+0x4000);put(params+0xc,5);put(graph+0x24,std::min(read(graph+0x24),2u));put(context+0xc4,123);}
        if(site==0x498230&&(mutation&4))put(params+0x14,1);
        if(site==0x494dd0||site==0x494e80){const auto sample=args.at(site==0x494dd0?0:1);U slot=last_voice;
            while(slot<8&&read(scratch+slot*0x14+4)!=sample)++slot;last_voice=slot+1;
            if(mutation&8){put(params+0xc,read(params+0xc)==0xffffffffu?3u:0xffffffffu);put(params+8,7);if(slot+1<8){const auto at=scratch+(slot+1)*0x14;put(at+4,0x7654);put(at+8,0x10000);put(at+0xc,128);put(at+0x10,13);}}
            if(mutation&16){put(context+0xc0,0xf000);put(0x4f2ea8+id*0x28,0);}}
        return 0x90000000u+calls;
    };
    U result=0;bool fault=false;try{if(mode==0)dfre::mission_sound_global_request(bindings,id,params);else if(mode==1)result=dfre::mission_sound_bank_play(bindings,bank,params,context);else if(mode==2)result=dfre::mission_sound_bank_random(bindings,bank);else if(mode==3)result=dfre::mission_sound_bank_filter(bindings,bank);else result=dfre::mission_sound_bank_quality(bindings,bank);}catch(const dfre::MissionSoundBankError& e){if(std::string(e.what()).find("attenuation")==std::string::npos)throw;fault=true;}
    std::cout<<result<<' '<<fault;for(const auto& region:memory.regions)std::cout<<' '<<hex(region.bytes);std::cout<<' '<<trace.str()<<'\n';
}return 0;}
}
int main(){try{return run();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
