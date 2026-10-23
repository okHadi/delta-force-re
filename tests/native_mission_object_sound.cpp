#include "dfre/mission_object_sound.hpp"
#include <cassert>
#include <cstring>
using namespace dfre;
namespace {std::uint32_t word(const MissionTickState& state,std::size_t at){const auto image=snapshot_mission_actor(state,{0,0});std::uint32_t n;std::memcpy(&n,image.data()+at,4);return n;}}
int main(){
    MissionTickState state;mission_object_sound(state,{0,99},0x45c3a0,{},1);
    AiActorTables tables;tables.groups[0].resize(1);auto& actor=tables.groups[0][0];actor.active=true;actor.definition_index=1;actor.position[0]=1;actor.position[1]=2;actor.position[2]=3;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},99);
    MissionObjectLiveBindings b;unsigned calls=0;b.definition=[](auto& current,auto index,auto at){assert(index==1&&at==0x138);current.tables.groups[0][0].position[0]=9;return 55u;};
    b.call=[&](auto& current,const auto&,const auto& action)->MissionObjectCallResult{++calls;assert(action.site==0x43e7d0&&action.args[0]==55&&action.args[1]==1&&action.args[2]==2&&action.args[3]==3);current.behavior.rng=0;current.tables.groups[0][0].position[1]=8;return {};};
    mission_object_sound(state,{0,0},0x45c3a0,b);assert(calls==1&&state.behavior.rng==1&&word(state,0xfc)==512&&state.tables.groups[0][0].position[0]==9&&state.tables.groups[0][0].position[1]==8);
    b.call=[](auto& current,const auto&,const auto&)->MissionObjectCallResult{current.behavior.rng=0xcafebabe;throw MissionTickError("Sound failure");};bool failed=false;try{mission_object_sound(state,{0,0},0x45c400,b);}catch(const MissionTickError&){failed=true;}assert(failed&&state.behavior.rng==0xcafebabe&&word(state,0xfc)==512);
    bind_mission_object_sound(state,0x45c3a0,b);assert(state.update_callbacks.count(0x45c3a0));failed=false;try{bind_mission_object_sound(state,0x45c3a0,b);}catch(const MissionTickError&){failed=true;}assert(failed);
    failed=false;try{bind_mission_object_sound(state,0x45c460,b);}catch(const MissionTickError&){failed=true;}assert(failed&&!state.update_callbacks.count(0x45c460));
}
