#include "dfre/mission_object_smoke.hpp"
#include "dfre/mission_actor_insert_live.hpp"
#include "dfre/mission_actor_lifecycle.hpp"
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class B>U get(const B& b,U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(b.at(at+i))<<(i*8);return n;}
template<class B>void put(B& b,U at,U n,U width=4){for(U i=0;i<width;++i)b.at(at+i)=std::uint8_t(n>>(i*8));}
struct Fixture {
 static constexpr U A=0x7dde80,X=0x822e80;
 MissionTickState state;MissionAnimations animations{std::vector<MissionAnimationBinding>{}};std::array<MissionAudioRegistration,1024> registrations{};MissionActorLifecycle life;
 unsigned emits=0,removed=0;bool fail_emit=false;std::array<U,3> emitted{};
 Fixture():life(state,animations,registrations,{}){
  AiActorTables tables;tables.groups[0].resize(1);tables.groups[0][0].active=true;tables.groups[0][0].definition_index=1;std::vector<AiItemDefinition> definitions(2);definitions[1].field_28=6;
  init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(definitions),{},0x12345678);prepare_mission_extra_actor_pool(state);
  for(unsigned i=0;i<2;++i){state.tables.extra[i].active=true;state.tables.extra[i].definition_index=1;auto image=snapshot_mission_actor(state,{4,i});put(image,4,1);apply_mission_actor_image(state,{4,i},image);}
  state.tables.extra[0].field_6c=4;state.tables.extra[0].position[0]=11;state.tables.extra[0].position[1]=22;state.tables.extra[0].position[2]=33;state.extra_for({0,0}).field_f8=X;state.tables.groups[0][0].field_7c=0;
 }
 MissionObjectSmokeBindings bindings(){MissionObjectSmokeBindings b;
  const auto ref=[&](U at,U width)->std::pair<AiActorRef,U>{if(at>=A&&std::uint64_t(at-A)+width<=0x114)return {{0,0},at-A};if(at>=X&&std::uint64_t(at-X)+width<=1024*0x114){const U offset=at-X;check(offset%0x114+width<=0x114,"cross actor read");return {{4,offset/0x114},offset%0x114};}throw MissionTickError("Native smoke address is not owned");};
  b.read=[&,ref](U at,U width){const auto found=ref(at,width);return get(snapshot_mission_actor(state,found.first),found.second,width);};
  b.write=[&,ref](U at,U width,U value){const auto found=ref(at,width);auto image=snapshot_mission_actor(state,found.first);put(image,found.second,value,width);apply_mission_actor_image(state,found.first,image);};
  b.call=[this](U site,const std::vector<U>& args)->U{
   if(site==0x42a440){check(args.size()==3,"emitter arg count");++emits;std::copy(args.begin(),args.end(),emitted.begin());if(fail_emit){state.tables.groups[0][0].position[0]=9;throw MissionTickError("Emitter stopped");}state.tables.groups[0][0].field_7c=98;return 0;}
   if(site==0x477860)return behavior_rng_next(state.behavior);
   if(site==0x45da30){check(args==std::vector<U>{A},"wrong removal actor");++removed;life.remove(AiActorRef{0,0});return 0;}
   throw MissionTickError("Native smoke body is not owned");
  };return b;
 }
};
}
int main(){try{
 {Fixture f;bind_mission_object_smoke(f.state,f.bindings());const U old=f.state.behavior.rng;BehaviorState expected;expected.rng=old;const auto draw=behavior_rng_next(expected);AiActorAction action{AiActorActionKind::update,{0,0},0x42a520};f.state.update_callbacks.at(0x42a520)(f.state,{0,0},action);check(f.emits==1&&f.emitted==std::array<U,3>{11,22,33}&&f.state.tables.groups[0][0].field_7c==99,"generator current owner/count differs");check(f.state.behavior.rng==expected.rng&&f.state.tables.groups[0][0].field_fc==(draw>>13)+16,"generator RNG is not shared");f.fail_emit=true;bool failed=false;try{f.state.update_callbacks.at(0x42a520)(f.state,{0,0},action);}catch(const MissionTickError&){failed=true;}check(failed&&f.state.tables.groups[0][0].position[0]==9&&f.state.tables.groups[0][0].field_7c==99&&f.state.behavior.rng==expected.rng,"emitter error advanced tail");}
 {Fixture f;f.state.tables.extra[0].field_6c=0;f.registrations[0]={123,Fixture::A,4,0};auto b=f.bindings();const auto old=f.state.behavior.rng;mission_object_smoke_generator(b,Fixture::A);check(f.removed==1&&!f.emits&&!f.state.tables.groups[0][0].active&&!f.registrations[0].definition&&f.state.behavior.rng==old,"generator removal missed canonical lifecycle");}
 {Fixture f;auto b=f.bindings();const U at=Fixture::X+0x114;auto image=snapshot_mission_actor(f.state,{4,1});put(image,0x24,0xfffffff0);put(image,0x28,1);put(image,0x2c,2);put(image,0x40,0xfffffff8);put(image,0x44,0xffffffff);put(image,0x7c,0x20000);put(image,0x3b,255,1);apply_mission_actor_image(f.state,{4,1},image);bind_mission_object_smoke(f.state,b);AiActorAction action{AiActorActionKind::secondary,{4,1},0x42a590};f.state.secondary_callbacks.at(0x42a590)(f.state,{4,1},action);const auto moved=snapshot_mission_actor(f.state,{4,1});check(get(moved,0x24)==0xffffffe8&&get(moved,0x28)==0xffffffff&&get(moved,0x2c)==1,"canonical extra movement differs");check(get(moved,0x7c)==0x20000&&get(moved,0x3b,1)==0&&get(moved,0x44)==31&&get(moved,0x40)==0,"canonical smoke growth/counter differs");check(at==0x822f94&&f.state.tables.extra.size()==1024,"extra owner changed");}
 std::cout<<"smoke callback checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
