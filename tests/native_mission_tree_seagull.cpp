#include "dfre/mission_object_tree_spawn.hpp"
#include "dfre/mission_object_seagull.hpp"
#include "dfre/world_hits.hpp"
#include "dfre/mission_primary_actor_live.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/mission_positional_sound.hpp"
#include <map>
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}}
int main(){try{
 constexpr U A=0x7dde80,G=0x7cca80,D=0x18000000,W=0x1d001000,F=0x1d000000,S=0x1d000300;
 AiActorTables tables;tables.groups[0].resize(1);tables.groups[0][0].active=true;tables.groups[0][0].definition_index=1;tables.groups[0][0].field_70=1;
 // This fixture explicitly owns every physical row. No implicit tail provider is used.
 tables.groups[3].resize(256);tables.groups[3][0].active=true;tables.groups[3][0].definition_index=1;tables.groups[3][0].field_70=2;
 std::vector<AiItemDefinition> definitions(3);definitions[1].field_28=6;definitions[2].field_28=6;definitions[2].field_84=0x451fd0;definitions[2].field_90=0x452310;
 MissionTickState state;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),{},7);state.globals.mission_67f528=1;
 std::map<U,std::uint8_t> bytes;for(U i=0;i<3*0x13c;++i)bytes[D+i]=0;for(U i=0;i<0x114;++i)bytes[F+i]=0xa5;for(U i=0;i<0x100;++i)bytes[W+i]=0;for(U i=0;i<28;++i)bytes[S+i]=0xa5;
 const auto set=[&](U at,U n){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));};
 set(D+2*0x13c+0x20,0xfd3);set(D+2*0x13c+0x28,6);set(D+2*0x13c+0x84,0x451fd0);set(D+2*0x13c+0x90,0x452310);set(D+0x13c+0x78,W);set(W+0x10,0x10000);
 MissionPrimaryActorLiveBindings pb;pb.initial_definition=[=](std::size_t){return D+0x13c;};pb.definition_index=[=](U at){if(at<D||at>=D+3*0x13c||(at-D)%0x13c)throw MissionTickError("Tree definition is not current");return (at-D)/0x13c;};pb.definition_address=[=](U index){return D+index*0x13c;};pb.delegated.read=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;};pb.delegated.write=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));};
 std::vector<AiActorImage> retained_tail(1023);retained_tail.back()[0x110]=0x5a;MissionPrimaryActorLive primary(state,pb,std::move(retained_tail));auto insertion=primary.insertion_bindings();
 unsigned random_calls=0,insertions=0,sound_calls=0,tail_reads=0;std::vector<U> sound_ids;TerrainGeometry terrain(AssetBytes(1024*1024),AssetBytes(1024*1024),std::vector<U>(65536));
 MissionObjectTreeBindings b;
 b.read=[&](U at,U width){if(at==A+1023*0x114&&width==4)++tail_reads;if(at==0x67f528&&width==4)return state.globals.mission_67f528;if(primary.claims(at,width))return primary.read(at,width);if(at==0xc23810)return D;if(at==0xc23814)return 3u;if(at==0xc23820)return 0u;if(at==0x67dbc9)return 0u;
  if(at>=G&&std::uint64_t(at)+width<=G+256*0x114){const U slot=(at-G)/0x114,offset=(at-G)%0x114;check(offset+width<=0x114,"Cross-record read");auto image=snapshot_mission_actor(state,{3,slot});if(offset==4&&width==4)return state.tables.groups[3][slot].active?D+state.tables.groups[3][slot].definition_index*0x13c:0u;U n=0;for(U i=0;i<width;++i)n|=U(image.at(offset+i))<<(8*i);return n;}return pb.delegated.read(at,width);};
 b.write=[&](U at,U width,U n){if(primary.claims(at,width)){primary.write(at,width,n);return;}pb.delegated.write(at,width,n);};
 MissionSoundBankBindings sound;sound.read=[](U at,U width){check(width==4&&(at==0x4f2ea8+0x4c*0x28||at==0x4f2ea8+0x44*0x28||at==0x4f2ea8+0x43*0x28||at==0x4f2ea8+0x48*0x28),"Unexpected cold sound read");return 0u;};
 b.call=[&](U site,const std::vector<U>& args)->U{if(site==0x44fba0)return mission_object_tree_signal(b,args.at(0));if(site==0x477670)return U(collision_approximate_length(std::int32_t(args.at(0)),std::int32_t(args.at(1)),std::int32_t(args.at(2))));if(site==0x477860){++random_calls;return behavior_rng_next(state.behavior);}if(site==0x477770)return query_angle_477770(std::int32_t(args.at(0)),std::int32_t(args.at(1)));if(site==0x477820){const U index=args.at(0)>>21;b.write(args.at(1),4,U(kSineTableQ22.at(index)));b.write(args.at(2),4,U(kSineTableQ22.at(index+512)));return 0;}if(site==0x47df40)return U(ground_height_query(terrain,std::int32_t(args.at(0)),std::int32_t(args.at(1))));if(site==0x43e7d0){++sound_calls;sound_ids.push_back(args.at(0));mission_sound_positional_request(sound,args.at(0),args.at(1),args.at(2),args.at(3),F+0x200);return 0;}if(site==0x49a620){for(U i=0;i<args.at(2);++i)b.write(args.at(0)+i,1,args.at(1));return args.at(0);}if(site==0x45d970){++insertions;return mission_actor_insert(insertion,args.at(0),args.at(1));}throw MissionTickError("Tree required service is unowned");};
 check(mission_object_tree_signal(b,A)==0x280000,"Real target contribution changed");
 BehaviorState expected=state.behavior;for(unsigned i=0;i<5;++i)behavior_rng_next(expected);
 bind_mission_object_tree_spawn(state,b,F);state.update_callbacks.at(0x451e70)(state,{0,0},{AiActorActionKind::update,{0,0},0x451e70});
 check(state.tables.groups[0].size()==4&&random_calls==5&&insertions==3&&sound_calls==1&&state.behavior.rng==expected.rng,"Canonical three-actor/RNG join changed");
 for(U slot=1;slot<4;++slot)check(state.tables.groups[0][slot].definition_index==2&&state.tables.groups[0][slot].position[2]==0x40000&&primary.read(A+slot*0x114+4,4)==D+2*0x13c,"Inserted actor source image changed");
 check(primary.read(A+0xe0,4)==10&&state.tables.groups[0][0].field_fc==64,"Spawner cooldown/FC changed");
 mission_object_tree_spawn(b,A,F,0xffffffff);check(primary.read(A+0xe0,4)==9&&random_calls==5,"Cooldown consumed foreign calls");

 // Consume exactly the producer's health0 images before any declared mutation.
 MissionObjectSeagullBindings gull{b.read,b.write,b.call};bind_mission_object_seagull(state,gull,S);const U bird=A+0x114;
 const auto update=[&](U slot){const auto callback=state.tables.groups[0].at(slot).field_110;check(callback==0x451fd0,"Producer update callback differs");state.update_callbacks.at(callback)(state,{0,slot},{AiActorActionKind::update,{0,slot},callback});};
 const auto move=[&](U slot){const auto callback=state.tables.groups[0].at(slot).field_10c;check(callback==0x452310,"Producer secondary callback differs");state.secondary_callbacks.at(callback)(state,{0,slot},{AiActorActionKind::secondary,{0,slot},callback});};
 for(U slot=1;slot<4;++slot){const U raw=A+slot*0x114;check(primary.read(raw+0x74,2)==0,"Tree manufactured bird health");update(slot);check((primary.read(raw+0x6c,2)&0x10c)==0x10c&&primary.read(raw+0x88,4)==0&&primary.read(raw+0xf0,4)==0&&primary.read(raw+0xfc,4)==0,"Produced health0 image took another branch");}
 check(random_calls==5&&state.behavior.rng==expected.rng&&sound_ids==std::vector<U>({0x4c,0x44,0x44,0x44}),"Health0 consumer changed RNG/sound order");
 const auto before_scan=tail_reads;check(mission_seagull_flock_first(gull,bird)==0&&mission_seagull_flock_index(gull,bird)==0xffffffff&&tail_reads>=before_scan+2,"Dead flock scan did not use the full retained physical tail");check(primary.read(A+1023*0x114+0x110,1)==0x5a,"Producer or scan changed retained tail bytes");
 update(1);check(primary.read(bird+0xfc,4)==0x400&&random_calls==5,"Dead follow-up branch differs");
 const auto z=primary.read(bird+0x2c,4);move(1);check(primary.read(bird+0x2c,4)==z&&primary.read(bird+0xf0,4)==0xfffffecc,"Dead movement did not use current ground/fall state");
 // Separate explicit current health mutation exercises a healthy leader.
 primary.write(bird+0x74,2,1);primary.write(bird+0x6c,1,0);const U counter=primary.read(bird+0x8f,1);BehaviorState healthy_expected=state.behavior;behavior_rng_next(healthy_expected);
 update(1);check(random_calls==6&&state.behavior.rng==healthy_expected.rng&&primary.read(bird+0xf4,4)==0&&primary.read(bird+0xfc,4)==16,"Declared healthy mutation did not use actual leader body");check(primary.read(bird+0x8f,1)==((counter+1)&255)&&primary.read(bird+0x8c,4)>>24==((counter+1)&255),"Leader byte8F/DWORD8C alias differs");
 const auto x=primary.read(bird+0x24,4),y=primary.read(bird+0x28,4),vx=primary.read(bird+0x3c,4),vy=primary.read(bird+0x40,4);check(vx||vy,"Healthy trig control has no velocity");move(1);check(primary.read(bird+0x24,4)==x+vx&&primary.read(bird+0x28,4)==y+vy,"Healthy movement used another actor owner");
 primary.write(bird+0xe0,4,0x12345678);primary.write(bird+0xe3,1,0xab);check(primary.read(bird+0xe0,4)==0xab345678,"ByteE3/DWORDE0 alias differs");
 const unsigned consumed_randoms=random_calls;
 primary.write(A+0xe0,4,0);primary.write(A+0xfc,4,123);auto strict=b;strict.call={};bool failed=false;try{mission_object_tree_spawn(strict,A,F);}catch(const MissionTickError&){failed=true;}check(failed&&primary.read(A+0xfc,4)==123,"Missing helper did not preserve error prefix");
 auto missing_tail=b;missing_tail.read=[&](U at,U width){if(at==G+255*0x114)throw MissionTickError("Physical tail is not bound");return b.read(at,width);};failed=false;try{mission_object_tree_signal(missing_tail,A);}catch(const MissionTickError&){failed=true;}check(failed,"Missing physical tail silently succeeded");
 auto missing_sound=b;missing_sound.call=[&](U site,const std::vector<U>& args)->U{if(site==0x43e7d0)throw MissionTickError("Sound is not bound");return b.call(site,args);};failed=false;try{mission_object_tree_spawn(missing_sound,A,F);}catch(const MissionTickError&){failed=true;}check(failed&&primary.read(A+0xe0,4)==10&&primary.read(A+0xfc,4)==123&&random_calls==consumed_randoms,"Missing sound lost exact prefix");
 std::cout<<"Tree to primary to Seagull checks passed; healthy branch is an explicit mutation control\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
