#include "dfre/world.hpp"
#include <cstring>

namespace dfre {
std::vector<WorldPlacement> place_single_player_objects(const MissionData& mission,const TerrainGeometry& terrain) {
    std::vector<WorldPlacement> result;
    for(std::size_t group=0;group<mission.object_groups.size();++group) {
        const auto& objects=mission.object_groups[group];
        for(std::size_t index=0;index<objects.size();++index) {
            const auto& object=objects[index];
            if(!single_player_object_enabled(object.flags))continue;
            auto pose=mission_object_pose(object);
            const auto z=std::uint32_t(pose.position[2])+std::uint32_t(terrain.base_height(pose.position[0],pose.position[1]))+(group==3?65536u:0u);
            std::memcpy(&pose.position[2],&z,sizeof(z));
            result.push_back({group,index,object.object_id,object.type_id,pose});
        }
    }
    return result;
}
}
