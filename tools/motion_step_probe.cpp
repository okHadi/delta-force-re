#include "dfre/motion_step.hpp"
#include <iostream>
#include <string>

// Line probes for tools/check_motion_step_original.py. Every mode reads one
// case per line and prints one result per line.
int main(int argc,char** argv){
    const std::string mode=argc>1?argv[1]:"";
    if(mode=="sincos"){
        std::uint32_t angle;
        while(std::cin>>angle){const auto value=dfre::frame_base_sincos(angle,0);std::cout<<value[0]<<' '<<value[1]<<'\n';}
        return 0;
    }
    if(mode=="frame_step"){
        std::uint32_t vertical,three_d,yaw,angle,pitch;
        std::int32_t speed,side,definition_vertical,up,z,water;
        while(std::cin>>vertical>>three_d>>yaw>>angle>>pitch>>speed>>side>>definition_vertical>>up>>z>>water){
            dfre::FrameStepInput input;
            input.frame_vertical=vertical!=0;input.walk_3d=three_d!=0;input.frame_angle=angle;
            input.frame_speed=speed;input.definition_side=side;input.definition_vertical=definition_vertical;
            input.definition_up=up;input.position_z=z;input.water_level=water;
            input.base=dfre::frame_base_sincos(yaw,angle);input.pitch_pair=dfre::frame_pitch_sincos(pitch);
            const auto step=dfre::frame_step(input);
            std::cout<<step.x<<' '<<step.y<<' '<<step.z<<'\n';
        }
        return 0;
    }
    if(mode=="frame_step_math"){
        std::uint32_t vertical,three_d;
        std::int32_t sin_base,cos_base,sin_pitch,cos_pitch,speed,side,definition_vertical,up,z,water;
        while(std::cin>>vertical>>three_d>>sin_base>>cos_base>>sin_pitch>>cos_pitch>>speed>>side>>definition_vertical>>up>>z>>water){
            dfre::FrameStepInput input;
            input.frame_vertical=vertical!=0;input.walk_3d=three_d!=0;input.frame_speed=speed;
            input.definition_side=side;input.definition_vertical=definition_vertical;input.definition_up=up;
            input.position_z=z;input.water_level=water;input.base={sin_base,cos_base};input.pitch_pair={sin_pitch,cos_pitch};
            const auto step=dfre::frame_step(input);
            std::cout<<step.x<<' '<<step.y<<' '<<step.z<<'\n';
        }
        return 0;
    }
    if(mode=="slope"){
        std::int32_t x,y,z,sin_base,cos_base,slope_x,slope_y;
        std::uint32_t flags;
        while(std::cin>>x>>y>>z>>flags>>sin_base>>cos_base>>slope_x>>slope_y){
            dfre::StepVector step;step.x=x;step.y=y;step.z=z;
            const auto result=dfre::terrain_slope_response(step,flags,slope_x,slope_y,{sin_base,cos_base});
            std::cout<<result.step.x<<' '<<result.step.y<<' '<<result.step.z<<' '<<int(result.slope_blocked)<<' '
                <<int(result.wall)<<' '<<int(result.applied)<<'\n';
        }
        return 0;
    }
    if(mode=="step_cache"){
        std::int32_t x,y,z,airborne,cx,cy,cz;
        while(std::cin>>x>>y>>z>>airborne>>cx>>cy>>cz){
            dfre::StepVector step;step.x=x;step.y=y;step.z=z;dfre::CachedStep cache;cache.x=cx;cache.y=cy;cache.z=cz;
            const auto result=dfre::cached_step(step,airborne!=0,cache);
            std::cout<<result.x<<' '<<result.y<<' '<<result.z<<' '<<cache.x<<' '<<cache.y<<' '<<cache.z<<'\n';
        }
        return 0;
    }
    if(mode=="ground"){
        std::int32_t px,py,pz,yaw,vertical,on_object,is_local,full_pass;std::uint32_t flags;
        std::int32_t height,water,clearance,dx,dy,dz;
        std::int32_t gx,gy,gz,rotation,offset_x,offset_y;
        while(std::cin>>px>>py>>pz>>yaw>>vertical>>flags>>on_object>>is_local>>full_pass
            >>height>>water>>clearance>>dx>>dy>>dz>>gx>>gy>>gz>>rotation>>offset_x>>offset_y){
            dfre::MotionActor actor;actor.position={px,py,pz};actor.yaw=std::uint32_t(yaw);actor.vertical=vertical;
            actor.flags=flags;actor.on_ground_object=on_object!=0;
            dfre::GroundObject object;object.position={gx,gy,gz};object.rotation=std::uint32_t(rotation);
            object.offset_x=offset_x;object.offset_y=offset_y;
            dfre::GroundQuery query;query.height=height;query.water_level=water;
            query.clearance=clearance;query.full_pass=full_pass!=0;
            dfre::StepVector step;step.x=dx;step.y=dy;step.z=dz;
            const auto result=dfre::ground_update(actor,on_object?&object:nullptr,query,step,is_local!=0);
            std::cout<<actor.position[0]<<' '<<actor.position[1]<<' '<<actor.position[2]<<' '<<std::uint32_t(actor.yaw)<<' '
                <<actor.vertical<<' '<<actor.flags<<' '
                <<int(result.snapped)<<' '<<int(result.entered_water)<<' '<<int(result.left_water)<<' '<<int(result.airborne)<<'\n';
        }
        return 0;
    }
    if(mode=="animation"){
        std::uint32_t control,z,vertical,flags,has_object,mode_value,prone,stance;
        std::uint32_t pulse_jump,pulse_prone,pulse_crouch,pulse_stand,slot_flags,steady,jump_impulse;
        while(std::cin>>control>>z>>vertical>>flags>>has_object>>mode_value>>prone>>stance
            >>pulse_jump>>pulse_prone>>pulse_crouch>>pulse_stand>>slot_flags>>steady>>jump_impulse){
            dfre::MovementActorState state;
            state.control=std::uint16_t(control);state.z=std::int32_t(z);state.vertical=std::int32_t(vertical);
            state.flags=flags;state.has_ground_object=has_object!=0;state.mode=std::int32_t(mode_value);state.prone=std::int32_t(prone);
            state.stance=std::int32_t(stance);state.pulse_jump=std::uint8_t(pulse_jump);
            state.pulse_prone=std::uint8_t(pulse_prone);state.pulse_crouch=std::uint8_t(pulse_crouch);
            state.pulse_stand=std::uint8_t(pulse_stand);
            const auto animation=dfre::movement_animation(state,std::uint8_t(slot_flags),steady!=0,std::int32_t(jump_impulse));
            std::cout<<animation<<' '<<state.z<<' '<<state.vertical<<' '<<state.mode<<' '<<state.prone<<' '<<state.stance
                <<' '<<int(state.pulse_jump)<<' '<<int(state.pulse_prone)<<' '<<int(state.pulse_crouch)<<' '<<int(state.pulse_stand)<<'\n';
        }
        return 0;
    }
    std::cerr<<"motion_step_probe MODE\nModes: sincos frame_step frame_step_math slope ground animation\n";
    return 2;
}
