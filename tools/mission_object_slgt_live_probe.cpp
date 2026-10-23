#include "dfre/mission_object_live.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {using U=std::uint32_t;void put(AiActorImage& image,std::size_t at,U n){std::memcpy(image.data()+at,&n,4);}std::string hex(const AiActorImage& image){const char* d="0123456789abcdef";std::string s;for(auto n:image){s+=d[n>>4];s+=d[n&15];}return s;}}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);U v[15];for(auto& n:v){std::uint64_t x;if(!(input>>x))throw MissionTickError("Short searchlight fixture");n=U(x);}
    MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);auto& a=tables.groups[0][0];a.active=true;a.definition_index=1;a.person_slot=0;MissionTickConfig config;config.host=v[0]!=0;
    init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(2),0x12345678);
    auto image=snapshot_mission_actor(state,{0,0});image[0x6c]=std::uint8_t(v[1]);image[0x6d]=std::uint8_t(v[13]);
    const std::size_t offsets[]={0xe0,0xe4,0xe8,0xec,0x30,0x8c,0x34,0xf0};for(std::size_t i=0;i<8;++i)put(image,offsets[i],v[i+3]);put(image,0x90,v[14]);apply_mission_actor_image(state,{0,0},image);state.person_extra(0).field_44=v[2];
    std::ostringstream trace;unsigned reads=0;U rate=v[11];MissionObjectLiveBindings b;
    b.definition=[&](auto& current,U index,U at){trace<<'D'<<index<<','<<at<<';';if(at!=0xe0)throw MissionTickError("Unexpected searchlight definition field");
        const U result=rate;const auto read=reads++;
        auto changed=snapshot_mission_actor(current,{0,0});
        if(read==0&&(v[12]&1)){U speed;std::memcpy(&speed,changed.data()+0xec,4);put(changed,0xec,speed+0x1234);}
        if(read==1&&(v[12]&2)){U speed;std::memcpy(&speed,changed.data()+0xec,4);put(changed,0xec,speed^0x80000000);}
        if(read==0&&(v[12]&4)){put(changed,0x30,0x98760000);put(changed,0x8c,0x76540000);put(changed,0x34,0x12340000);put(changed,0xf0,0xfedc0000);}
        apply_mission_actor_image(current,{0,0},changed);
        if(read==0&&(v[12]&8))rate=0x80000001;
        if(read==0&&(v[12]&32))current.tables.groups[0][0].person_slot=1;
        if(read==0&&(v[12]&64))current.behavior.rng=0xcafebabe;
        if(v[12]&16)throw MissionTickError("Searchlight definition failure");return result;
    };
    bool failed=false;try{mission_object_slgt_move(state,{0,0},b);}catch(const std::exception&){failed=true;}
    std::cout<<failed<<' '<<state.behavior.rng<<' '<<hex(snapshot_mission_actor(state,{0,0}))<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
