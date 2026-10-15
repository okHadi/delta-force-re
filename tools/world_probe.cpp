#include "dfre/world.hpp"
#include <iostream>
int main(int argc,char** argv){try{
    if(argc!=3)throw dfre::AssetError("world_probe GAME_FOLDER MISSION.BMS|--all");
    const dfre::AssetStore assets(argv[1]);
    const auto names=std::string(argv[2])=="--all"?assets.missions():std::vector<std::string>{argv[2]};
    std::cout<<'[';bool first_mission=true;
    for(const auto& name:names){
        const auto mission=dfre::load_mission(assets,name);const auto terrain=dfre::load_terrain_runtime(assets,name);
        const auto placed=dfre::place_single_player_objects(mission,terrain.geometry);
        if(!first_mission)std::cout<<',';first_mission=false;std::cout<<"{\"mission\":\""<<name<<"\",\"objects\":[";
        bool first=true;for(const auto& o:placed){if(!first)std::cout<<',';first=false;
            std::cout<<"{\"group\":"<<o.group<<",\"index\":"<<o.index<<",\"object_id\":"<<o.object_id<<",\"type_id\":"<<o.type_id<<",\"position\":["<<o.pose.position[0]<<','<<o.pose.position[1]<<','<<o.pose.position[2]<<"],\"angles\":["<<o.pose.angles[0]<<','<<o.pose.angles[1]<<','<<o.pose.angles[2]<<"]}";
        }std::cout<<"]}";
    }std::cout<<"]\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
