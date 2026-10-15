#include "dfre/movement.hpp"
#include <cstdint>
#include <iostream>
#include <string>

// Line probes for tools/check_movement_original.py. Each mode reads one case
// per line and prints one result per line.
int main(int argc,char** argv){
    const std::string mode=argc>1?argv[1]:"";
    if(mode=="mouse_look"){
        std::int32_t axis_x,axis_y,sensitivity,flip,steady;
        while(std::cin>>axis_x>>axis_y>>sensitivity>>flip>>steady){
            const auto result=dfre::mouse_look(axis_x,axis_y,sensitivity,flip!=0,steady!=0);
            std::cout<<result.turn<<' '<<result.look<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(mode=="look_step"){
        std::uint32_t flags0,flags1,yaw,pitch;
        while(std::cin>>flags0>>flags1>>yaw>>pitch){
            dfre::PlayerPose pose;pose.angles={yaw,pitch,0};
            const auto result=dfre::apply_look_step(pose,std::uint8_t(flags0),std::uint8_t(flags1));
            std::cout<<result.angles[0]<<' '<<result.angles[1]<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(mode=="look_action"){
        std::uint32_t action;std::int32_t value;std::uint32_t yaw,pitch;
        while(std::cin>>action>>value>>yaw>>pitch){
            dfre::PlayerPose pose;pose.angles={yaw,pitch,0};
            const auto result=dfre::apply_look_action(std::uint16_t(action),value,pose);
            std::cout<<result.angles[0]<<' '<<result.angles[1]<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(mode=="step"){
        std::int32_t x,y,z,vertical,dx,dy,dz;
        while(std::cin>>x>>y>>z>>vertical>>dx>>dy>>dz){
            dfre::ActorMotion actor;actor.position={x,y,z};actor.vertical_velocity=vertical;
            dfre::apply_step(actor,dx,dy,dz);
            std::cout<<actor.position[0]<<' '<<actor.position[1]<<' '<<actor.position[2]<<' '<<actor.vertical_velocity<<'\n';
        }
        return std::cin.eof()?0:2;
    }
    if(mode=="move_dir"){
        std::uint32_t flags0,actor_bit;
        while(std::cin>>flags0>>actor_bit)
            std::cout<<dfre::move_octant(std::uint8_t(flags0))<<' '<<dfre::move_anim_selector(std::uint8_t(flags0),actor_bit!=0)<<'\n';
        return std::cin.eof()?0:2;
    }
    std::cerr<<"movement_probe MODE\nModes: mouse_look look_step look_action step move_dir\n";
    return 2;
}
