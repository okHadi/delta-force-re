#include "dfre/mission_event_queries.hpp"
#include <cstring>
#include <limits>
namespace dfre { namespace {
constexpr std::array<std::uint32_t,4> bases{0x7dde80,0x867e80,0x8ace80,0x7cca80};
std::int32_t signed_bits(std::uint32_t n){std::int32_t s;std::memcpy(&s,&n,4);return s;}
std::int32_t side(std::uint32_t n){const auto high=n>>16;return high<0x8000?std::int32_t(high):std::int32_t(high)-0x10000;}
struct Reader {
 MissionTickState& state;const MissionEventQueryBindings& bindings;
 std::int32_t count(std::size_t group){
  if(bindings.before_read)bindings.before_read(group==3?0x8cfa18:0x8cfa1c);
  if(bindings.current_count)return bindings.current_count(group);
  const auto n=state.tables.groups[group].size();
  if(n>std::size_t(std::numeric_limits<std::int32_t>::max()))throw MissionTickError("Event query count exceeds signed range");
  return std::int32_t(n);
 }
 std::uint32_t word(std::uint32_t address){
  if(bindings.before_read)bindings.before_read(address);
  for(std::size_t g=0;g<4;++g){
   if(address<bases[g])continue;
   const auto delta=address-bases[g];const auto slot=delta/0x114,offset=delta%0x114;
   if(slot>=state.tables.groups[g].size() || offset>0x110)continue;
   const auto image=snapshot_mission_actor(state,{std::uint8_t(g),slot});
   std::uint32_t value;std::memcpy(&value,image.data()+offset,4);return value;
  }
  throw MissionTickError("Event query reached an unbound actor address");
 }
};
}
std::uint32_t mission_event_rect(MissionTickState& state,const MissionEventRect& p,const MissionEventQueryBindings& bindings){
 const auto side_id=p.side,min_x=p.min_x,min_y=p.min_y,max_x=p.max_x,max_y=p.max_y;
 if(!side_id || min_x==max_x || min_y==max_y)return 0;
 Reader read{state,bindings};
 for(const auto g:{3u,0u}){
  if(read.count(g)<=0)continue;
  std::uint32_t index=0,address=bases[g];
  for(;;){
   if(side(read.word(address+0x70))==side_id){
    const auto x=signed_bits(read.word(address+0x24));
    if(x>=min_x && x<=max_x){const auto y=signed_bits(read.word(address+0x28));if(y>=min_y && y<=max_y)return 1;}
   }
   const auto bound=read.count(g);++index;address+=0x114;
   if(signed_bits(index)>=bound)break;
  }
 }
 return 0;
}
std::uint32_t mission_event_linked_sides(MissionTickState& state,std::int32_t first,std::int32_t second,const MissionEventQueryBindings& bindings){
 if(!first || !second)return 0;
 Reader read{state,bindings};if(read.count(3)<=0)return 0;
 const auto bound=read.count(3);std::uint32_t index=0,address=bases[3];
 do{
  if(read.word(address) && read.word(address+0x90) && side(read.word(address+0x70))==first){
   const auto link=read.word(address+0x90);
   if(side(read.word(link+0x70))==second)return 1;
  }
  ++index;address+=0x114;
 }while(signed_bits(index)<bound);
 return 0;
}
}
