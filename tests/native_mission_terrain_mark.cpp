#include "dfre/mission_terrain_mark.hpp"
#include "dfre/mission_behavior_zones.hpp"
#include <cassert>
#include <cstring>
using namespace dfre;
namespace {unsigned word(const std::uint8_t* p,unsigned at){unsigned n=0;for(unsigned i=0;i<4;++i)n|=unsigned(p[at+i])<<(8*i);return n;}}
int main(){MissionTickState state;state.globals.mission_67f528=1;state.behavior.rng=1;MissionBehaviorZones zones;auto zone=zones.bindings(state.globals.mission_67f528);mission_behavior_zones_reset(zone);MissionTerrainMarkBindings b;unsigned type=0,index=3,table=0x1210000,sounds=0,inserts=0;std::vector<unsigned> reads;
 b.read=[&](unsigned at,unsigned width){reads.push_back(at);if(at==0xbd52ac){assert(width==4);return 0x1300000u;}if(at==0x13fffff){assert(width==1);return 7u;}if(at==0xbd533b){assert(width==1);return type;}if(at==0x8e44c4)return index;if(at==0xc23810)return table;if(at==0x1211000+3*0x13c+0x6c)return 0x71000001u;if(at==0x1211000+3*0x13c+0x84)return 0x453810u;if(at==0x1211000+3*0x13c+0x90)return 0x4563a0u;throw MissionTickError("Unowned current terrain-mark read");};
 Fixed3 position{-1,0x3ff0000,3};mission_terrain_mark(state,position,b);assert(reads.size()==3&&state.behavior.rng==1&&zones.count==0);
 type=1;bool failed=false;try{mission_terrain_mark(state,position,b);}catch(const MissionTickError&){failed=true;}assert(failed&&state.behavior.rng==1&&zones.count==0);
 // The selected item remains captured while the sound callback changes its cell.
 b.call=[&](unsigned site,const auto& args,std::uint8_t* image,std::size_t length){if(site==0x43e7d0){assert(args[0]==0x3c&&args[1]==0xffffffff&&args[2]==0x3ff0000&&args[3]==3);++sounds;index=99;table=0x1211000;}else if(site==0x45fcf0){assert(args[3]==5&&args[4]==0&&args[5]==0);mission_behavior_zone_add(zone,args[0],args[1],args[2],args[3],args[4],args[5]);}else{assert(site==0x45d970&&length==0x114&&args[1]==1&&args[0]==0);assert(word(image,0)==3&&word(image,4)==0x1211000+3*0x13c&&word(image,0x10)==0x71000001);assert(word(image,0x24)==0xffffffff&&word(image,0x28)==0x3ff0000&&word(image,0x2c)==3);assert(word(image,0x6c)==0&&word(image,0x110)==0x453810&&word(image,0x10c)==0x4563a0);for(unsigned at:{8u,12u,20u,0x38u,0x70u,0x100u})assert(word(image,at)==0);++inserts;throw MissionTickError("Real insertion owner is required");}};
 failed=false;try{mission_terrain_mark(state,position,b);}catch(const MissionTickError&){failed=true;}assert(failed&&sounds==1&&inserts==1&&zones.count==1&&state.behavior.rng!=1);assert(zones.records[0][0]==0&&zones.records[0][1]==0&&zones.records[0][2]==0xfffffffau&&zones.records[0][3]==4);assert(state.tables.groups[0].empty()&&state.tables.groups[1].empty()&&state.tables.groups[3].empty());
}
