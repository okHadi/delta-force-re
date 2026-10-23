#include "dfre/mission_object_cactus.hpp"
#include <cassert>
#include <cstring>
using namespace dfre;
namespace {std::uint32_t word(const MissionTickState& state,std::size_t at){const auto image=snapshot_mission_actor(state,{0,0});std::uint32_t n;std::memcpy(&n,image.data()+at,4);return n;}}
int main(){
    AiActorTables tables;tables.groups[0].resize(1);tables.groups[0][0].active=true;tables.groups[0][0].definition_index=1;
    MissionTickState state;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},123);
    state.globals.mission_67f528=0;mission_object_cactus(state,{0,0},{},0);assert(word(state,0xfc)==0);
    bool failed=false;try{mission_object_cactus(state,{0,0},{},4);}catch(const MissionTickError&){failed=true;}assert(failed);
    state.globals.mission_67f528=1;MissionObjectLiveBindings b;unsigned calls=0;std::uint32_t global=1;
    b.call=[&](auto& current,const auto&,const auto& action)->MissionObjectCallResult{if(calls++==0){assert(action.site==0x44df90);global=2;current.tables.groups[0][0].field_6c=0x80;}else{assert(action.site==0x447980&&action.args[1]==2);current.tables.groups[0][0].field_6c=0x20;}return {};};
    b.global=[&](auto&,auto at){assert(at==0xa158c0&&calls==1);return global;};
    mission_object_cactus(state,{0,0},b);assert(calls==2&&word(state,0xfc)==0x400&&state.tables.groups[0][0].field_6c==0x2c);
    state.tables.groups[0][0].field_6c=0;calls=0;b.call=[](auto& current,const auto&,const auto&)->MissionObjectCallResult{current.tables.groups[0][0].field_6c=0x80;throw MissionTickError("Cactus effect failure");};
    failed=false;try{mission_object_cactus(state,{0,0},b);}catch(const MissionTickError&){failed=true;}assert(failed&&state.tables.groups[0][0].field_6c==0x80);
    bind_mission_object_cactus(state,b);assert(state.update_callbacks.count(0x4534a0));failed=false;try{bind_mission_object_cactus(state,b);}catch(const MissionTickError&){failed=true;}assert(failed);
}
