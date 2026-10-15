#include "dfre/movement.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {void require(bool yes,const char* message){if(!yes)throw std::runtime_error(message);}}
int main(){try{
 dfre::PlayerPose pose;pose.angles={0x80000000,0x10000000,0x1234};
 const auto left=dfre::apply_look_step(pose,0,dfre::control_turn_left);
 require(left.angles[0]==0x80000000+2*0xffffff&&left.angles[1]==0x10000000,"Running turn left step");
 require(dfre::apply_look_step(pose,dfre::control_no_run,dfre::control_turn_left).angles[0]==0x80000000+0xffffff,"NoRun halves the step");
 require(dfre::apply_look_step(pose,0,dfre::control_turn_right).angles[0]==0x80000000-2*0xffffff,"Turn right subtracts");
 dfre::PlayerPose level;level.angles={0,0,0};
 require(dfre::apply_look_step(level,dfre::control_look_up,0).angles[1]==0x1fffffe,"Look up adds pitch");
 require(dfre::apply_look_step(level,dfre::control_look_down,0).angles[1]==0xfe000002,"Look down stays inside the wrapped range");
 require(dfre::apply_look_step(level,dfre::control_look_up|dfre::control_look_down,0).angles[1]==0,"Up and down in one tick cancel");
 require(dfre::clamp_pitch(0x40000000)==0x40000000&&dfre::clamp_pitch(0x40000001)==0x40000000,"Upper pitch limit");
 require(dfre::clamp_pitch(0x80000000)==0x40000000&&dfre::clamp_pitch(0x80000001)==0xd0000000,"Middle pitch range moves to the lower limit");
 require(dfre::clamp_pitch(0xd0000000)==0xd0000000&&dfre::clamp_pitch(0xcfffffff)==0xd0000000,"Lower pitch limit");
 require(dfre::clamp_pitch(0xfe000002)==0xfe000002&&dfre::clamp_pitch(0xffffffff)==0xffffffff&&dfre::clamp_pitch(0)==0,"Wrapped pitch range kept");
 require(dfre::apply_look_step(pose,0,dfre::control_turn_left|dfre::control_turn_right).angles[0]==0x80000000,"Opposite turn bits cancel");
 const dfre::PlayerPose acted=dfre::apply_look_action(dfre::action_turn_right_abs,-3,pose);
 require(acted.angles[0]==0x80000000+3&&acted.angles[1]==pose.angles[1]&&acted.angles[2]==pose.angles[2],"Turn right abs adds the negated value");
 require(dfre::apply_look_action(dfre::action_look_up_abs,5,pose).angles[1]==0x10000005,"Look up abs adds");
 require(dfre::apply_look_action(dfre::action_look_dn_abs,5,pose).angles[1]==0x10000000-5,"Look down abs subtracts");
 require(dfre::apply_look_action(dfre::action_turn_left_abs,5,pose).angles[0]==0x80000005,"Turn left abs adds");
 require(dfre::apply_look_action(0x9f,5,pose).angles==pose.angles,"Unknown action keeps the pose");
 dfre::PlayerPose wrap;wrap.angles={0xffffffff,0,0};
 require(dfre::apply_look_action(dfre::action_turn_left_abs,1,wrap).angles[0]==0,"Angle wrap is explicit");
 dfre::ActorMotion actor;actor.position={1,2,3};actor.vertical_velocity=4;
 dfre::apply_step(actor,1,2,3);
 require(actor.position==std::array<std::int32_t,3>{2,4,10}&&actor.vertical_velocity==4,"Step adds x, y and velocity+dz to z");
 dfre::ActorMotion jump;const auto max=std::numeric_limits<std::int32_t>::max();
 jump.position={max,max,0};jump.vertical_velocity=-1;dfre::apply_step(jump,1,1,-1);
 require(std::uint32_t(jump.position[0])==0x80000000u&&std::uint32_t(jump.position[2])==0xfffffffeu,"Step and vertical wrap");
 require(dfre::move_octant(0)==0&&dfre::move_octant(dfre::control_forward)==0&&dfre::move_octant(dfre::control_back)==4,"Forward is octant 0, back is 4");
 require(dfre::move_octant(dfre::control_slide_left)==2&&dfre::move_octant(dfre::control_slide_right)==6,"Left is 2, right is 6");
 require(dfre::move_octant(dfre::control_forward|dfre::control_slide_left)==1&&dfre::move_octant(dfre::control_back|dfre::control_slide_left)==3,"Diagonal octants");
 require(dfre::move_octant(dfre::control_forward|dfre::control_slide_right)==7&&dfre::move_octant(dfre::control_back|dfre::control_slide_right)==5,"Other diagonal octants");
 require(dfre::move_octant(dfre::control_forward|dfre::control_back)==0&&dfre::move_octant(dfre::control_slide_left|dfre::control_slide_right)==0,"Conflicting bits clear the octant");
 require(dfre::move_octant(0x1e)==0&&dfre::move_octant(dfre::control_forward|dfre::control_back|dfre::control_slide_left)==2,"Observed jump table entries");
 require(dfre::move_anim_selector(0,false)==0&&dfre::move_anim_selector(dfre::control_forward,false)==3,"Selector 3 while moving");
 require(dfre::move_anim_selector(dfre::control_forward,true)==2&&dfre::move_anim_selector(dfre::control_forward|dfre::control_no_run,false)==2,"Selector 2 with NoRun or the actor flag");
 require(dfre::move_anim_selector(dfre::control_forward|dfre::control_back,false)==0,"Selector clears on conflicts");
 const auto mouse=dfre::mouse_look(1,1,128,true,false);
 require(mouse.turn==0x200000&&mouse.look==0x200000,"Mouse scale at sensitivity 128");
 require(dfre::mouse_look(1,1,128,true,true).look==0x80000,"Steady divides the scale by four");
 require(dfre::mouse_look(1,1,128,false,false).look==-0x200000,"Flip negates the look axis only");
 require(dfre::mouse_look(0,0,255,false,false).turn==0&&dfre::mouse_look(1,1,0,true,false).look==0,"Zero axis and zero sensitivity");
 std::cout<<"movement checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
