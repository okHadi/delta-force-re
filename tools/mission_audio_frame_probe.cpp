#include "dfre/mission_audio_frame.hpp"
#include <iostream>
#include <sstream>
#include <vector>
using namespace dfre;
namespace {using U=std::uint32_t;U next(std::istream& s){std::uint64_t n;if(!(s>>n))throw std::runtime_error("Short audio frame row");return U(n);}std::int32_t si(U v){return v<=0x7fffffff?std::int32_t(v):std::int32_t(std::int64_t(v)-0x100000000LL);}}
int main(int argc,char** argv){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream in(line);
    if(argc>1&&std::string(argv[1])=="attenuation"){const auto distance=si(next(in)),range=si(next(in));const auto scale=std::uint8_t(next(in)),ceiling=std::uint8_t(next(in));try{std::cout<<mission_audio_attenuation(distance,range,scale,ceiling)<<'\n';}catch(const std::runtime_error&){std::cout<<"fault\n";}continue;}
    const auto counter=next(in);Fixed3 listener{si(next(in)),si(next(in)),si(next(in))};auto yaw=next(in);const auto mutation=next(in),count=next(in);
    MissionAudioFrameCells c{counter,listener,yaw};std::vector<std::array<U,4>> defs(count);std::vector<std::array<U,5>> actors(count);
    for(U i=0;i<count;++i){for(auto& v:defs[i])v=next(in);for(auto& v:actors[i])v=next(in);auto& r=c.registrations_6e0700[i];r={i+1,i+1,next(in),0x12345678};}
    for(auto& n:c.identities_6e06d0)n=next(in);for(auto& n:c.handles_6e06e8)n=next(in);
    std::ostringstream trace;U starts=0;MissionAudioFrameBindings b;
    b.definition_word=[&](U id,U at){const auto& d=defs.at(id-1);return at==0x18?d[0]:at==0x1c?d[1]:at==0x24?d[2]:d[3];};
    b.actor_word=[&](U id,U at){const auto& a=actors.at(id-1);return at==0?a[0]:at==0x20?a[1]:a.at(2+(at-0x24)/4);};
    b.update=[&](auto handle,auto pan,auto volume,auto rate){trace<<"U,"<<handle<<','<<pan<<','<<volume<<','<<rate<<';';if(mutation&2){++c.selected_6e0470[0].volume;c.selected_6e0470[1].pan=17;}if(mutation&8)c.counter_4f7ea8=92;};
    b.stop=[&](auto handle){trace<<"T,"<<handle<<';';if(mutation&1)c.handles_6e06e8[1]=0;if(mutation&16)c.handles_6e06e8[0]=888;};
    b.start=[&](auto sample,auto pan,auto volume,auto rate,auto priority){trace<<"S,"<<sample<<','<<pan<<','<<volume<<','<<rate<<','<<priority<<';';if(mutation&4)c.handles_6e06e8[5]=777;return (mutation&32)?0u:1000u+(++starts);};
    mission_audio_frame(c,b);std::cout<<c.counter_4f7ea8;
    for(U i=0;i<count;++i){const auto& r=c.registrations_6e0700[i];for(auto n:{r.definition,r.actor,r.identity,r.reserved})std::cout<<' '<<n;}
    for(const auto& a:c.candidates_6e0270)std::cout<<' '<<a.registration<<' '<<a.volume;
    for(const auto& s:c.selected_6e0470)for(auto n:{s.sample,s.identity,s.registration,s.volume,s.played,s.rate,s.pan})std::cout<<' '<<n;
    for(auto n:c.identities_6e06d0)std::cout<<' '<<n;for(auto n:c.handles_6e06e8)std::cout<<' '<<n;
    std::cout<<' '<<(trace.str().empty()?"-":trace.str())<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
