#include "dfre/mission_object_sound.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {std::string hex(const AiActorImage& image){const char* d="0123456789abcdef";std::string s;for(auto n:image){s+=d[n>>4];s+=d[n&15];}return s;}}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);std::uint32_t v[9];for(auto& n:v){std::uint64_t x;if(!(input>>x))throw MissionTickError("Short object-sound fixture");n=std::uint32_t(x);}
    AiActorTables tables;tables.groups[0].resize(1);auto& actor=tables.groups[0][0];actor.active=true;actor.definition_index=1;for(unsigned axis=0;axis<3;++axis)actor.position[axis]=v[3+axis];
    MissionTickState state;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},v[6]);auto image=snapshot_mission_actor(state,{0,0});std::memcpy(image.data()+0xfc,&v[8],4);apply_mission_actor_image(state,{0,0},image);
    std::ostringstream trace;MissionObjectLiveBindings b;b.definition=[&](auto& current,auto index,auto at){trace<<'D'<<index<<','<<at<<';';if(v[7]&1)current.tables.groups[0][0].position[0]^=0xffffffff;if(v[7]&2)current.behavior.rng=0x12345678;if(v[7]&4)throw MissionTickError("Object-sound definition failure");return v[2];};
    b.call=[&](auto& current,const auto&,const auto& action)->MissionObjectCallResult{trace<<'C'<<action.site;for(auto n:action.args)trace<<','<<n;trace<<';';if(v[7]&8)current.behavior.rng=0xcafebabe;if(v[7]&16)current.tables.groups[0][0].position[1]^=0xffffffff;if(v[7]&32)throw MissionTickError("Object-sound call failure");return {};};
    bool failed=false;try{mission_object_sound(state,{0,0},v[0],b,v[1]);}catch(const std::exception&){failed=true;}
    std::cout<<failed<<' '<<state.behavior.rng<<' '<<hex(snapshot_mission_actor(state,{0,0}))<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
