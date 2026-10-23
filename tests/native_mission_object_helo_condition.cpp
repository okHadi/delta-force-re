#include "dfre/mission_object_helo_condition.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/events.hpp"
#include "dfre/actor_queries.hpp"
#include "dfre/world_hits.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
template<class B> U get(const B& b,U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(b.at(at+i))<<(i*8);return n;}
template<class B>void put(B& b,U at,U n,U width=4){for(U i=0;i<width;++i)b.at(at+i)=std::uint8_t(n>>(i*8));}
std::int32_t si(U n){std::int32_t result;std::memcpy(&result,&n,4);return result;}
struct Fixture {
 static constexpr U A=0x7dde80,P=0x794680,F=0x1304000;
 MissionTickState state;EventStage events;std::array<std::uint8_t,16> frame{};std::array<U,4> speeds{111,222,333,444};bool change_person=false,fail_event=false;unsigned marks=0;
 Fixture(){AiActorTables tables;tables.groups[0].resize(1);tables.groups[1].resize(1);tables.groups[2].resize(2);for(auto& rows:tables.groups)for(auto& a:rows){a.active=true;a.definition_index=1;}
  tables.groups[0][0].person_slot=0;tables.groups[0][0].field_72=1;tables.groups[1][0].object_id=77;tables.groups[1][0].position[0]=0x300000;
  tables.groups[2][0].field_7c=0x100000;tables.groups[2][1].position[0]=65536;tables.groups[2][1].field_7c=1;
  init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(2),123);
  state.world.behavior_world.lists.resize(3);for(auto& l:state.world.behavior_world.lists){l.count=2;l.indices[0]=0;l.indices[1]=1;}
  state.person_extra(0).field_a4=1;state.people[0].field_ac=1;state.person_extra(1).field_a4=1;state.people[1].field_ac=2;state.people[1].field_b0=1;frame.fill(0xa5);
 }
 AiPersonImage person(unsigned slot){auto image=ai_person_image(state.people.at(slot));put(image,0xa4,state.person_extra(slot).field_a4);put(image,0xac,state.people.at(slot).field_ac);put(image,0xb0,state.people.at(slot).field_b0);return image;}
 MissionObjectHeloBindings bindings(){MissionObjectHeloBindings b;
  b.read=[&](U at,U width)->U{
   for(unsigned group:{0u,1u,2u}){const U base=mission_actor_source_address(group,0);if(at>=base&&std::uint64_t(at-base)+width<=state.tables.groups[group].size()*0x114){const U off=at-base;auto image=snapshot_mission_actor(state,{std::uint8_t(group),off/0x114});if(group==0&&off/0x114==0)put(image,0xc,P+U(*state.tables.groups[0][0].person_slot)*0xb4);return get(image,off%0x114,width);}}
   if(at>=P&&std::uint64_t(at-P)+width<=0x168){const U off=at-P;return get(person(off/0xb4),off%0xb4,width);}
   if(at>=0x78f280&&std::uint64_t(at-0x78f280)+width<=state.world.behavior_world.lists.size()*0x88){const U off=at-0x78f280;const auto& list=state.world.behavior_world.lists.at(off/0x88);U field=off%0x88;U word=field<4?list.flags:field<8?list.count:list.indices.at((field-8)/4);return width==1?(word>>((field%4)*8))&255:word;}
   if(at>=F&&std::uint64_t(at-F)+width<=16)return get(frame,at-F,width);
   for(U i=0;i<speeds.size();++i)if(at==0x7936bc+i*0x40&&width==4)return speeds[i];
   throw MissionTickError("Native condition read has no current owner");
  };
  b.write=[&](U at,U width,U n){
   if(at>=A&&std::uint64_t(at-A)+width<=0x114){auto image=snapshot_mission_actor(state,{0,0});put(image,at-A,n,width);apply_mission_actor_image(state,{0,0},image);return;}
   if(at>=P&&std::uint64_t(at-P)+width<=0x168){const U off=at-P,slot=off/0xb4,field=off%0xb4;check(width==4,"person write width");auto& p=state.people.at(slot);if(field==0xa4){state.person_extra(slot).field_a4=n;p.tail_flag=n!=0;}else if(field==0xac)p.field_ac=n;else if(field==0xb0)p.field_b0=n;else if(field>=0x54&&field<=0x5c)p.position[(field-0x54)/4]=n;else throw MissionTickError("Native condition person field is not owned");return;}
   if(at>=F&&std::uint64_t(at-F)+width<=16){put(frame,at-F,n,width);return;}throw MissionTickError("Native condition write has no current owner");
  };
  b.call=[this](U site,const std::vector<U>& args)->U{
   if(site==0x45c6a0){auto current=bindings();return mission_object_helo_marker(current,args.at(0),args.at(1),args.at(2),args.at(3),args.at(4));}
   if(site==0x477670)return U(collision_approximate_length(si(args.at(0)),si(args.at(1)),si(args.at(2))));
   if(site==0x477770)return query_angle_477770(si(args.at(0)),si(args.at(1)));
   if(site==0x42ee30){events.set_flag_42ee30(args.at(0),args.at(1),args.at(2));++marks;if(change_person){state.tables.groups[0][0].person_slot=1;state.people[0].field_ac=2;state.world.behavior_world.lists[2].count=1;speeds[1]=777;}if(fail_event)throw MissionTickError("Native event stopped");return 0;}
   throw MissionTickError("Native condition body is not owned");
  };return b;
 }
};
}
int main(){try{
 {Fixture f;auto b=f.bindings();check(mission_object_helo_condition(b,Fixture::A,Fixture::F,16)==1,"waypoint result");check(f.events.flag_42e580(1,1,0)&&f.marks==1,"canonical event flag missing");check(f.state.people[0].field_b0==1&&get(snapshot_mission_actor(f.state,{0,0}),0x88)==222,"canonical entry/speed missing");}
 {Fixture f;f.state.world.behavior_world.lists[1].count=1;f.state.world.behavior_world.lists[1].flags=1;f.state.tables.groups[0][0].position[0]=123;auto b=f.bindings();check(!mission_object_helo_condition(b,Fixture::A,Fixture::F,16),"terminal list result");check(!f.state.person_extra(0).field_a4&&!f.state.people[0].field_ac&&f.state.people[0].position[0]==123,"terminal canonical person writes differ");}
 {Fixture f;f.change_person=true;auto b=f.bindings();mission_object_helo_condition(b,Fixture::A,Fixture::F,16);check(f.state.people[0].field_b0==1&&f.state.people[0].field_ac==2&&f.state.people[1].field_b0==0&&get(snapshot_mission_actor(f.state,{0,0}),0x88)==777,"captured person/current actor reads differ");}
 {Fixture f;f.fail_event=true;auto b=f.bindings();bool failed=false;try{mission_object_helo_condition(b,Fixture::A,Fixture::F,16);}catch(const MissionTickError&){failed=true;}check(failed&&f.events.flag_42e580(1,1,0)&&f.state.people[0].field_b0==0,"event failure prefix differs");}
 {Fixture f;auto b=f.bindings();f.state.person_extra(0).field_a4=0;const auto before=f.frame;check(!mission_object_helo_condition(b,Fixture::A,Fixture::F,16)&&f.frame==before,"inactive output changed");f.state.person_extra(0).field_a4=3;f.state.people[0].field_ac=0x867e80;f.state.people[0].field_b0=77;check(mission_object_helo_marker(b,Fixture::A,Fixture::F+8,Fixture::F+12,Fixture::F,Fixture::F+4)==1,"raw nonmarker target missing");check(get(f.frame,0)==0x100000&&get(f.frame,12)==0x300000,"raw target distance/radius differ");f.state.people[0].field_b0=78;const auto saved=f.frame;check(!mission_object_helo_marker(b,Fixture::A,Fixture::F+8,Fixture::F+12,Fixture::F,Fixture::F+4)&&f.frame==saved,"false target changed outputs");b.call={};f.state.people[0].field_b0=77;bool failed=false;try{mission_object_helo_marker(b,Fixture::A,Fixture::F+8,Fixture::F+12,Fixture::F,Fixture::F+4);}catch(const MissionTickError&){failed=true;}check(failed,"missing math accepted");}
 std::cout<<"helicopter condition checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
