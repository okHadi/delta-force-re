#include "dfre/mission_event_person.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do{if(!(x))throw std::runtime_error("event person: " #x);}while(false)
namespace {
using namespace dfre;using U=std::uint32_t;
struct Host final:EventHost {
 MissionTickState& state;EventStage* stage=nullptr;std::function<U(U)> table;std::function<std::uint16_t()> fp=[](){return std::uint16_t(0x37f);};
 explicit Host(MissionTickState& s):state(s){}
 void action_4601a0(std::size_t index)override{auto b=mission_event_person_bindings(state,stage->action(index),0x2201000,table,fp);mission_event_person_edits(b,0x2201000);}
};
void initialize(MissionTickState& state){
 AiActorTables actors;U person=0;for(auto group:{1u,3u,0u}){actors.groups[group].resize(2);for(auto& actor:actors.groups[group]){actor.person_slot=person++;actor.field_72=3;}}
 std::vector<AiPersonState> people(6);for(auto& p:people){p.flags24=0xff;p.flags25=0xfe;p.field_70=0x12345678;}
 init_mission_tick(state,{},std::move(actors),AiItemDefinitionTable{},std::move(people),0);
}
}
int main(){try{
 using namespace dfre;
 for(U kind=1;kind<=15;++kind){
  MissionTickState state;initialize(state);Host host(state);EventStage stage(&host);host.stage=&stage;
  EventTableRecord action{};action[1]=3;action[2]=kind;action[3]=3;action[4]=kind==1?2:500;
  unsigned lookups=0;host.table=[&](U address){CHECK(address==0x4fb2d4);++lookups;return 0xdeadbeefu;};
  stage.load_tables({}, {},{action});stage.dispatch_action(0);CHECK(lookups==(kind==1?6u:0u));
  for(const auto& p:state.people){switch(kind){case 1:CHECK(p.field_64==0xdeadbeef);break;case 2:case 3:case 4:case 15:CHECK(p.flags24==0xff);break;case 5:CHECK(p.field_9c==2);break;case 6:CHECK(p.field_9c==0);break;case 7:CHECK(p.field_98==U(std::int32_t(500u<<16)/360));break;case 8:CHECK(p.field_70==13107);break;case 9:CHECK(p.field_74==327680);break;case 10:CHECK(p.field_88==31000);break;case 11:CHECK(p.field_8c==31000);break;case 12:CHECK(p.field_94==31000);break;case 13:CHECK(p.field_7c==355);break;case 14:CHECK(p.flags25==0xff);break;}}
  if(kind==2||kind==3||kind==4||kind==8||kind==14||kind==15){stage.action(0)[4]=0;stage.dispatch_action(0);for(const auto& p:state.people){if(kind==8)CHECK(p.field_70==13107);else if(kind==14)CHECK(p.flags25==0xfe);else CHECK(p.flags24==std::uint8_t(0xffu&~(kind==2?2u:kind==3?0x10u:kind==4?0x20u:1u)));}}
 }
 {
  MissionTickState state;initialize(state);Host host(state);EventStage stage(&host);host.stage=&stage;EventTableRecord action{};action[1]=3;action[2]=1;action[3]=3;action[4]=2;stage.load_tables({}, {},{action});
  unsigned lookups=0;host.table=[&](U at){CHECK(at==0x4fb2d4);++lookups;state.tables.groups[1][0].person_slot=1;state.tables.groups[1].resize(1);stage.action(0)[2]=2;stage.action(0)[4]=0;return 0xabcdef01u;};stage.dispatch_action(0);
  CHECK(lookups==1&&state.people[0].field_64==0xabcdef01&&state.people[1].field_64==0&&state.people[1].flags24==0xff);for(std::size_t slot=2;slot<6;++slot)CHECK(state.people[slot].flags24==0xfd);
 }
 {
  MissionTickState state;initialize(state);Host host(state);EventStage stage(&host);host.stage=&stage;EventTableRecord action{};action[1]=3;action[2]=1;action[3]=3;action[4]=2;stage.load_tables({}, {},{action});unsigned calls=0;host.table=[&](U){if(++calls==2)throw std::runtime_error("table owner failed");return 99u;};bool failed=false;try{stage.dispatch_action(0);}catch(const std::runtime_error&){failed=true;}CHECK(failed&&state.people[0].field_64==99&&state.people[1].field_64==0);
  host.table={};failed=false;try{stage.dispatch_action(0);}catch(const MissionTickError&){failed=true;}CHECK(failed);
  stage.action(0)[2]=8;stage.action(0)[4]=100;host.fp={};failed=false;try{stage.dispatch_action(0);}catch(const MissionTickError&){failed=true;}CHECK(failed&&state.people[0].field_70==0x12345678);
  auto b=mission_event_person_bindings(state,stage.action(0),0x2201000,{},{});state.tables.groups[0].resize(1025);failed=false;try{b.read(0x8cfa1c,4);}catch(const MissionTickError&){failed=true;}CHECK(failed);failed=false;try{b.read(0x822e80+0xc,4);}catch(const MissionTickError&){failed=true;}CHECK(failed);
 }
 CHECK(mission_event_person_rate(0,0x37f)==0x80000000u);bool failed=false;try{mission_event_person_rate(1,0x17f);}catch(const MissionTickError&){failed=true;}CHECK(failed);failed=false;try{mission_event_person_rate(1,0x340);}catch(const MissionTickError&){failed=true;}CHECK(failed);
 std::cout<<"event person checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
