#include "dfre/mission_fragment_spawn.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
using namespace dfre;
namespace {
std::string hex(const ProjectileRecordImage& a){std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto b:a)s<<std::setw(2)<<unsigned(b);return s.str();}
void put(ProjectileRecordImage& a,unsigned at,std::uint32_t value){for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(value>>(8*i));}
}
int main(){unsigned seed;while(std::cin>>seed){
 MissionTickState state;state.projectiles.resize(512);for(auto& g:state.tables.groups)g.resize(1024);state.tables.groups[3].resize(256);state.tables.groups[2].resize(512);state.tables.extra.resize(1024);MissionProjectileSpawnCells cells;
 cells.ring_index=std::array<std::uint32_t,4>{0,511,0xffffffff,1000}[seed%4];const auto selected=(cells.ring_index+1)&511;
 for(unsigned i=0;i<512;++i){ProjectileRecordImage image;for(unsigned j=0;j<image.size();++j)image[j]=std::uint8_t(seed+i+j);put(image,0xf8,0);apply_projectile_record(state.projectiles[i],image);}
 std::array<std::uint32_t,6> position{};std::uint32_t random=seed+1;for(auto& p:position){random=random*1664525u+1013904223u;p=random;}
 const auto owner=seed%5?0x7cca80u+std::array<std::uint32_t,10>{0,255,256,259,1279,1280,2303,2304,3328,3839}[seed%10]*0x114u:0u;
 const auto speed=std::array<std::uint32_t,8>{0,1,63,64,0x7fffffff,0x80000000,0xffffffff,0x100000}[seed%8];
 std::vector<std::string> calls;unsigned call=0;MissionFragmentSpawnBindings b;b.sincos_4776c0=[&](auto angle,auto& sine,auto& cosine){
  std::ostringstream log;log<<'['<<angle<<','<<cells.ring_index<<",\""<<hex(snapshot_projectile_record(state.projectiles[selected]))<<"\"]";calls.push_back(log.str());++call;
  if(seed&1){position[0]+=call*11;position[1]^=call*22;position[2]-=call*33;position[3]+=call*0x12345;position[4]^=call*0x67890;cells.ring_index=call==1?77:0xffffffff;
   auto image=snapshot_projectile_record(state.projectiles[selected]);image[8]=std::uint8_t(0xa0+call);image[0x20]=std::uint8_t(0xb0+call);image[0x70]=std::uint8_t(0xc0+call);image[0x10c]=std::uint8_t(0xd0+call);put(image,0x10,0x12345678);apply_projectile_record(state.projectiles[selected],image);
  }sincos_q22(angle,sine,cosine);
 };
 MissionFragmentSpawn spawn(state,cells,b);const auto result=spawn.spawn(position,owner,speed);std::uint32_t checksum=2166136261u;
 for(const auto& p:state.projectiles)for(auto byte:snapshot_projectile_record(p))checksum=(checksum^byte)*16777619u;
 std::cout<<"{\"result\":"<<result<<",\"ring\":"<<cells.ring_index<<",\"record\":\""<<hex(snapshot_projectile_record(state.projectiles[selected]))<<"\",\"checksum\":"<<checksum<<",\"input\":[";
 for(unsigned i=0;i<6;++i){if(i)std::cout<<',';std::cout<<position[i];}std::cout<<"],\"calls\":[";for(std::size_t i=0;i<calls.size();++i){if(i)std::cout<<',';std::cout<<calls[i];}std::cout<<"]}\n";
}}
