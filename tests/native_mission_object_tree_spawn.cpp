#include "dfre/mission_object_tree_spawn.hpp"
#include "dfre/mission_primary_actor_live.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/mission_positional_sound.hpp"
#include <map>
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}}
int main(){try{
 constexpr U A=0x7dde80,G=0x7cca80,D=0x18000000,W=0x1d001000,F=0x1d000000;
 AiActorTables tables;tables.groups[0].resize(1);tables.groups[0][0].active=true;tables.groups[0][0].definition_index=1;tables.groups[0][0].field_70=1;
 // This fixture explicitly owns every physical row. No implicit tail provider is used.
 tables.groups[3].resize(256);tables.groups[3][0].active=true;tables.groups[3][0].definition_index=1;tables.groups[3][0].field_70=2;
 std::vector<AiItemDefinition> definitions(3);definitions[1].field_28=6;definitions[2].field_28=6;
 MissionTickState state;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),{},7);
 std::map<U,std::uint8_t> bytes;for(U i=0;i<3*0x13c;++i)bytes[D+i]=0;for(U i=0;i<0x114;++i)bytes[F+i]=0xa5;for(U i=0;i<0x100;++i)bytes[W+i]=0;
 const auto set=[&](U at,U n){for(U i=0;i<4;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));};
 set(D+2*0x13c+0x20,0xfd3);set(D+2*0x13c+0x28,6);set(D+0x13c+0x78,W);set(W+0x10,0x10000);
 MissionPrimaryActorLiveBindings pb;pb.initial_definition=[=](std::size_t){return D+0x13c;};pb.definition_index=[=](U at){if(at<D||at>=D+3*0x13c||(at-D)%0x13c)throw MissionTickError("Tree definition is not current");return (at-D)/0x13c;};pb.definition_address=[=](U index){return D+index*0x13c;};pb.delegated.read=[&](U at,U width){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;};pb.delegated.write=[&](U at,U width,U n){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));};
 MissionPrimaryActorLive primary(state,pb,std::vector<AiActorImage>(1023));auto insertion=primary.insertion_bindings();
 unsigned random_calls=0,insertions=0,sound_calls=0;
 MissionObjectTreeBindings b;
 b.read=[&](U at,U width){if(primary.claims(at,width))return primary.read(at,width);if(at==0xc23810)return D;if(at==0xc23814)return 3u;if(at==0xc23820)return 0u;if(at==0x67dbc9)return 0u;
  if(at>=G&&std::uint64_t(at)+width<=G+256*0x114){const U slot=(at-G)/0x114,offset=(at-G)%0x114;check(offset+width<=0x114,"Cross-record read");auto image=snapshot_mission_actor(state,{3,slot});if(offset==4&&width==4)return state.tables.groups[3][slot].active?D+state.tables.groups[3][slot].definition_index*0x13c:0u;U n=0;for(U i=0;i<width;++i)n|=U(image.at(offset+i))<<(8*i);return n;}return pb.delegated.read(at,width);};
 b.write=[&](U at,U width,U n){if(primary.claims(at,width)){primary.write(at,width,n);return;}pb.delegated.write(at,width,n);};
 MissionSoundBankBindings sound;sound.read=[](U at,U width){check(at==0x4f2ea8+0x4c*0x28&&width==4,"Unexpected cold sound read");return 0u;};
 b.call=[&](U site,const std::vector<U>& args)->U{if(site==0x44fba0)return mission_object_tree_signal(b,args.at(0));if(site==0x477670)return U(collision_approximate_length(std::int32_t(args.at(0)),std::int32_t(args.at(1)),std::int32_t(args.at(2))));if(site==0x477860){++random_calls;return behavior_rng_next(state.behavior);}if(site==0x43e7d0){++sound_calls;mission_sound_positional_request(sound,args.at(0),args.at(1),args.at(2),args.at(3),F+0x200);return 0;}if(site==0x49a620){for(U i=0;i<args.at(2);++i)b.write(args.at(0)+i,1,args.at(1));return args.at(0);}if(site==0x45d970){++insertions;return mission_actor_insert(insertion,args.at(0),args.at(1));}throw MissionTickError("Tree required service is unowned");};
 check(mission_object_tree_signal(b,A)==0x280000,"Real target contribution changed");
 BehaviorState expected=state.behavior;for(unsigned i=0;i<5;++i)behavior_rng_next(expected);
 bind_mission_object_tree_spawn(state,b,F);state.update_callbacks.at(0x451e70)(state,{0,0},{AiActorActionKind::update,{0,0},0x451e70});
 check(state.tables.groups[0].size()==4&&random_calls==5&&insertions==3&&sound_calls==1&&state.behavior.rng==expected.rng,"Canonical three-actor/RNG join changed");
 for(U slot=1;slot<4;++slot)check(state.tables.groups[0][slot].definition_index==2&&state.tables.groups[0][slot].position[2]==0x40000&&primary.read(A+slot*0x114+4,4)==D+2*0x13c,"Inserted actor source image changed");
 check(primary.read(A+0xe0,4)==10&&state.tables.groups[0][0].field_fc==64,"Spawner cooldown/FC changed");
 mission_object_tree_spawn(b,A,F,0xffffffff);check(primary.read(A+0xe0,4)==9&&random_calls==5,"Cooldown consumed foreign calls");
 primary.write(A+0xe0,4,0);primary.write(A+0xfc,4,123);auto strict=b;strict.call={};bool failed=false;try{mission_object_tree_spawn(strict,A,F);}catch(const MissionTickError&){failed=true;}check(failed&&primary.read(A+0xfc,4)==123,"Missing helper did not preserve error prefix");
 auto missing_tail=b;missing_tail.read=[&](U at,U width){if(at==G+255*0x114)throw MissionTickError("Physical tail is not bound");return b.read(at,width);};failed=false;try{mission_object_tree_signal(missing_tail,A);}catch(const MissionTickError&){failed=true;}check(failed,"Missing physical tail silently succeeded");
 auto missing_sound=b;missing_sound.call=[&](U site,const std::vector<U>& args)->U{if(site==0x43e7d0)throw MissionTickError("Sound is not bound");return b.call(site,args);};failed=false;try{mission_object_tree_spawn(missing_sound,A,F);}catch(const MissionTickError&){failed=true;}check(failed&&primary.read(A+0xe0,4)==10&&primary.read(A+0xfc,4)==123&&random_calls==5,"Missing sound lost exact prefix");
 std::cout<<"tree spawn canonical checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
