#include "dfre/mission_event_queries.hpp"
#include <iostream>
#include <vector>
using namespace dfre;
int main(){unsigned seed;while(std::cin>>seed){
 MissionTickState s;for(unsigned g=0;g<4;++g)s.tables.groups[g].resize(5);
 for(unsigned g=0;g<4;++g)for(unsigned i=0;i<5;++i){auto& a=s.tables.groups[g][i];a.active=(seed+i+g)%3!=0;a.definition_index=a.active?1:0;a.field_70=99;a.field_72=std::uint16_t((seed+i+g)%5-2);a.position[0]=(seed*13+i*7+g)%41-20;a.position[1]=(seed*7+i*11+g)%41-20;s.runtime_for({std::uint8_t(g),i}).link_90=(seed+i)%4?mission_actor_source_address((g+1)%4,(i+2)%5):0;}
 std::vector<unsigned> reads;unsigned c3=0,c0=0;
 MissionEventQueryBindings b;b.current_count=[&](std::size_t g){const auto n=g==3?++c3:++c0;if(seed%11==0)return -1;if(seed%7==0 && n>1)return 2;return 5;};b.before_read=[&](unsigned at){reads.push_back(at);if(seed%13==0 && at==0x7cca80+0x90)s.runtime_for({3,0}).link_90=mission_actor_source_address(0,0);};
 const auto first=std::int32_t(seed%5)-2,second=std::int32_t((seed/5)%5)-2;
 unsigned answer=seed%2?mission_event_linked_sides(s,first,second,b):mission_event_rect(s,{first,-15,-12,15,12},b);
 std::cout<<"{\"result\":"<<answer<<",\"reads\":[";for(std::size_t i=0;i<reads.size();++i){if(i)std::cout<<',';std::cout<<reads[i];}std::cout<<"]}\n";
}}
