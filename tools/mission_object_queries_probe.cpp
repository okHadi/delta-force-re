#include "dfre/mission_object_queries.hpp"
#include <iostream>
using namespace dfre;
int main(){unsigned seed;while(std::cin>>seed){
 if(seed>=10000){
  const auto id=seed-10000;MissionTickState s;s.tables.groups[0].resize(1);s.people.resize(1);auto& shooter=s.tables.groups[0][0];shooter.person_slot=0;shooter.field_70=1;shooter.position[0]=10*65536;shooter.position[1]=20*65536;shooter.position[2]=30*65536;s.people[0].field_74=0x10000;s.people[0].field_9c=1;
  const std::array<unsigned,3> target{(20+id%4)*65536,(20+id%3)*65536,(30+id%5)*65536};unsigned target_reads=0,tail_reads=0,calls=0;MissionObjectTargetBindings b;
  b.physical_slot=[&](std::size_t index){++tail_reads;AiActorImage image{};if(index==255){++target_reads;image[0]=1;image[0x70]=2;for(unsigned axis=0;axis<3;++axis)for(unsigned j=0;j<4;++j)image[0x24+axis*4+j]=std::uint8_t(target[axis]>>(j*8));if(target_reads>=4&&target_reads<=6){const auto axis=target_reads-4;shooter.position[axis]=target[axis]-(axis==0?65536:0);}}return image;};
  b.sight_line=[&](const auto&,const auto&){++calls;return 1;};b.frame_weight_10=[&](const auto&){return (id%3+1)*65536;};const auto r=mission_object_target(s,{0,0},b);
  std::cout<<"{\"target\":"<<r.source_actor<<",\"score\":"<<r.score<<",\"calls\":"<<calls<<",\"tail_reads\":"<<tail_reads<<",\"shooter\":["<<shooter.position[0]<<','<<shooter.position[1]<<','<<shooter.position[2]<<"]}\n";continue;
 }
 MissionTickState s;s.tables.groups[0].resize(1);s.tables.groups[3].resize(256);s.people.resize(2);auto& shooter=s.tables.groups[0][0];shooter.person_slot=0;shooter.field_70=1;shooter.angle_yaw=(seed%4)*0x40000000u;shooter.angle_pitch=(seed%3-1)*0x01000000u;s.people[0].field_74=(seed%5)*0x10000;s.people[0].field_9c=seed%2;s.people[1].field_74=0x18000;s.people[1].field_9c=1;s.config.local_actor=seed%8;s.globals.flags_67dbc9=seed&8;
 for(unsigned i=0;i<8;++i){auto& a=s.tables.groups[3][i];a.active=(seed+i)%7!=0;a.definition_index=a.active?1:0;a.field_70=(seed+i)%5?2:1;a.field_6c=std::uint8_t((seed+i)%9==0?4:i&1);s.extra_for({3,i}).field_6d=(seed+i)%4==0?0x40:0;const unsigned distance=(seed*17+i*23)%1100;const auto axis=(seed+i)%4;a.position[axis&1]=axis<2?distance*65536u:0u-distance*65536u;a.position[2]=((seed+i)%9-4)*65536u;}
 unsigned calls=0;MissionObjectTargetBindings b;b.sight_line=[&](const AiActorRef&,const AiActorRef& t){++calls;if(seed&16){s.extra_for(t).field_6d^=0x40;s.tables.groups[3][t.index].field_6c^=1;shooter.person_slot=1;}if(seed&32)shooter.angle_yaw+=0x10000000u;return std::int32_t((seed+t.index)%4);};b.frame_weight_10=[&](const AiActorRef& t){if(seed&64)s.extra_for(t).field_6d^=0x40;return (seed+t.index)%4*0x8000u;};const auto r=mission_object_target(s,{0,0},b);
 EventStage e;const auto a=seed%62,bit=std::uint32_t(std::int32_t(seed%192)-64);const auto region=a+(bit&0x80000000u?1u:0u);mission_object_region_mark(e,region,bit);
 std::cout<<"{\"target\":"<<r.source_actor<<",\"score\":"<<r.score<<",\"calls\":"<<calls<<",\"flags\":[";for(unsigned i=0;i<128;++i){if(i)std::cout<<',';std::cout<<e.flag_word_6e0050(i);}std::cout<<"]}\n";
}}
