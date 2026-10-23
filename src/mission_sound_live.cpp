#include "dfre/mission_sound_live.hpp"
#include "dfre/mission_live.hpp"
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;
constexpr U stride=0x114,projectile_base=0x9f3098;
constexpr std::array<U,4> actor_bases{0x7dde80,0x867e80,0x8ace80,0x7cca80};
constexpr std::array<std::size_t,4> actor_caps{1024,1024,512,256};
struct Record {U base;std::size_t slot;std::uint8_t group;bool projectile;};
void extent(U offset,U width){
 if(width!=1&&width!=2&&width!=4)throw MissionTickError("Sound record width must be1,2 or4");
 if(std::uint64_t(offset)+width>stride)throw MissionTickError("Sound access crosses its record");
}
void capacities(const MissionTickState& state){
 for(std::size_t g=0;g<4;++g)if(state.tables.groups[g].size()>actor_caps[g])throw MissionTickError("Sound actor count exceeds its physical pool");
 if(state.projectiles.size()>512)throw MissionTickError("Sound projectile count exceeds its physical pool");
}
std::optional<Record> locate(const MissionTickState& state,U at,U width){
 extent(0,width);capacities(state);const auto end=std::uint64_t(at)+width;
 if(end>0x100000000ULL)throw MissionTickError("Sound address range wraps");
 const auto pool=[&](U base,std::size_t capacity,std::size_t count,std::uint8_t group,bool projectile)->std::optional<Record>{
  const auto limit=std::uint64_t(base)+capacity*stride;
  if(std::uint64_t(at)>=limit||end<=base)return std::nullopt;
  if(at<base)throw MissionTickError("Sound access enters a physical pool from outside");
  const U delta=at-base;extent(delta%stride,width);const auto slot=std::size_t(delta/stride);
  if(slot>=count)throw MissionTickError("Sound record has no current owner");
  return Record{base+U(slot)*stride,slot,group,projectile};
 };
 for(std::uint8_t g=0;g<4;++g)if(auto r=pool(actor_bases[g],actor_caps[g],state.tables.groups[g].size(),g,false))return r;
 if(at<0x8cf680u&&end>0x7cca80u)throw MissionTickError("Sound actor reservation has no claimed physical pool");
 return pool(projectile_base,512,state.projectiles.size(),0,true);
}
Record exact(const MissionTickState& state,U base,U offset,U width){
 extent(offset,width);const auto r=locate(state,base,1);
 if(!r||r->base!=base)throw MissionTickError("Sound record base needs an aligned current owner");
 return *r;
}
U read(MissionTickState& state,U base,U offset,U width){
 const auto r=exact(state,base,offset,width);U value=0;
 const auto bytes=r.projectile?snapshot_projectile_record(state.projectiles[r.slot]):snapshot_mission_actor(state,{r.group,r.slot});
 for(U i=0;i<width;++i)value|=U(bytes[offset+i])<<(8*i);return value;
}
void store(MissionTickState& state,U base,U offset,U width,U value){
 const auto r=exact(state,base,offset,width);
 auto bytes=r.projectile?snapshot_projectile_record(state.projectiles[r.slot]):snapshot_mission_actor(state,{r.group,r.slot});
 for(U i=0;i<width;++i)bytes[offset+i]=std::uint8_t(value>>(8*i));
 if(r.projectile)apply_projectile_record(state.projectiles[r.slot],bytes);else apply_mission_actor_image(state,{r.group,r.slot},bytes);
}
}
MissionSoundRuntimeBindings mission_sound_live_bindings(MissionTickState& state,std::function<U()> script_key){
 MissionSoundRuntimeBindings b;
 b.record_base=[&state](U at,U width)->std::optional<U>{const auto r=locate(state,at,width);return r?std::optional<U>(r->base):std::nullopt;};
 b.record_read=[&state](U base,U offset,U width){return read(state,base,offset,width);};
 b.record_store=[&state](U base,U offset,U width,U value){store(state,base,offset,width,value);};
 b.actor_word=[&state](U base,U offset){return read(state,base,offset,4);};
 b.script_key=std::move(script_key);return b;
}
} // namespace dfre
