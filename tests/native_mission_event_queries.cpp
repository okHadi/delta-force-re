#include "dfre/mission_event_queries.hpp"
#include <cassert>
using namespace dfre;
int main(){MissionTickState s;s.tables.groups[3].resize(3);s.tables.groups[0].resize(1);auto& a=s.tables.groups[3][0];a.field_72=0xffff;a.field_70=5;a.position[0]=10;a.position[1]=20;
 assert(mission_event_rect(s,{-1,0,0,11,21})==1);assert(mission_event_rect(s,{5,0,0,11,21})==0);assert(mission_event_rect(s,{-1,11,0,0,21})==0);assert(mission_event_rect(s,{-1,0,0,0,21})==0);
 a.active=true;a.definition_index=1;s.runtime_for({3,0}).link_90=mission_actor_source_address(0,0);s.tables.groups[0][0].field_72=2;
 assert(mission_event_linked_sides(s,-1,2)==1);unsigned counts=0;MissionEventQueryBindings b;b.current_count=[&](std::size_t){++counts;return counts==1?3:1;};assert(mission_event_linked_sides(s,-1,9,b)==0);assert(counts==2);
 s.runtime_for({3,0}).link_90=0x123;bool failed=false;try{mission_event_linked_sides(s,-1,2);}catch(const MissionTickError&){failed=true;}assert(failed);
 assert(mission_event_linked_sides(s,0,2)==0);
 s.runtime_for({3,0}).link_90=mission_actor_source_address(0,0);b={};unsigned links=0;b.before_read=[&](unsigned at){if(at==0x7cca80+0x90 && ++links==2)s.tables.groups[0][0].field_72=7;};assert(mission_event_linked_sides(s,-1,7,b)==1);assert(links==2);
 b={};counts=0;b.current_count=[&](std::size_t){++counts;return 1;};assert(mission_event_rect(s,{9,0,0,11,21},b)==0);assert(counts==4);
 b={};b.before_read=[&](unsigned at){if(at==0x7cca80+0x24)s.tables.groups[3][0].position[0]=11;};assert(mission_event_rect(s,{-1,0,0,11,21},b)==1);
 b={};b.current_count=[](std::size_t){return 4;};failed=false;try{mission_event_linked_sides(s,-1,8,b);}catch(const MissionTickError&){failed=true;}assert(failed);
 b.current_count=[](std::size_t){return -1;};assert(mission_event_rect(s,{-1,0,0,11,21},b)==0);
}
