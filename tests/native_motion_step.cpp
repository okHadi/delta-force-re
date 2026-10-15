#include "dfre/motion_step.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}}
// Expected values were read from the original Df.exe replay; see
// tools/check_motion_step_original.py and .local/opencode/motion-step-work/original-values.txt.
int main(){try{
 dfre::FrameStepInput step;
 step.frame_speed=100;step.definition_side=7;step.definition_vertical=11;step.definition_up=13;
 step.base={0,4194303};step.pitch_pair={0,4194303};
 const auto walk_2d=dfre::frame_step(step);
 require(walk_2d.x==49&&walk_2d.y==0&&walk_2d.z==20,"Original 2D walk step");
 step.walk_3d=true;
 const auto walk_3d=dfre::frame_step(step);
 require(walk_3d.x==98&&walk_3d.y==0&&walk_3d.z==0,"Original 3D walk step");
 step.walk_3d=false;step.frame_vertical=true;
 const auto vehicle=dfre::frame_step(step);
 require(vehicle.x==-12&&vehicle.y==-6&&vehicle.z==10,"Original vehicle step");
 step.frame_vertical=false;step.frame_speed=0x100000;step.base={4194303,0};
 const auto yaw90=dfre::frame_step(step);
 require(yaw90.x==0&&yaw90.y==524287&&yaw90.z==20,"Original 2D step at a quarter turn");
 step.walk_3d=true;step.base={0,4194303};step.pitch_pair={4194303,0};step.position_z=0x10000;step.water_level=0x20000;
 const auto pitch90=dfre::frame_step(step);
 require(pitch90.x==0&&pitch90.y==0&&pitch90.z==524287,"Original 3D step at a quarter pitch");
 step.walk_3d=false;step.pitch_pair={0,4194303};
 step.frame_speed=100;step.base={0,4194303};step.position_z=0;step.water_level=0;
 dfre::StepVector base{49,0,20};
 const auto wall=dfre::terrain_slope_response(base,dfre::actor_active,0xb400,0xc000,{0,4194303});
 require(wall.applied&&!wall.slope_blocked&&wall.wall&&wall.step==base,"Original slope wall bit");
 const auto blocked=dfre::terrain_slope_response(base,dfre::actor_active,0x20000,0,{0,4194303});
 require(blocked.applied&&blocked.slope_blocked&&!blocked.wall&&blocked.step==base,"Original slope block");
 const auto pushback=dfre::terrain_slope_response(base,dfre::actor_active|dfre::actor_slope_blocked,0x20000,0x20000,{0,4194303});
 require(pushback.applied&&pushback.slope_blocked&&pushback.step.x==-13106&&pushback.step.y==-13106&&pushback.step.z==20,"Original slope pushback");
 const auto gated=dfre::terrain_slope_response(base,dfre::actor_active|dfre::actor_in_water,0x20000,0x20000,{0,4194303});
 require(!gated.applied&&!gated.slope_blocked&&gated.step==base,"Slope response stays off in water");
 dfre::CachedStep cache{0x111,0x222,0x333};
 require(dfre::cached_step(base,true,cache)==dfre::StepVector({0x111,0x222,20}),"Airborne step uses the cached ground step");
 require(cache.x==0x111&&cache.z==0x333,"Airborne step leaves the cache alone");
 require(dfre::cached_step(base,false,cache)==base&&cache==dfre::CachedStep{49,0,20},"Grounded step fills the cache");
 auto ground=[&](std::int32_t z,std::int32_t vertical,std::uint32_t flags,std::int32_t height,std::int32_t water,bool full,std::int32_t clearance=1){
  dfre::MotionActor actor;actor.position={0x4000,0x8000,z};actor.vertical=vertical;actor.flags=flags;
  dfre::GroundQuery query;query.height=height;query.water_level=water;query.clearance=clearance;query.full_pass=full;
  dfre::ground_update(actor,nullptr,query,{2047,0,20},true);
  return actor;
 };
 const auto snapped=ground(0x5000,7,0,0x30000,0,true);
 require(snapped.position[2]==196607&&snapped.vertical==0&&snapped.flags==0,"Original ground snap");
 const auto walked=ground(0x5000,7,0,0x4000,0,true);
 require(walked.position[2]==20507&&walked.vertical==-171&&walked.flags==0,"Original grounded gravity");
 const auto landed=ground(0x5000,7,dfre::actor_airborne,0x4000,0,true);
 require(landed.position[2]==20507&&landed.vertical==-171&&landed.flags==dfre::actor_airborne,"Original airborne gravity");
 const auto splashed=ground(0x5000,7,dfre::actor_airborne,0x1000,0x20000,true);
 require(splashed.vertical==7&&splashed.flags==dfre::actor_in_water,"Original water entry");
 require(snapped.position[0]==18431&&snapped.position[1]==32768,"Original ground step is applied");
 // Boundary cases read from the original replay with z, height and clearance at the 32-bit limits.
 const auto water_snap=ground(std::numeric_limits<std::int32_t>::min(),100,dfre::actor_airborne|dfre::actor_in_water,0,0x10000,false,-2);
 require(water_snap.position[2]==2&&water_snap.vertical==0&&water_snap.flags==dfre::actor_in_water,
     "Airborne snap at the signed limit keeps the water bit");
 const auto snap_min=ground(std::numeric_limits<std::int32_t>::min(),100,0,0x7fffffff,0,true,-2);
 require(snap_min.position[2]==-2147483528&&snap_min.vertical==-78&&snap_min.flags==dfre::actor_in_water,
     "Grounded water gravity at the signed limit");
 const auto push_min=dfre::terrain_slope_response({49,0,20},dfre::actor_active|dfre::actor_slope_blocked,std::numeric_limits<std::int32_t>::min(),0x7fffffff,{0,4194303});
 require(push_min.step.x==214728704&&push_min.step.y==-214728703&&push_min.step.z==20,"Slope pushback wraps at the limits");
 dfre::MotionActor object_actor;object_actor.position={std::numeric_limits<std::int32_t>::min(),0x7fffffff,0x5000};object_actor.vertical=7;
 object_actor.flags=7;object_actor.on_ground_object=true;
 dfre::GroundObject object;object.rotation=0x40000000;object.offset_x=0x7fffffff;object.offset_y=0x7fffffff;
 dfre::GroundQuery object_query;object_query.height=0x4000;object_query.clearance=0xfffffff;object_query.full_pass=true;
 dfre::ground_update(object_actor,&object,object_query,{0,0,20},true);
 require(object_actor.position[0]==2&&object_actor.position[1]==-1&&object_actor.yaw==0x40000000,
     "Ground object rotation wraps at the limits");
 dfre::MovementActorState state;
 const auto animate=[&](std::uint16_t control,std::uint32_t flags,std::uint8_t slot,bool steady=false,std::int32_t stance=0){
  state=dfre::MovementActorState{};state.control=control;state.flags=flags;state.z=0x40000000;state.vertical=5;
  state.stance=stance;
  return dfre::movement_animation(state,slot,steady,0x1111);
 };
 require(animate(0,0,0)==1&&state.z==0x40000000,"Original idle animation");
 require(animate(0,0,0x40)==7,"Original idle animation with slot bit 0x40");
 require(animate(0,0,0x0c)==3,"Original idle animation with slot bit 8");
 require(animate(0,0,0x0c,false,1)==12,"Original idle animation at the raised stance");
 require(animate(0x02,0,0)==60&&state.vertical==5,"Original forward animation without NoRun");
 require(animate(0x03,0,0)==20,"Original forward animation with NoRun");
 require(animate(0x04,0,0)==64,"Original back animation");
 require(animate(0x08,0,0)==62,"Original left animation");
 require(animate(0x0a,0,0)==61,"Original diagonal animation");
 require(animate(0x06,0,0)==1,"Opposite movement bits fall back to idle");
 require(animate(0,dfre::actor_in_water,0)==140,"Original water animation");
 require(animate(0,dfre::actor_airborne,0)==130,"Original airborne animation");
 require(animate(0,dfre::actor_slope_blocked,0)==130,"Original slope animation");
 state=dfre::MovementActorState{};state.control=0x400;state.z=0x40000000;state.vertical=5;state.pulse_jump=1;
 require(dfre::movement_animation(state,0,false,0x1111)==1&&state.vertical==0x1111&&state.z==0x40001000&&state.pulse_jump==0,
     "Original jump pulse applies the impulse");
 require(animate(0,0,0,true)==4,"Original steady animation");
 state=dfre::MovementActorState{};state.control=0;state.z=0x40000000;state.vertical=5;state.pulse_prone=1;
 require(dfre::movement_animation(state,0,false,0x1111)==110&&state.prone==1&&state.pulse_prone==0,"Original prone pulse");
 state=dfre::MovementActorState{};state.control=0;state.z=0x40000000;state.vertical=5;state.pulse_crouch=1;
 require(dfre::movement_animation(state,0,false,0x1111)==100&&state.mode==1&&state.pulse_crouch==0,"Original crouch pulse");
 state=dfre::MovementActorState{};state.control=0;state.z=0x40000000;state.vertical=5;state.pulse_stand=1;
 require(dfre::movement_animation(state,0,false,0x1111)==1&&state.pulse_stand==0,"Original stand pulse");
 std::cout<<"motion step checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
