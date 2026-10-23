#include "dfre/mission_projectile_trails.hpp"
#include <algorithm>
#include <stdexcept>
namespace dfre {
MissionProjectileTrailBindings mission_projectile_trail_bindings(MissionProjectileTrailCells& c){
 const auto cell=[&c](std::uint32_t at)->std::uint32_t&{
  if(at==0x4f9148)return c.ring_4f9148;
  if(at<0x9e3098||at>=0x9f3098||(at-0x9e3098)%4)throw std::runtime_error("Projectile trail address exceeds its owner");
  const auto word=(at-0x9e3098)/4;return c.records_9e3098[word/8][word%8];
 };
 return {[cell](auto at){return cell(at);},[cell](auto at,auto value){cell(at)=value;}};
}
std::uint32_t mission_projectile_trail_record(const MissionProjectileTrailBindings& b,const std::array<std::uint32_t,7>& args){
 if(!b.read)throw std::runtime_error("Projectile trail reader is not bound");
 const auto index=(b.read(0x4f9148)+1)&0x7ffu;
 if(!b.write)throw std::runtime_error("Projectile trail writer is not bound");
 b.write(0x4f9148,index);const auto at=0x9e3098u+(index<<5);
 for(unsigned i=0;i<6;++i){const auto value=args[i];b.write(at+i*4,value);}
 b.write(at+24,0x1e);const auto color=args[6];b.write(at+28,color);
 return b.read(0x4f9148);
}
void mission_projectile_trail_clear(MissionProjectileTrailCells& c){std::fill_n(c.records_9e3098.begin(),512,std::array<std::uint32_t,8>{});}
} // namespace dfre
