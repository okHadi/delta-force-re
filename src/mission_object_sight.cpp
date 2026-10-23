#include "dfre/mission_object_sight.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t s;std::memcpy(&s,&n,4);return s;}
std::int32_t product(std::int32_t a,std::int32_t b,unsigned shift){return si(U(std::uint64_t(std::int64_t(a)*b)>>shift));}
std::int32_t position(MissionTickState& state,const AiActorRef& ref,unsigned axis){const auto* a=state.actor_at(ref);if(!a)throw MissionTickError("Sight endpoint is outside its current actor table");return si(a->position[axis]);}
struct Reader {
 MissionTickState& state;const MissionObjectSightBindings& bindings;
 std::int32_t coordinate(const AiActorRef& ref,unsigned axis){if(bindings.before_read)bindings.before_read(mission_actor_source_address(ref.group,ref.index)+0x24+axis*4);return position(state,ref,axis);}
 std::int32_t count(std::size_t g){if(bindings.before_read)bindings.before_read(g==1?0x8cf904:0x8cfa1c);if(bindings.current_count)return bindings.current_count(g);const auto n=state.tables.groups[g].size();if(n>std::size_t(std::numeric_limits<std::int32_t>::max()))throw MissionTickError("Sight actor count exceeds signed range");return std::int32_t(n);}
 std::int32_t water(){if(bindings.before_read)bindings.before_read(0xbd5248);return state.world.water_level;}
};
}
std::int32_t mission_object_sight(MissionTickState& state,const AiActorRef& source,const AiActorRef& target,const MissionObjectSightBindings& b){
 Reader read{state,b};std::array<std::int32_t,3> delta{};
 // Preserve the interleaved source loads, including source Z before target Z.
 const auto ex=read.coordinate(target,0),sx=read.coordinate(source,0);delta[0]=si(U(ex)-U(sx));
 const auto ey=read.coordinate(target,1),sy=read.coordinate(source,1),sz=read.coordinate(source,2),ez=read.coordinate(target,2);delta[1]=si(U(ey)-U(sy));delta[2]=si(U(ez)-U(sz));
 U sum=0;for(auto d:delta)sum+=U(product(d,d,22));
 const auto length=si(U(collision_sqrt(si(sum)))<<11);
 if(!length)return 1;
 if(length>=0x3200000)return 0;
 const auto reciprocal=std::int64_t(0x100000000LL)/length;
 if(reciprocal<std::numeric_limits<std::int32_t>::min() || reciprocal>std::numeric_limits<std::int32_t>::max())throw MissionTickError("Sight reciprocal exceeds the original signed divide range");
 WorldHitQuery query;for(unsigned i=0;i<3;++i)query.direction[i]=product(delta[i],std::int32_t(reciprocal),16);
 for(unsigned i=0;i<3;++i)query.origin[i]=read.coordinate(source,i);
 for(unsigned i=0;i<3;++i)query.end[i]=read.coordinate(target,i);
 query.length=length;query.skip={mission_actor_source_address(source.group,source.index),mission_actor_source_address(target.group,target.index)};
 for(unsigned i=0;i<3;++i){query.bounds[i*2]=std::min(query.origin[i],query.end[i]);query.bounds[i*2+1]=std::max(query.origin[i],query.end[i]);}
 if(!b.terrain)throw MissionTickError("Sight requires47E4B0");
 const auto terrain=b.terrain(source,query.direction,length);if(terrain!=1)return terrain;
 if(!b.model_list)throw MissionTickError("Sight requires42DB80");
 WorldModelHit hit;const auto first=b.model_list(1,read.count(1),query,hit);if(!first)return si(0u-U(hit.distance));
 U masked=U(first)-1;
 const auto second=b.model_list(0,read.count(0),query,hit);if(!second)return si(0u-U(hit.distance));masked+=U(second)-1;
 if(read.water()){
  const auto water=read.water();
  if(water>=read.coordinate(source,2) || water>=read.coordinate(target,2))++masked;
 }
 return si(masked+1);
}
MissionObjectSightBindings mission_object_sight_bindings(MissionTickState& state,const MissionGeometry& geometry,const AssetBytes& bounds,MissionGeometryMatrixSource matrices){
 MissionObjectSightBindings b;
 b.terrain=[&state,&bounds](const AiActorRef& ref,const auto& direction,std::int32_t length){if(!state.world.terrain)throw MissionTickError("Sight needs current terrain");std::array<std::int32_t,3> start{};for(unsigned axis=0;axis<3;++axis)start[axis]=position(state,ref,axis);const auto march=terrain_ray_march_detail(*state.world.terrain,bounds,CollisionSegment{start,direction,length});return march.hit?march.lambda:march.lambda==0x10000?1:march.lambda;};
 b.model_list=[&state,&geometry,matrices=std::move(matrices)](std::size_t group,std::int32_t count,const WorldHitQuery& query,WorldModelHit& hit){
  if(group!=0 && group!=1)throw MissionTickError("Sight model group is invalid");
  U masked=0;hit={};
  for(std::int32_t i=0;i<count;++i){const AiActorRef ref{std::uint8_t(group),std::size_t(i)};const auto identity=mission_actor_source_address(group,std::size_t(i));if(identity==query.skip[0]||identity==query.skip[1])continue;const auto image=snapshot_mission_actor(state,ref);const auto record=geometry.world_hit_record(identity,image,matrices);WorldModelHit row;const auto result=world_model_list_hit({record},query,row);if(!result){hit=row;hit.record=std::size_t(i);return std::int32_t(0);}masked+=U(result)-1;}
  hit.masked=masked;return si(masked+1);
 };
 return b;
}
}
