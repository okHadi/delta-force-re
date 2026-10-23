#include "dfre/mission_object_queries.hpp"
#include <cassert>
using namespace dfre;
int main(){MissionTickState s;s.tables.groups[0].resize(1);s.tables.groups[3].resize(256);s.people.resize(1);s.tables.groups[0][0].person_slot=0;s.tables.groups[0][0].field_70=1;s.people[0].field_74=0x10000;s.people[0].field_9c=1;auto& t=s.tables.groups[3][255];t.active=true;t.definition_index=1;t.field_70=2;t.position[0]=0x100000;MissionObjectTargetBindings b;bool failed=false;try{mission_object_target(s,{0,0},b);}catch(const MissionTickError&){failed=true;}assert(failed);b.sight_line=[](const auto&,const auto&){return 1;};b.frame_weight_10=[](const auto&){return 0x10000;};auto r=mission_object_target(s,{0,0},b);assert(r.source_actor==mission_actor_source_address(3,255));assert(r.score>0);
 const auto initial_score=r.score;b.frame_weight_10=[&](const auto& ref){s.extra_for(ref).field_6d=0x40;return 0x10000;};assert(mission_object_target(s,{0,0},b).score==initial_score*6);b.frame_weight_10=[](const auto&){return 0x10000;};s.extra_for({3,255}).field_6d=0;
 s.config.local_actor=255;s.globals.flags_67dbc9=8;assert(!mission_object_target(s,{0,0},b).source_actor);s.globals.flags_67dbc9=0;
 s.tables.groups[3].resize(1);failed=false;try{mission_object_target(s,{0,0},b);}catch(const MissionTickError&){failed=true;}assert(failed);unsigned reads=0;b.physical_slot=[&](std::size_t i){++reads;AiActorImage image{};if(i==255){image[0]=1;image[0x70]=2;image[0x26]=16;}return image;};r=mission_object_target(s,{0,0},b);assert(r.source_actor==mission_actor_source_address(3,255));assert(reads>255);assert(s.tables.groups[3].size()==1);
 s.config.local_actor.reset();s.tables.groups[0][0].position[0]=10*65536;unsigned target_reads=0;
 b.physical_slot=[&](std::size_t i){AiActorImage image{};if(i==255){++target_reads;image[0]=1;image[0x70]=2;image[0x26]=20;if(target_reads==4)s.tables.groups[0][0].position[0]=19*65536;}return image;};
 r=mission_object_target(s,{0,0},b);assert(r.source_actor==mission_actor_source_address(3,255));assert(r.score==0xc80000);assert(target_reads==8);
 EventStage e;mission_object_region_mark(e,1,0xffffffff);assert(e.flag_word_6e0050(1)==0x80000000u);mission_object_region_mark(e,63,63);assert(e.flag_word_6e0050(127)==0x80000000u);failed=false;try{mission_object_region_mark(e,64,0);}catch(const EventError&){failed=true;}assert(failed);
}
