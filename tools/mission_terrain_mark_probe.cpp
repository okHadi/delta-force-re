#include "dfre/mission_terrain_mark.hpp"
#include <iostream>
#include <cstring>
#include <sstream>
namespace {
std::string hex(const std::uint8_t* p,std::size_t size){const char* d="0123456789abcdef";std::string s;for(std::size_t i=0;i<size;++i){s+=d[p[i]>>4];s+=d[p[i]&15];}return s;}
}
int main(){using namespace dfre;std::uint32_t type,x,y,z,seed,mutation;while(std::cin>>type>>x>>y>>z>>seed>>mutation){MissionTickState state;state.behavior.rng=seed;std::uint32_t index=3,table=0x1210000,reads=0,calls=0;std::ostringstream trace;MissionTerrainMarkBindings b;
 b.read=[&](auto at,auto width){trace<<'R'<<at<<','<<width<<';';++reads;if((mutation&64)&&reads==1)throw MissionTickError("Source failure");if(at==0xbd52ac)return 0x1220000u;if(width==1&&at>=0x1220000&&at<0x1320000)return 7u;if(at==0xbd5334+7){if(mutation&1)index=5;return type;}if(at>=0x8e44c4&&at<=0x8e44e4)return index;if(at==0xc23810)return table;if(at%4==0&&(at>=table)){if(mutation&8)state.behavior.rng^=0xabcdef01;return at^0x55667788u;}throw MissionTickError("Unowned fixture read");};
 b.call=[&](auto site,const auto& args,std::uint8_t* image,std::size_t length){trace<<'C'<<site;for(auto n:args)trace<<','<<n;trace<<':'<<hex(image,length)<<';';++calls;if(site==0x43e7d0){if(mutation&2)index=9;if(mutation&4)table=0x1211000;if(mutation&16)state.behavior.rng=0xfedcba98;}if((mutation&32)&&calls==2)throw MissionTickError("Zone failure");if((mutation&128)&&site==0x45d970)throw MissionTickError("Insertion failure");};
 Fixed3 p;std::memcpy(&p.x,&x,4);std::memcpy(&p.y,&y,4);std::memcpy(&p.z,&z,4);bool failed=false;try{mission_terrain_mark(state,p,b);}catch(const std::exception&){failed=true;}std::cout<<failed<<' '<<state.behavior.rng<<' '<<index<<' '<<table<<' '<<trace.str()<<'\n';
}}
