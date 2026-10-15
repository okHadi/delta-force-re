#include "dfre/motion_queries.hpp"
#include <fstream>
#include <iostream>
#include <string>

// Raw 1 MiB elevation map file, exactly the bytes the original reads at 0xBD5284.
namespace {
dfre::TerrainGeometry load(const std::string& path){
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("cannot read map file");
    dfre::AssetBytes map((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    if(map.size()!=1024u*1024u)throw std::runtime_error("map file must be 1 MiB");
    return dfre::TerrainGeometry(std::move(map),dfre::AssetBytes(1024u*1024u),std::vector<std::uint32_t>(65536u));
}
}
int main(int argc,char** argv){
    try{
        if(argc!=3)throw std::runtime_error("motion_queries_probe MODE MAPFILE | MODE: height slope actor_height");
        const std::string mode=argv[1];
        const auto terrain=load(argv[2]);
        std::int32_t x,y;
        if(mode=="height"){
            while(std::cin>>x>>y)std::cout<<dfre::ground_height_query(terrain,x,y)<<'\n';
            return 0;
        }
        if(mode=="slope"){
            while(std::cin>>x>>y){const auto slope=dfre::ground_slope_query(terrain,x,y);std::cout<<slope.x<<' '<<slope.y<<'\n';}
            return 0;
        }
        if(mode=="actor_height"){
            std::int32_t has_entity,entity_height;
            while(std::cin>>x>>y>>has_entity>>entity_height){
                dfre::ActorGroundQuery query;query.has_ground_entity=has_entity!=0;query.entity_height=entity_height;
                std::cout<<dfre::ground_height_for_actor(terrain,query,x,y)<<'\n';
            }
            return 0;
        }
        throw std::runtime_error("unknown mode");
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
}
