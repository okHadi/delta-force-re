#include "dfre/mission_event_actor_orders.hpp"
#include <cassert>
using namespace dfre;
namespace {
struct Fixture {MissionTickState state;Fixture(){AiActorTables tables;tables.groups[3].resize(1);tables.groups[2].resize(3);auto& actor=tables.groups[3][0];actor.active=true;actor.definition_index=1;actor.person_slot=0;actor.field_72=3;init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(3),0);state.world.behavior_world.lists.resize(2);auto& list=state.world.behavior_world.lists[0];list.count=3;list.indices[0]=2;list.indices[1]=0;list.indices[2]=1;for(unsigned i=0;i<3;++i){state.person_extra(i).field_a4=10+i;state.people[i].field_ac=20+i;state.people[i].field_b0=30+i;}}};
}
int main(){
    mission_event_actor_orders({},2,0);bool failed=false;try{mission_event_closest_marker({},0x7cca80,0);}catch(const MissionTickError&){failed=true;}assert(failed);
    {Fixture f;auto b=mission_event_actor_order_bindings(f.state);assert(b.read(0x7cca8c,4)==0x794680&&b.read(0x7cca8c,1)==0x80);assert(mission_event_closest_marker(b,0x7cca80,0)==2);assert(mission_event_closest_marker(b,0x7cca80,0x20000000)==2);
        f.state.tables.groups[2][0].position[0]=f.state.tables.groups[2][0].position[1]=f.state.tables.groups[2][0].position[2]=0x80000000;assert(mission_event_closest_marker(b,0x7cca80,0)==1);
        for(auto& a:f.state.tables.groups[2])for(auto& p:a.position)p=0;f.state.world.behavior_world.lists[0].count=33;f.state.world.behavior_world.lists[1].flags=1;assert(mission_event_closest_marker(b,0x7cca80,0)==32);
        f.state.tables.groups[3][0].person_slot.reset();assert(mission_event_closest_marker(b,0x7cca80,0xffffffff)==0);
    }
    {Fixture f;auto underlying=mission_event_actor_order_bindings(f.state);auto b=underlying;unsigned pointers=0;b.read=[&](auto at,auto width){if(at==0x7cca8c){if(++pointers==2)f.state.tables.groups[3][0].person_slot=1;if(pointers==4)f.state.tables.groups[3][0].person_slot=2;}return underlying.read(at,width);};
        mission_event_actor_orders(b,3,0);assert(pointers==4&&f.state.person_extra(0).field_a4==1&&f.state.people[0].tail_flag&&f.state.people[0].field_ac==20&&f.state.people[1].field_ac==0&&f.state.people[1].field_b0==31&&f.state.people[2].field_b0==2);
    }
    {Fixture f;auto underlying=mission_event_actor_order_bindings(f.state);auto b=underlying;unsigned pointers=0;b.read=[&](auto at,auto width){if(at==0x7cca8c&&++pointers==4)f.state.tables.groups[3][0].person_slot.reset();return underlying.read(at,width);};
        failed=false;try{mission_event_actor_orders(b,3,0);}catch(const MissionTickError&){failed=true;}assert(failed&&f.state.person_extra(0).field_a4==1&&f.state.people[0].field_ac==0&&f.state.people[0].field_b0==30);
    }
    {Fixture f;auto b=mission_event_actor_order_bindings(f.state);f.state.tables.groups[3][0].person_slot=99;assert(b.read(0x7ccaf0,4)==0x30000);failed=false;try{b.read(0x7cca8c,4);}catch(const MissionTickError&){failed=true;}assert(failed);f.state.tables.groups[3][0].person_slot=0;failed=false;try{mission_event_closest_marker(b,0x7cca80,2);}catch(const MissionTickError&){failed=true;}assert(failed);}
}
