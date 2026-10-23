#include "dfre/mission_object_barrel.hpp"
#include "dfre/mission_event_actor_flags.hpp"
#include <cassert>
#include <cstring>
using namespace dfre;using U=std::uint32_t;
int main(){
 MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);tables.groups[0][0].active=true;tables.groups[0][0].definition_index=1;
 init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},0);state.globals.mission_67f528=1;
 const AiActorRef ref{0,0};const U A=mission_actor_source_address(0,0),F=0x1d000000;std::array<std::uint8_t,24> frame{};MissionObjectBarrelBindings b;
 b.read=[&](U at,U width){if(at==0x67f528){assert(width==4);return state.globals.mission_67f528;}const auto image=snapshot_mission_actor(state,ref);U n=0;for(U i=0;i<width;++i)n|=U(at>=F&&at<F+24?frame.at(at-F+i):image.at(at-A+i))<<(i*8);return n;};
 b.write=[&](U at,U width,U n){if(at>=F&&at<F+24){for(U i=0;i<width;++i)frame.at(at-F+i)=std::uint8_t(n>>(i*8));return;}auto image=snapshot_mission_actor(state,ref);for(U i=0;i<width;++i)image.at(at-A+i)=std::uint8_t(n>>(i*8));apply_mission_actor_image(state,ref,image);};
 b.write(A+0x74,2,0);b.write(A+0x6c,1,0x80);mission_object_barrel(b,A,F,0xffffffff);assert(b.read(A+0xfc,4)==10&&b.read(A+0x6c,1)==0x84);
 auto missing=b;bool failed=false;try{mission_object_barrel(missing,A,F);}catch(const MissionTickError&){failed=true;}assert(failed&&b.read(F,4)==b.read(A+0x24,4));
 MissionAnimations animations(std::vector<MissionAnimationBinding>{});std::array<MissionAudioRegistration,1024> audio{};MissionActorLifecycle lifecycle(state,animations,audio,{});
 GameMessageStage messages;MissionServerMessageBuffer buffer;std::vector<std::array<std::uint8_t,mission_client_record_size>> clients(17);clients[0][0x18c]=1;clients[0][0x1c4]=5;MissionClientOutboxBindings outbox_b;outbox_b.client=[&](std::size_t i){return MissionClientRecordView{clients.at(i).data(),clients.at(i).size()};};outbox_b.cell=[](U){return 0u;};MissionClientOutbox outbox(messages,buffer,outbox_b);auto removal=mission_event_actor_flag_bindings(lifecycle,outbox);
 for(U i=0;i<6;++i)b.write(A+0x24+i*4,4,100+i);std::vector<U> calls;
 b.call=[&](U site,const std::vector<U>& args){calls.push_back(site);if(site==0x446fb0){assert((args==std::vector<U>{0,F,10}));for(U i=0;i<6;++i)assert(b.read(F+i*4,4)==100+i);b.write(A+0x24,4,11);b.write(A+0x28,4,22);b.write(A+0x2c,4,33);}else if(site==0x447610){assert((args==std::vector<U>{0,11,22,33}));}else if(site==0x4471d0){assert((args==std::vector<U>{A}));mission_event_actor_remove_one(state,ref,removal);}else throw MissionTickError("Unowned barrel test call");return 0u;};
 bind_mission_object_barrel(state,b,F);AiActorAction action{};action.target=0x45bf80;state.update_callbacks.at(0x45bf80)(state,ref,action);assert((calls==std::vector<U>{0x446fb0,0x447610,0x4471d0}));assert(snapshot_mission_actor(state,ref)==AiActorImage{});assert(clients[0][0x204+8]==0x10);
 failed=false;try{bind_mission_object_barrel(state,b,F);}catch(const MissionTickError&){failed=true;}assert(failed);failed=false;try{state.update_callbacks.at(0x45bf80)(state,{0,1},action);}catch(const MissionTickError&){failed=true;}assert(failed);
 state.globals.mission_67f528=0;auto gated=b;gated.write={};gated.call={};mission_object_barrel(gated,A,F,123);
}
