#include "dfre/mission_object_mine_update.hpp"
#include "dfre/mission_actor_lifecycle.hpp"
#include "dfre/world_runtime.hpp"
#include "dfre/projectiles.hpp"
#include <cassert>
#include <cstring>
using namespace dfre;using U=std::uint32_t;
int main(){
 MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);auto& actor=tables.groups[0][0];actor.active=true;actor.definition_index=1;actor.field_7c=0x2ffff;
 init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},123);
 MissionAnimations animations(std::vector<MissionAnimationBinding>{});std::array<MissionAudioRegistration,1024> registrations{};MissionActorLifecycle lifecycle(state,animations,registrations,{});
 constexpr U frame=0x1300000;const AiActorRef ref{0,0};const U base=mission_actor_source_address(0,0);std::array<std::uint8_t,8> locals{};
 MissionObjectMineUpdateBindings b;unsigned removals=0;
 b.read=[&](U at,U width){U n=0;if(at>=frame&&std::uint64_t(at-frame)+width<=locals.size()){for(U i=0;i<width;++i)n|=U(locals.at(at-frame+i))<<(8*i);return n;}const auto image=snapshot_mission_actor(state,ref);for(U i=0;i<width;++i)n|=U(image.at(at-base+i))<<(8*i);return n;};
 b.write=[&](U at,U width,U n){if(at>=frame&&std::uint64_t(at-frame)+width<=locals.size()){for(U i=0;i<width;++i)locals.at(at-frame+i)=std::uint8_t(n>>(8*i));return;}auto image=snapshot_mission_actor(state,ref);for(U i=0;i<width;++i)image.at(at-base+i)=std::uint8_t(n>>(8*i));apply_mission_actor_image(state,ref,image);};
 b.call=[&](U site,const std::vector<U>& args)->U{if(site==0x477820){const auto index=args.at(0)>>21;b.write(args.at(1),4,U(kSineTableQ22.at(index)));b.write(args.at(2),4,U(kSineTableQ22.at(index+512)));return 0;}assert(site==0x45da30&&args.at(0)==base);++removals;lifecycle.remove(ref);return 0;};
 bind_mission_object_mine_update(state,b,frame);AiActorAction action{};action.kind=AiActorActionKind::update;action.target=0x451d40;
 state.update_callbacks.at(0x451d40)(state,ref,action);
 assert(b.read(base+0x7c,4)==0x33fff&&b.read(base+0x3c,4)==0xcff&&b.read(base+0x40,4)==0);
 assert(b.read(base+0xfc,4)==16&&(b.read(base+0x6d,1)&1)&&removals==0);
 b.write(base+0x2c,4,0x640001);registrations[0].actor=base;registrations[0].definition=1;
 state.update_callbacks.at(0x451d40)(state,ref,action);
 assert(removals==1&&!state.tables.groups[0][0].active&&registrations[0].actor==0);
 assert(b.read(base+0xfc,4)==16&&b.read(base+0x6d,1)==1&&b.read(base+4,4)==0);
 bool failed=false;try{bind_mission_object_mine_update(state,b,frame);}catch(const MissionTickError&){failed=true;}assert(failed);
 failed=false;try{state.update_callbacks.at(0x451d40)(state,{0,1},action);}catch(const MissionTickError&){failed=true;}assert(failed);
 b.call={};b.write(base+0x7c,4,0);failed=false;try{mission_object_mine_update(b,base,frame);}catch(const MissionTickError&){failed=true;}assert(failed&&b.read(base+0x7c,4)==0x4000);
}
