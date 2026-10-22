#include "dfre/motion_clearance.hpp"
#include <iostream>
#include <string>
#include <vector>

// Line probes for tools/check_motion_clearance_original.py.
//   clearance: "count mask value0 value1 value2 ..." one array per line.
//   entity:    a stream of cases, each "terrain x y z depth entity_count" followed
//              by per-entity, per-instance, per-part and per-volume records; each
//              result line is "height entity_index" (-1 when no entity matched).
namespace {
dfre::GroundPiece read_piece(){
    dfre::GroundPiece piece;std::size_t volume_count;
    std::cin>>piece.flags>>piece.point[0]>>piece.point[1]
        >>piece.bounds[0]>>piece.bounds[1]>>piece.bounds[2]>>piece.bounds[3]>>volume_count;
    for(std::size_t v=0;v<volume_count;++v){
        dfre::GroundVolume volume;std::int32_t skip,accepted;
        std::cin>>skip>>volume.bounds[0]>>volume.bounds[1]>>volume.bounds[2]>>volume.bounds[3]>>accepted>>volume.height;
        volume.skip=skip!=0;volume.accepted=accepted!=0;
        piece.volumes.push_back(volume);
    }
    return piece;
}
}
int main(int argc,char** argv){
    const std::string mode=argc>1?argv[1]:"";
    if(mode=="clearance"){
        std::size_t count;std::uint32_t mask;
        while(std::cin>>count>>mask){
            std::vector<dfre::PosePosition> poses;
            for(std::size_t i=0;i<count;++i){dfre::PosePosition pose{};std::cin>>pose[0]>>pose[1]>>pose[2];poses.push_back(pose);}
            std::cout<<dfre::ground_clearance(poses,mask)<<'\n';
        }
        return 0;
    }
    if(mode=="entity"){
        std::int32_t terrain,x,y,z,depth;std::size_t entity_count;
        while(std::cin>>terrain>>x>>y>>z>>depth>>entity_count){
            std::vector<std::array<dfre::GroundDefinition,2>> definitions;
            std::vector<dfre::GroundEntity> entities;
            definitions.reserve(entity_count);entities.reserve(entity_count);
            for(std::size_t e=0;e<entity_count;++e){
                dfre::GroundEntity entity;std::int32_t has_definition,definition_radius;
                std::int32_t has_alternate,alternate_radius;std::size_t piece_count;
                std::cin>>entity.flags>>has_definition>>definition_radius>>has_alternate>>alternate_radius
                    >>entity.position[0]>>entity.position[1]>>piece_count;
                std::vector<dfre::GroundPiece> pieces;
                for(std::size_t i=0;i<piece_count;++i)pieces.push_back(read_piece());
                std::array<dfre::GroundDefinition,2> owned{};
                owned[0].radius=definition_radius;owned[0].pieces=pieces;
                owned[1].radius=alternate_radius;owned[1].pieces=std::move(pieces);
                definitions.push_back(std::move(owned));
                const auto* stored=definitions.back().data();
                entity.definition=has_definition?&stored[0]:nullptr;
                entity.alternate=has_alternate?&stored[1]:nullptr;
                entities.push_back(entity);
            }
            const auto result=dfre::ground_entity_height(terrain,{x,y,z},depth,entities);
            std::cout<<result.height<<' '<<(result.entity?std::int32_t(result.entity-entities.data()):-1)<<'\n';
        }
        return 0;
    }
    std::cerr<<"motion_clearance_probe MODE | clearance entity\n";
    return 2;
}
