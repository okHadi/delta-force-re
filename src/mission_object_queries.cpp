#include "dfre/mission_object_queries.hpp"
#include <cstring>
namespace dfre { namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t s;std::memcpy(&s,&n,4);return s;}
struct Reader {
 const MissionTickState& state;const MissionObjectTargetBindings& bindings;
 U word(const AiActorRef& ref,unsigned offset,unsigned width=4)const{
  AiActorImage image;
  if(ref.group==3 && ref.index>=state.tables.groups[3].size()){
   if(!bindings.physical_slot)throw MissionTickError("Object target needs its current physical actor slot");
   image=bindings.physical_slot(ref.index);
  }else image=snapshot_mission_actor(state,ref);
  U n=0;for(unsigned i=0;i<width;++i)n|=U(image[offset+i])<<(i*8);return n;
 }
};
U product(U a,U b){return U(std::uint64_t(std::int64_t(si(a))*si(b))>>16);}
std::int32_t absolute(U n){return si((n^(0u-(n>>31)))+(n>>31));}
U folded(U a){auto n=a>>24;return n>0x7f?0x100-n:n;}
AiPersonState& person(MissionTickState& state,const AiActorRef& ref){const auto* actor=state.actor_at(ref);if(!actor || !actor->person_slot || *actor->person_slot>=state.people.size())throw MissionTickError("Object target needs a current shooter person");return state.people[*actor->person_slot];}
}
MissionObjectTarget mission_object_target(MissionTickState& state,const AiActorRef& shooter,const MissionObjectTargetBindings& b){
 if(!state.actor_at(shooter))throw MissionTickError("Object target shooter is outside its current table");
 Reader read{state,b};
 MissionObjectTarget result;
 for(std::size_t i=0;i<256;++i){
  const AiActorRef target{3,i};
  if(!read.word(target,0))continue;
  const auto team=read.word(target,0x70,2);
  const auto shooter_team=read.word(shooter,0x70,2);
  if(team==shooter_team || !team)continue;
  const auto candidate_flags=read.word(target,0x6c,1);
  if(candidate_flags&4)continue;
  if(state.config.local_actor==i && (state.globals.flags_67dbc9&8))continue;
  const auto target_x=read.word(target,0x24);
  const auto shooter_x=read.word(shooter,0x24);
  const auto target_y=read.word(target,0x28);
  const auto shooter_y=read.word(shooter,0x28);
  const auto target_z=read.word(target,0x2c);
  const auto shooter_z=read.word(shooter,0x2c);
  const auto dx=target_x-shooter_x,dy=target_y-shooter_y,dz=target_z-shooter_z;
  const auto yaw_angle=query_angle_477770(si(dx),si(dy));
  const auto shooter_yaw=read.word(shooter,0x30);
  const auto yaw=folded(yaw_angle-shooter_yaw);if(yaw>16)continue;
  const U x=U(si(dx)>>16),y=U(si(dy)>>16);const auto plane=query_isqrt_477620(x*x+y*y)<<16;
  const auto pitch_angle=query_angle_477770(si(plane),si(dz));
  const auto shooter_pitch=read.word(shooter,0x34);
  if(folded(pitch_angle-shooter_pitch)>16)continue;
  const auto distance=si(behavior_octagon3(absolute(dx),absolute(dy),absolute(dz)))>>16;
  const U inverse=distance?U(std::int64_t(0x640000)/distance):0x640000;
  if(distance>=1000)continue;
  if(!b.sight_line)throw MissionTickError("Object target requires42E2B0");
  const auto visible=b.sight_line(shooter,target);if(visible<=0 || visible>=3)continue;
  if(!b.frame_weight_10)throw MissionTickError("Object target requires the current frame weight");
  const auto frame=b.frame_weight_10(target);
  const U scale=(read.word(target,0x6d,1)&0x40)?0x60000:0x10000;
  const auto yaw_score=product(inverse,(16-yaw)*0x1000);
  const auto frame_score=product(yaw_score,frame);
  auto score=product(frame_score,scale);
  if(!(read.word(target,0x6c,1)&1))score*=2;
  if(si(score)>si(result.score)){result.source_actor=mission_actor_source_address(3,i);result.score=score;}
 }
 const auto person_scale=person(state,shooter).field_74;
 result.score=product(result.score,person_scale);
 const auto person_mode=person(state,shooter).field_9c;
 if(!person_mode)result.score=U(si(result.score)>>2);
 return result;
}
void mission_object_region_mark(EventStage& events,U a,U b){events.set_flag_42edd0(a,b);}
}
