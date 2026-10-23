#include "dfre/mission_live.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
namespace {
void image(const dfre::AiActorImage& record){static constexpr char digits[]="0123456789abcdef";for(auto byte:record)std::cout<<digits[byte>>4]<<digits[byte&15];}
}
int main(){try{std::string line;while(std::getline(std::cin,line)){
    std::istringstream row(line);std::uint32_t group,slot,active,x,y,z,yaw;row>>group>>slot>>active>>x>>y>>z>>yaw;if(!row||group>=4||slot>1023)throw std::runtime_error("bad linked-position case");
    dfre::MissionTickState state;dfre::AiActorTables tables;tables.groups[3].resize(1);tables.groups[group].resize(std::max<std::size_t>(tables.groups[group].size(),slot+1));
    const dfre::AiActorRef owner{3,0},target{std::uint8_t(group),slot};
    auto& actor=tables.groups[3][0];actor.active=true;actor.definition_index=1;actor.position[0]=x;actor.position[1]=y;actor.position[2]=z;actor.angle_yaw=yaw;
    if(group!=3||slot!=0){auto& carried=tables.groups[group][slot];carried.active=active!=0;carried.definition_index=active?1:0;carried.position[0]=z;carried.position[1]=x;carried.position[2]=y;carried.angle_yaw=~yaw;}
    dfre::init_mission_tick(state,{},std::move(tables),dfre::AiItemDefinitionTable(std::vector<dfre::AiItemDefinition>(2)),{},0);
    const auto raw=dfre::mission_actor_source_address(target.group,target.index);state.runtime_for(owner).link_90=raw;
    image(dfre::snapshot_mission_actor(state,owner));std::cout<<' ';image(dfre::snapshot_mission_actor(state,target));std::cout<<' ';
    dfre::bind_mission_linked_position(state);std::array<std::int32_t,3> position{};const std::array<std::uint32_t,3> bits{x,y,z};std::memcpy(position.data(),bits.data(),sizeof(position));state.world.linked_position(raw,position);
    image(dfre::snapshot_mission_actor(state,owner));std::cout<<' ';image(dfre::snapshot_mission_actor(state,target));std::cout<<'\n';
}return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
