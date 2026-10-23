#include "dfre/mission_object_wood_large.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {std::string hex(const AiActorImage& image){const char* d="0123456789abcdef";std::string s;for(auto n:image){s+=d[n>>4];s+=d[n&15];}return s;}}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);std::uint32_t v[7];for(auto& n:v){std::uint64_t x;if(!(input>>x))throw MissionTickError("Short large wood fixture");n=std::uint32_t(x);}
    AiActorTables tables;tables.groups[0].resize(1);auto& actor=tables.groups[0][0];actor.active=true;actor.definition_index=1;actor.field_6c=std::uint8_t(v[2]);
    MissionTickState state;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},123);state.globals.mission_67f528=v[0];
    auto image=snapshot_mission_actor(state,{0,0});image[0x74]=std::uint8_t(v[3]);image[0x75]=std::uint8_t(v[3]>>8);std::memcpy(image.data()+0xfc,&v[6],4);apply_mission_actor_image(state,{0,0},image);
    std::uint32_t global=v[4];unsigned calls=0;std::ostringstream trace;MissionObjectLiveBindings b;
    b.call=[&](auto& current,const auto&,const auto& action)->MissionObjectCallResult{
        trace<<'C'<<action.site;for(auto n:action.args)trace<<','<<n;trace<<';';const auto call=calls++;auto& a=current.tables.groups[0][0];
        if(call==0){if(v[5]&1)a.field_6c^=0x80;if(v[5]&4)global=0x55555555;if(v[5]&128)current.behavior.rng=0xcafebabe;}
        if(call==1&&(v[5]&2))a.field_6c^=0x40;
        if((call==0&&(v[5]&8))||(call==1&&(v[5]&32)))throw MissionTickError("Large wood foreign failure");return {};};
    b.global=[&](auto& current,auto at){trace<<'G'<<at<<';';if(v[5]&64)current.tables.groups[0][0].field_6c^=0x20;if(v[5]&16)throw MissionTickError("Large wood global failure");return global;};
    bool failed=false;try{mission_object_wood_large(state,{0,0},b,v[1]);}catch(const std::exception&){failed=true;}
    std::cout<<failed<<' '<<global<<' '<<state.behavior.rng<<' '<<hex(snapshot_mission_actor(state,{0,0}))<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
