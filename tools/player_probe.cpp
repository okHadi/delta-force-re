#include "dfre/player.hpp"
#include <iostream>
#include <string>
namespace {
void pose(const dfre::PlayerPose& p){std::cout<<"{\"position\":["<<p.position[0]<<','<<p.position[1]<<','<<p.position[2]<<"],\"angles\":["<<p.angles[0]<<','<<p.angles[1]<<','<<p.angles[2]<<"]}";}
}
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="angles"){int yaw,pitch;while(std::cin>>yaw>>pitch){dfre::MissionObject o;o.angle_degrees={std::int16_t(yaw),std::int16_t(pitch)};const auto p=dfre::mission_object_pose(o);std::cout<<p.angles[0]<<' '<<p.angles[1]<<'\n';}return 0;}
 if(argc!=3)throw dfre::AssetError("player_probe GAME_FOLDER MISSION.BMS|--all | angles");
 const dfre::AssetStore assets(argv[1]);const auto names=std::string(argv[2])=="--all"?assets.missions():std::vector<std::string>{argv[2]};std::cout<<"[";bool first=true;
 for(const auto& name:names){const auto mission=dfre::load_mission(assets,name);const auto terrain=dfre::load_terrain_runtime(assets,name);const auto player=dfre::initial_player(mission,terrain.geometry);if(!first)std::cout<<',';first=false;std::cout<<"{\"mission\":\""<<name<<"\",\"start\":";
  if(!player)std::cout<<"null";else{std::cout<<"{\"marker_index\":"<<player->marker_index<<",\"marker_object_id\":"<<player->marker_object_id<<",\"actor\":";pose(player->actor);std::cout<<",\"camera\":";pose(player->camera);std::cout<<'}';}std::cout<<'}';}
 std::cout<<"]\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
