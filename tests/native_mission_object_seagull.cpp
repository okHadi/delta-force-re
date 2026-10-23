#include "dfre/mission_object_seagull.hpp"
#include "dfre/mission_primary_actor_live.hpp"
#include "dfre/world_hits.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Fixture {
 static constexpr U A=0x7dde80,D=0x1300000,F=0x1302000;
 MissionTickState state;std::unique_ptr<MissionPrimaryActorLive> primary;
 TerrainGeometry terrain{AssetBytes(1024*1024),AssetBytes(1024*1024),std::vector<U>(65536)};
 std::array<std::uint8_t,28> frame{};unsigned sounds=0;U sound_id=0;bool sound_mutates=false,stop_trig=false;
 Fixture(){
  AiActorTables tables;tables.groups[0].resize(2);for(auto& actor:tables.groups[0]){actor.active=true;actor.definition_index=1;}
  std::vector<AiItemDefinition> definitions(2);definitions[1].id=0xfd3;definitions[1].field_28=6;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),{},1);state.world.terrain=&terrain;state.globals.mission_67f528=1;
  MissionPrimaryActorLiveBindings b;b.initial_definition=[=](std::size_t){return D+0x13cu;};b.definition_index=[=](U raw){if(raw!=D+0x13c)throw MissionTickError("Definition source is not owned");return 1u;};b.definition_address=[=](U index){if(index!=1)throw MissionTickError("Definition index is not owned");return D+0x13cu;};primary=std::make_unique<MissionPrimaryActorLive>(state,b,std::vector<AiActorImage>(1022));
  for(U i=0;i<2;++i){primary->write(A+i*0x114+0x72,2,7);primary->write(A+i*0x114+0x74,2,1);primary->write(A+i*0x114+0x7c,4,0x30000);primary->write(A+i*0x114+0xec,4,0x400000);}
 }
 MissionObjectSeagullBindings bindings(){MissionObjectSeagullBindings b;
  b.read=[&](U at,U width){if(primary->claims(at,width))return primary->read(at,width);if(at>=F&&std::uint64_t(at-F)+width<=frame.size()){U value=0;for(U i=0;i<width;++i)value|=U(frame.at(at-F+i))<<(i*8);return value;}if(at==0x67f528&&width==4)return state.globals.mission_67f528;if(at==D+0x13c+0x20&&width==4)return state.definitions.entries().at(1).id;throw MissionTickError("Native seagull read is not owned");};
  b.write=[&](U at,U width,U value){if(primary->claims(at,width)){primary->write(at,width,value);return;}if(at>=F&&std::uint64_t(at-F)+width<=frame.size()){for(U i=0;i<width;++i)frame.at(at-F+i)=std::uint8_t(value>>(i*8));return;}throw MissionTickError("Native seagull store is not owned");};
  b.call=[this](U site,const std::vector<U>& args)->U{
   if(site==0x477860)return behavior_rng_next(state.behavior);
   if(site==0x477670)return U(collision_approximate_length(std::int32_t(args.at(0)),std::int32_t(args.at(1)),std::int32_t(args.at(2))));
   if(site==0x477770)return query_angle_477770(std::int32_t(args.at(0)),std::int32_t(args.at(1)));
   if(site==0x477820){if(stop_trig){primary->write(A+0x7c,4,0x12345);throw MissionTickError("Trig provider stopped");}const U index=args.at(0)>>21;auto memory=bindings();memory.write(args.at(1),4,U(kSineTableQ22.at(index)));memory.write(args.at(2),4,U(kSineTableQ22.at(index+512)));return 0;}
   if(site==0x47df40)return U(ground_height_query(*state.world.terrain,std::int32_t(args.at(0)),std::int32_t(args.at(1))));
   if(site==0x43e7d0){++sounds;sound_id=args.at(0);if(sound_mutates){primary->write(A+0x7c,4,0x20000);primary->write(A+0x88,4,0x20000);}return 0;}
   throw MissionTickError("Native seagull service is not owned");
  };return b;
 }
};
}
int main(){try{
 {Fixture f;auto b=f.bindings();check(mission_seagull_flock_first(b,Fixture::A+0x114)==Fixture::A&&mission_seagull_flock_index(b,Fixture::A+0x114)==1,"physical flock scan differs");f.primary->write(Fixture::A+0x6c,1,4);check(mission_seagull_flock_first(b,Fixture::A+0x114)==Fixture::A+0x114&&mission_seagull_flock_index(b,Fixture::A+0x114)==0,"current dead flag was not read");}
 {Fixture f;auto b=f.bindings();U seed=1;BehaviorState expected;for(;;++seed){expected.rng=seed;if(!(behavior_rng_next(expected)&15))break;}f.state.behavior.rng=seed;f.sound_mutates=true;bind_mission_object_seagull(f.state,b,Fixture::F);AiActorAction ai{AiActorActionKind::update,{0,0},0x451fd0};f.state.update_callbacks.at(0x451fd0)(f.state,{0,0},ai);check(f.sounds==1&&f.sound_id==0x43&&f.state.behavior.rng==expected.rng,"leader did not use shared RNG");check(f.primary->read(Fixture::A+0x3c,4)==0x7ff&&f.primary->read(Fixture::A+0x40,4)==0&&f.primary->read(Fixture::A+0xfc,4)==16,"AI ignored current sound mutation or trig output");const auto x=f.state.tables.groups[0][0].position[0];AiActorAction move{AiActorActionKind::secondary,{0,0},0x452310};f.state.secondary_callbacks.at(0x452310)(f.state,{0,0},move);check(f.state.tables.groups[0][0].position[0]==x+0x7ff&&f.primary->read(Fixture::A+0xf4,4)==0,"move did not use canonical actor owner");}
 {Fixture f;auto b=f.bindings();f.primary->write(Fixture::A+0x6c,1,4);f.primary->write(Fixture::A+0x2c,4,0);f.primary->write(Fixture::A+0xf0,4,0xffffe000);mission_object_seagull_move(b,Fixture::A);check(f.sounds==1&&f.sound_id==0x48&&f.primary->read(Fixture::A+0x2c,4)==0x1000&&f.primary->read(Fixture::A+0xf0,4)==0x400,"ground bounce differs");}
 {Fixture f;auto b=f.bindings();f.stop_trig=true;const U old_fc=f.primary->read(Fixture::A+0xfc,4);bool failed=false;try{mission_object_seagull_ai(b,Fixture::A,Fixture::F);}catch(const MissionTickError&){failed=true;}check(failed&&f.primary->read(Fixture::A+0x7c,4)==0x12345&&f.primary->read(Fixture::A+0xfc,4)==old_fc,"AI error restored or completed later fields");}
 {Fixture f;auto b=f.bindings();f.primary->write(Fixture::A,4,0);f.primary->write(Fixture::A+0x114,4,0);check(mission_seagull_flock_index(b,Fixture::A)==0xffffffff&&mission_seagull_flock_first(b,Fixture::A)==0,"absent flock result differs");bool failed=false;try{mission_object_seagull_ai(b,Fixture::A,Fixture::F);}catch(const MissionTickError&){failed=true;}check(failed&&f.primary->read(Fixture::A+0xf4,4)==0xffffffff&&!f.primary->read(Fixture::A+0xe8,4),"missing leader failure prefix differs");}
 std::cout<<"seagull callback checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
