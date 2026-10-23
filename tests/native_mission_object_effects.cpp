#include "dfre/mission_object_effects.hpp"
#include "dfre/mission_fragment_spawn.hpp"
#include "dfre/mission_explosion_runtime.hpp"
#include "dfre/mission_behavior_zones.hpp"
#include "dfre/mission_client_outbox.hpp"
#include "dfre/mission_live.hpp"
#include <cassert>
#include <map>
using namespace dfre;using U=std::uint32_t;
int main(){
 MissionTickState state;AiActorTables tables;tables.groups[3].resize(1);tables.groups[3][0].active=true;tables.groups[3][0].definition_index=1;for(U i=0;i<3;++i)tables.groups[3][0].position[i]=i+1;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},0);state.globals.mission_67f528=1;state.projectiles.resize(512);
 constexpr U actor=0x7cca80,P=actor+0x24,Q=0x1d000000,E=0x1d001000,B=0x1d002000,D=0x18000000;std::map<U,std::uint8_t> bytes;for(U at:{Q,E,B,D})for(U i=0;i<0x200;++i)bytes[at+i]=0;
 auto put=[&](U at,U n,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(i*8));};auto get=[&](U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(i*8);return n;};
 put(Q+0x1c,0);put(Q+0x20,P);put(Q+0x24,4);put(E+0x14,0xdeadbeef);put(D+0x28,6);put(D+0x6c,0x111);put(D+0x84,0x222);put(D+0x90,0x333);
 MissionObjectEffectBindings b;b.read=[&](U at,U width){if(at==0x67f528)return state.globals.mission_67f528;if(at==0xc23810)return D-0x13c;if(at>=actor&&at+width<=actor+0x114){const auto image=snapshot_mission_actor(state,{3,0});U n=0;for(U i=0;i<width;++i)n|=U(image.at(at-actor+i))<<(i*8);return n;}return get(at,width);};b.write=[&](U at,U width,U n){put(at,n,width);};
 GameMessageStage messages;MissionServerMessageBuffer server;std::vector<std::array<std::uint8_t,mission_client_record_size>> clients(17);clients[0][0x18c]=1;clients[0][0x1c4]=5;MissionClientOutboxBindings ob;ob.client=[&](std::size_t slot){return MissionClientRecordView{clients.at(slot).data(),clients.at(slot).size()};};ob.cell=[](U){return 0u;};MissionClientOutbox outbox(messages,server,ob);
 MissionProjectileSpawnCells cells;MissionFragmentSpawn fragments(state,cells,{[](U angle,std::int32_t& sine,std::int32_t& cosine){sincos_q22(angle,sine,cosine);}});MissionBehaviorZones zones;auto zb=zones.bindings(state.globals.mission_67f528);bool wet_bound=false;unsigned wet_calls=0,sounds=0,spawned=0;
 b.call=[&](U site,const std::vector<U>& args)->U{
  if(site==0x442630){std::vector<std::uint8_t> packet(args.at(2));for(U i=0;i<args.at(2);++i)packet[i]=std::uint8_t(b.read(args.at(1)+i,1));outbox.broadcast(std::int32_t(args.at(0)),packet.data(),U(packet.size()));return 0;}
  if(site==0x4606b0){put(E+0x2c,args.at(0));put(E+0x30,args.at(1));put(E+0x34,args.at(2));mission_object_effect_apply(b,E);return 0;}
  if(site==0x477850){state.behavior.rng=args.at(0);return 0;}if(site==0x477860)return behavior_rng_next(state.behavior);
  if(site==0x45dc70){if(!wet_bound)throw MissionTickError("Required wet effect fixture is absent");assert(args.at(0)==3);++wet_calls;state.tables.groups[3][0].position[0]=9;return 0;}
  if(site==0x4657d0){Fixed3 position{std::int32_t(args.at(1)),std::int32_t(args.at(2)),std::int32_t(args.at(3))};return mission_explosion_enqueue_source(state,args.at(0),position,args.at(4));}
  if(site==0x43e7d0){assert((args==std::vector<U>{0x32,9,2,3}));++sounds;return 0;}
  if(site==0x45fcf0)return mission_behavior_zone_add(zb,args.at(0),args.at(1),args.at(2),args.at(3),args.at(4),args.at(5));
  if(site==0x463200){std::array<U,6> pose{};for(U i=0;i<6;++i)pose[i]=b.read(args.at(0)+i*4,4);assert(pose[5]==0xdeadbeef);++spawned;return fragments.spawn(pose,args.at(1),args.at(2));}
  if(site==0x49a620){for(U i=0;i<args.at(2);++i)b.write(args.at(0)+i,1,args.at(1)&255);return args.at(0);}
  if(site==0x4828c0){assert(args.at(0)==0xfce);return 1;}
  if(site==0x45d970){assert(args.at(0)==B&&args.at(1)==0);assert(get(B)==1&&get(B+4)==D&&get(B+0x10)==0x111&&get(B+0x110)==0x222&&get(B+0x10c)==0x333);throw MissionTickError("Primary raw insertion adapter is required");}
  throw MissionTickError("Unowned object effect fixture call");};
 bool failed=false;try{mission_object_effect_request(b,Q);}catch(const MissionTickError&){failed=true;}assert(failed&&clients[0][0x204+8]==0x27&&cells.ring_index==0&&state.pending_spawns.empty());
 wet_bound=true;mission_object_effect_request(b,Q);assert(wet_calls==1&&sounds==1&&spawned==4&&cells.ring_index==4&&state.pending_spawns.size()==1&&zones.count==1&&state.projectiles[4].class_flags==0x20&&state.projectiles[4].owner_slot==-1);
 put(B+0x120,0);put(B+0x124,11);put(B+0x128,22);put(B+0x12c,33);failed=false;try{mission_object_effect_allocate(b,B);}catch(const MissionTickError&){failed=true;}assert(failed&&get(B+0x24)==11&&get(B+0x28)==22&&get(B+0x2c)==33&&get(B+0x6c)==0&&get(B+0xf8)==0);
 state.globals.mission_67f528=0;auto gated=b;gated.call={};gated.write={};mission_object_effect_request(gated,Q);
}
