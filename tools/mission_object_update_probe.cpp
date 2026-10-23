#include "dfre/mission_object_update.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {void put(AiActorImage& a,unsigned at,unsigned n){std::memcpy(a.data()+at,&n,4);}std::string hex(AiActorImage a){put(a,0xc,0);const char* d="0123456789abcdef";std::string s;for(auto n:a){s+=d[n>>4];s+=d[n&15];}return s;}}
int main(){unsigned seed;while(std::cin>>seed){const auto body=std::array<unsigned,9>{0x429bc0,0x4519a0,0x4534a0,0x45c340,0x45c3a0,0x45c400,0x453810,0x4533f0,0x453930}[seed%9];const auto message=std::array<unsigned,5>{0,1,4,0x80000000,0xffffffff}[(seed/9)%5];const auto host=std::array<unsigned,3>{0,1,0x80000000}[(seed/45)%3];
 AiActorTables tables;tables.groups[0].resize(1);auto& a=tables.groups[0][0];a.active=true;a.definition_index=1;a.person_slot=0;a.field_7c=(seed/45%2)*65536;a.position[0]=1;a.position[1]=2;a.position[2]=0x7fff0000;a.field_6c=std::array<std::uint8_t,4>{0,4,0x80,0xff}[(seed/135)%4];MissionTickState s;init_mission_tick(s,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(1),0x12345678);s.globals.mission_67f528=host;auto image=snapshot_mission_actor(s,{0,0});put(image,0x1c,seed/9%2);put(image,0xf8,0x123);put(image,0xfc,0xabcdef);apply_mission_actor_image(s,{0,0},image);s.extra_for({0,0}).field_74=std::array<std::uint16_t,4>{1,0,0x8000,0xffff}[(seed/540)%4];
 std::ostringstream trace;unsigned calls=0;MissionObjectLiveBindings b;b.global=[&](auto& current,unsigned at){trace<<'G'<<at<<';';if(seed%5==0)current.tables.groups[0][0].field_6c^=0x20;return 0x11122233u;};b.definition=[&](auto& current,unsigned index,unsigned at){trace<<'D'<<index<<','<<at<<';';if(seed%13==0){current.tables.groups[0][0].position[0]^=0xffffffff;current.behavior.rng=0x98765432;}return 0x77u;};
 b.call=[&](auto& current,const auto&,const ObjectBehaviorAction& action)->MissionObjectCallResult{trace<<'C'<<action.site;for(auto n:action.args)trace<<','<<n;trace<<',';const char* d="0123456789abcdef";for(auto n:action.payload)trace<<d[n>>4]<<d[n&15];trace<<';';if(calls++==0&&seed%11==0){current.tables.groups[0][0].position[0]^=0xffffffff;current.behavior.rng=0xcafebabe;}return {};};
 mission_object_update(s,{0,0},body,message,b);std::cout<<s.behavior.rng<<' '<<hex(snapshot_mission_actor(s,{0,0}))<<' '<<trace.str()<<'\n';
}}
