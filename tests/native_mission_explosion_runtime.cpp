#include "dfre/mission_explosion_runtime.hpp"
#include "dfre/mission_area_live.hpp"
#include "dfre/mission_fragment_spawn.hpp"
#include "dfre/mission_peer_expiry.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("explosion runtime: " #x);}while(false)
using namespace dfre;
namespace {
void init(MissionTickState& state){AiActorTables tables;for(auto& group:tables.groups)group.resize(2);auto& a=tables.groups[3][0];a.active=true;a.definition_index=1;a.field_110=0x1234;
 std::vector<AiItemDefinition> defs(2);defs[1].field_28=3;defs[1].field_84=0x1234;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(defs),{},0);state.globals.mission_67f528=1;state.extra_for({3,0}).field_74=100;state.projectiles.resize(512);
}
}
int main(){try{
 MissionTickState state;init(state);state.globals.multiplayer_67f520=1;state.globals.mission_67f528=0;
 CHECK(!mission_explosion_enqueue_source(state,5,{1,2,3},0xdeadbeef)&&state.pending_spawns.empty());state.globals.multiplayer_67f520=0;
 CHECK(mission_explosion_enqueue_source(state,0xffffffff,{1,-2,3},0x7cca80));CHECK(state.pending_spawns[0].owner==actor_handle(3,0)&&state.pending_spawns[0].type==-1&&state.pending_spawns[0].position[1]==-2);
 state.pending_spawns.clear();for(unsigned group=0;group<4;++group){CHECK(mission_explosion_enqueue_source(state,group,{},mission_actor_source_address(group,0)));CHECK(state.pending_spawns.back().owner==actor_handle(group,0));}
 CHECK(mission_explosion_enqueue_source(state,0,{},0)&&state.pending_spawns.back().owner==0);const auto size=state.pending_spawns.size();bool failed=false;try{mission_explosion_enqueue_source(state,0,{},0xdeadbeef);}catch(const MissionTickError&){failed=true;}CHECK(failed&&state.pending_spawns.size()==size);
 while(state.pending_spawns.size()<30)CHECK(mission_explosion_enqueue(state,1,{},0));CHECK(!mission_explosion_enqueue_source(state,5,{},0xdeadbeef)&&state.pending_spawns.size()==30);state.pending_spawns.clear();state.globals.mission_67f528=1;
 // Actual460950 producer shares its RNG, fragment ring and area queue owners.
 MissionProjectileSpawnCells cells;cells.ring_index=511;MissionFragmentSpawnBindings fb;fb.sincos_4776c0=sincos_q22;MissionFragmentSpawn fragments(state,cells,fb);
 unsigned visual=0,sound=0,alert=0,spawned=0;MissionPeerExpiryEffectBindings producer;
 producer.call=[&](auto site,const auto& args)->std::uint32_t{
  if(site==0x477850){state.behavior.rng=args.at(0);return 0;}
  if(site==0x477860)return behavior_rng_next(state.behavior);
  if(site==0x4657d0){CHECK(args.size()==5);CHECK(mission_explosion_enqueue_source(state,args[0],{std::int32_t(args[1]),std::int32_t(args[2]),std::int32_t(args[3])},args[4]));return 0;}
  if(site==0x45dc70){++visual;return 0;}if(site==0x43e7d0){++sound;return 0;}if(site==0x45fcf0){++alert;return 0;}throw std::runtime_error("unbound producer call");
 };
 producer.spawn=[&](auto& position,auto owner,auto speed){++spawned;fragments.spawn(position,owner,speed);};std::array<std::uint32_t,7> position{};BehaviorState expected;expected.rng=0x5fc;for(unsigned i=0;i<6;++i)behavior_rng_next(expected);
 mission_peer_expiry_effect(producer,0x7cca80,{position.data(),position.size()},2,0xa5a5a5a5);
 CHECK(visual==1&&sound==1&&alert==1&&spawned==2&&cells.ring_index==1&&state.behavior.rng==expected.rng);CHECK(state.pending_spawns.size()==1&&state.pending_spawns[0].type==5&&state.pending_spawns[0].owner==actor_handle(3,0));CHECK(state.projectiles[0].class_flags==0x20&&state.projectiles[1].ttl==400);
 // Actual465BD0 and damage helpers see callback appends in the same queue.
 MissionGeometry geometry(std::vector<MissionGeometryModel>{});MissionAreaLiveBindings area;area.globals=[](const auto&){return MissionAreaLiveGlobals{};};unsigned queries=0,updates=0;
 area.segment=[&](auto&,const auto&,const auto&,const auto&){++queries;return 1;};
 area.update=[&](MissionTickState& live,const AiActorRef& ref,auto callback,auto message){CHECK(ref.group==3&&ref.index==0&&callback==0x1234);++updates;
  if(updates==1){CHECK(message==2&&live.extra_for(ref).field_74==96);live.extra_for(ref).field_74=80;CHECK(mission_explosion_enqueue(live,4,{},0));}
  else CHECK(updates==2&&message==5&&live.extra_for(ref).field_74==79);
 };
 MissionStageCall stage;stage.address=0x465bd0;CHECK(mission_area_live_stage(state,geometry,stage,area)==0);CHECK(queries==1&&updates==2&&state.pending_spawns.empty()&&state.extra_for({3,0}).field_74==79);
 std::cout<<"explosion runtime checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
