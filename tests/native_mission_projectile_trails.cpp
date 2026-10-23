#include "dfre/mission_projectile_trails.hpp"
#include <cassert>
#include <stdexcept>
int main(){using namespace dfre;MissionProjectileTrailCells c;auto b=mission_projectile_trail_bindings(c);std::array<std::uint32_t,7> args{1,2,3,4,5,6,7};assert(mission_projectile_trail_record(b,args)==1);assert((c.records_9e3098[1]==std::array<std::uint32_t,8>{1,2,3,4,5,6,30,7}));c.ring_4f9148=0xffffffff;assert(mission_projectile_trail_record(b,args)==0);assert(c.records_9e3098[0][7]==7);
 auto changes=b;changes.write=[&](auto at,auto value){if(at==0x9e3098+32){args[1]=99;c.ring_4f9148=0xaabbccdd;}b.write(at,value);};assert(mission_projectile_trail_record(changes,args)==0xaabbccdd);assert(c.records_9e3098[1][1]==99);
 // A later argument reads a destination word changed by an earlier store.
 c.ring_4f9148=0;auto alias=b;alias.write=[&](auto at,auto value){b.write(at,value);if(at==0x9e3098+32+20)args[6]=b.read(at);};assert(mission_projectile_trail_record(alias,args)==1);assert(c.records_9e3098[1][7]==args[5]);
 c.records_9e3098[511][7]=8;c.records_9e3098[512][0]=9;const auto counter=c.ring_4f9148;mission_projectile_trail_clear(c);assert(c.records_9e3098[511][7]==0&&c.records_9e3098[512][0]==9&&c.ring_4f9148==counter);
 bool failed=false;try{b.read(0x9f3098);}catch(const std::runtime_error&){failed=true;}assert(failed);failed=false;try{b.write(0x9e3099,1);}catch(const std::runtime_error&){failed=true;}assert(failed);failed=false;try{mission_projectile_trail_record({},args);}catch(const std::runtime_error&){failed=true;}assert(failed);
}
