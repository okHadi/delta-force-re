#include "dfre/player.hpp"
#include <cstring>

namespace dfre {
namespace {
std::int32_t signed_bits(std::uint32_t value) noexcept {std::int32_t result;std::memcpy(&result,&value,sizeof(result));return result;}
std::int32_t add(std::int32_t a,std::int32_t b) noexcept {return signed_bits(std::uint32_t(a)+std::uint32_t(b));}
std::uint32_t angle(int degrees) noexcept {
    // Original4497C2/4497DD wrap the shift before signed division by360.
    const auto numerator=signed_bits(std::uint32_t(degrees)<<16);
    return std::uint32_t(numerator/360)<<16;
}
}
bool single_player_object_enabled(std::uint32_t flags) noexcept{return !(flags&0x90);}
PlayerPose mission_object_pose(const MissionObject& object) noexcept {
    return {object.position_raw,{angle(90-int(object.angle_degrees[0])),angle(object.angle_degrees[1]),0}};
}
std::optional<InitialPlayer> initial_player(const MissionData& mission,const TerrainGeometry& terrain) {
    const auto& markers=mission.object_groups[2];
    for(std::size_t i=0;i<markers.size();++i){const auto& marker=markers[i];
        if(marker.type_id!=6001||!single_player_object_enabled(marker.flags))continue;
        InitialPlayer result;result.marker_index=i;result.marker_object_id=marker.object_id;result.actor=mission_object_pose(marker);
        result.actor.position[2]=add(result.actor.position[2],terrain.base_height(result.actor.position[0],result.actor.position[1]));
        result.camera=result.actor;result.camera.position[2]=add(result.camera.position[2],0xb333);return result;
    }
    return {};
}
} // namespace dfre
