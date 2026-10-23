#include "dfre/mission_object_sight.hpp"
#include <iostream>
using namespace dfre;
int main(){unsigned seed;while(std::cin>>seed){MissionTickState s;s.tables.groups[3].resize(2);s.tables.groups[0].resize(seed%4);s.tables.groups[1].resize(seed%3);s.world.water_level=seed%3?0:0x10000;auto& from=s.tables.groups[3][0];auto& to=s.tables.groups[3][1];for(unsigned axis=0;axis<3;++axis){from.position[axis]=(seed*(axis+3)%31)*65536u;to.position[axis]=from.position[axis]+((seed*(axis+7)%17)-8)*65536u;}
 if(seed%19==0)for(unsigned axis=0;axis<3;++axis)to.position[axis]=from.position[axis];
 if(seed%23==0)to.position[0]=from.position[0]+0x6400000u;
 if(seed%29==0)to.position[1]=from.position[1]+0x80000000u;
 std::vector<unsigned> reads;std::vector<std::vector<unsigned>> calls;MissionObjectSightBindings b;b.before_read=[&](unsigned at){reads.push_back(at);};
 b.terrain=[&](const AiActorRef&,const auto& dir,int length){calls.push_back({0x47e4b0,from.position[0],from.position[1],from.position[2],unsigned(dir[0]),unsigned(dir[1]),unsigned(dir[2]),unsigned(length)});if(seed&4){from.position[2]+=0x30000;to.position[2]-=0x20000;s.tables.groups[1].resize(4);s.world.water_level=0x20000;}return seed%7==1?-0x1000:seed%7==2?0:1;};
 b.model_list=[&](std::size_t g,int count,const WorldHitQuery& q,WorldModelHit& hit){std::vector<unsigned> row{0x42db80,unsigned(g),unsigned(count)};for(auto x:q.origin)row.push_back(unsigned(x));for(auto x:q.end)row.push_back(unsigned(x));for(auto x:q.direction)row.push_back(unsigned(x));row.push_back(unsigned(q.length));for(auto x:q.bounds)row.push_back(unsigned(x));for(auto x:q.skip)row.push_back(x);calls.push_back(row);hit.distance=0x12345+int(g);if(g==1 && seed&8){s.tables.groups[0].resize(5);s.world.water_level=0x30000;from.position[2]=0x20000;to.position[2]=0x40000;}return seed%11==unsigned(g)?0:int(1+(seed+g)%4);};
 const auto result=mission_object_sight(s,{3,0},{3,1},b);std::cout<<"{\"result\":"<<result<<",\"reads\":[";for(std::size_t i=0;i<reads.size();++i){if(i)std::cout<<',';std::cout<<reads[i];}std::cout<<"],\"calls\":[";for(std::size_t i=0;i<calls.size();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<calls[i].size();++j){if(j)std::cout<<',';std::cout<<calls[i][j];}std::cout<<']';}std::cout<<"]}\n";
}}
