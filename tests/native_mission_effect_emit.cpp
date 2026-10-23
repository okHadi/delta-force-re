#include "dfre/mission_effect_emit.hpp"
#include "dfre/mission_actor_insert_live.hpp"
#include "dfre/mission_live.hpp"
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
template<class B>U get(const B& b,U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(b.at(at+i))<<(i*8);return n;}
template<class B>void put(B& b,U at,U width,U n){for(U i=0;i<width;++i)b.at(at+i)=std::uint8_t(n>>(i*8));}
struct Fixture {
 static constexpr U F=0x1304000,G=0x1306000,D=0x1302000,T=0x8e44b0;
 MissionTickState state;std::array<std::uint8_t,0x114> frame{};std::array<std::uint8_t,12> args{};std::array<std::uint8_t,68> table{};
 U base=D,count=0,allocation=0;unsigned draws=0;bool mutate=true;
 Fixture(){std::vector<AiItemDefinition> definitions(2);definitions[1]={7016,0,17,0x453810,0x42a590,0,8,480};init_mission_tick(state,{},AiActorTables{},AiItemDefinitionTable(definitions),{},0x12345678);prepare_mission_extra_actor_pool(state);put(args,0,4,1);put(args,4,4,2);put(args,8,4,3);frame.fill(0xa5);table.fill(0xa5);}
 U read(U at,U width){
  if(at>=F&&std::uint64_t(at-F)+width<=frame.size())return get(frame,at-F,width);
  if(at>=G&&std::uint64_t(at-G)+width<=args.size())return get(args,at-G,width);
  if(at>=T&&std::uint64_t(at-T)+width<=table.size())return get(table,at-T,width);
  if(at==0xc23810&&width==4)return base;
  if(at>=D&&at<D+2*0x13c&&width==4){const U off=at-D;const auto& item=state.definitions.entries().at(off/0x13c);switch(off%0x13c){case 0x20:return item.id;case 0x24:return item.flags24;case 0x28:return item.field_28;case 0x6c:return item.field_6c;case 0x84:return item.field_84;case 0x90:return item.field_90;case 0x9c:return item.field_9c;default:break;}}
  throw MissionEffectEmitError("Native effect read is not owned");
 }
 void write(U at,U width,U n){if(at>=F&&std::uint64_t(at-F)+width<=frame.size()){put(frame,at-F,width,n);return;}if(at>=G&&std::uint64_t(at-G)+width<=args.size()){put(args,at-G,width,n);return;}if(at>=T&&std::uint64_t(at-T)+width<=table.size()){put(table,at-T,width,n);return;}throw MissionEffectEmitError("Native effect write is not owned");}
 MissionEffectEmitBindings bindings(){MissionEffectEmitBindings b;b.read=[this](U a,U w){return read(a,w);};b.write=[this](U a,U w,U n){write(a,w,n);};
  b.call=[this](U site,const std::vector<U>& a)->U{
   if(site==0x4828c0)return U(state.definitions.find_index(a.at(0)));
   if(site==0x49a620){check(a==std::vector<U>{F,0,0x114},"wrong emitter clear");frame.fill(0);if(mutate){put(args,0,4,7);put(args,4,4,8);put(args,8,4,9);base=D+0x13c;}return F;}
   if(site==0x477860){++draws;if(mutate&&draws==1){auto entries=state.definitions.entries();entries[1].field_6c=99;entries[1].field_84=0x45e000;entries[1].field_90=0x4563a0;state.definitions=AiItemDefinitionTable(std::move(entries));}return behavior_rng_next(state.behavior);}
   if(site==0x45d970){check(a==std::vector<U>{F,1},"wrong emitter allocation");MissionActorInsertBindings delegated;delegated.read=[this](U at,U w){return read(at,w);};delegated.write=[this](U at,U w,U n){write(at,w,n);};auto insert=mission_actor_insert_live_bindings(state,count,delegated,[](U raw){if(raw!=D+0x13c)throw MissionTickError("Unknown effect definition pointer");return 1u;});allocation=mission_actor_insert(insert,F,1);return allocation;}
   throw MissionEffectEmitError("Native effect required resource body is not bound");
  };return b;
 }
};
}
int main(){try{
 {Fixture f;auto b=f.bindings();BehaviorState expected;expected.rng=f.state.behavior.rng;const U first=behavior_rng_next(expected),second=behavior_rng_next(expected);mission_effect_emit(b,Fixture::G,Fixture::F,f.frame.size());check(f.allocation==0x822e80&&f.count==1&&f.state.tables.extra.size()==1024,"emitter used another actor pool");const auto image=snapshot_mission_actor(f.state,{4,0});check(f.state.extra_actor_encoding[0].raw_definition==Fixture::D+0x13c&&f.state.tables.extra[0].definition_index==1&&f.state.tables.extra[0].field_10==17,"held definition/model differs");check(f.state.tables.extra[0].position[0]==7&&f.state.tables.extra[0].position[1]==8&&f.state.tables.extra[0].position[2]==9,"current caller arguments lost");check(f.state.tables.extra[0].field_110==0x45e000&&f.state.tables.extra[0].field_10c==0x4563a0,"callback metadata was not current");check(get(image,0x38)==first<<16&&f.state.tables.extra[0].field_7c==(second+0x20000)>>4&&f.state.behavior.rng==expected.rng&&f.draws==2,"emitter RNG is not shared");}
 {Fixture f;f.mutate=false;for(auto& actor:f.state.tables.extra){actor.active=true;actor.definition_index=1;}auto b=f.bindings();const auto rng=f.state.behavior.rng;mission_effect_emit(b,Fixture::G,Fixture::F,f.frame.size());check(!f.allocation&&!f.count&&f.draws==2&&f.state.behavior.rng!=rng&&get(f.frame,0)==1,"full pool changed emitter order");}
 {Fixture f;auto b=f.bindings();bool failed=false;try{mission_effect_definitions_initialize(b);}catch(const MissionEffectEmitError&){failed=true;}check(failed&&!get(f.table,0)&&get(f.table,4)==0xa5a5a5a5&&f.draws==0,"missing resource did not preserve table prefix");failed=false;try{mission_effect_emit(b,Fixture::G,Fixture::F,0x113);}catch(const MissionEffectEmitError&){failed=true;}check(failed&&!f.draws&&!f.count,"short frame advanced emission");}
 std::cout<<"effect emission checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
