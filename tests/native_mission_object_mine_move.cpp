#include "dfre/mission_object_mine_move.hpp"
#include "dfre/world_runtime.hpp"
#include "dfre/mission_live.hpp"
#include <cassert>
#include <cstring>
using namespace dfre;using U=std::uint32_t;
int main(){
    MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);
    tables.groups[0][0].active=true;tables.groups[0][0].definition_index=1;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},123);
    const AiActorRef ref{0,0};const U base=mission_actor_source_address(0,0);
    MissionObjectMineMoveBindings b;
    b.read=[&](U at,U width){assert(width==4);const auto image=snapshot_mission_actor(state,ref);U n;std::memcpy(&n,image.data()+at-base,4);return n;};
    b.write=[&](U at,U width,U n){assert(width==4);auto image=snapshot_mission_actor(state,ref);std::memcpy(image.data()+at-base,&n,4);apply_mission_actor_image(state,ref,image);};
    b.write(base+0x24,4,0xffffffff);b.write(base+0x3c,4,2);
    b.write(base+0x2c,4,5);b.write(base+0x7c,4,0xffffff80);
    b.write(base+0xe0,4,0xe8000000);b.write(base+0xe4,4,0);
    bind_mission_object_mine_move(state,b);
    AiActorAction action{};action.kind=AiActorActionKind::secondary;action.target=0x451dd0;
    state.secondary_callbacks.at(0x451dd0)(state,ref,action);
    assert(b.read(base+0x24,4)==1&&b.read(base+0x2c,4)==4);
    assert(b.read(base+0xe0,4)==0xe5000000&&b.read(base+0xe4,4)==1);
    bool failed=false;try{bind_mission_object_mine_move(state,b);}catch(const MissionTickError&){failed=true;}assert(failed);
    failed=false;try{state.secondary_callbacks.at(0x451dd0)(state,{0,1},action);}catch(const MissionTickError&){failed=true;}assert(failed);
    failed=false;try{mission_object_mine_move({},base);}catch(const MissionTickError&){failed=true;}assert(failed);
    b.read={};failed=false;try{mission_object_mine_move(b,base);}catch(const MissionTickError&){failed=true;}assert(failed);
}
