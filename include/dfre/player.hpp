#pragma once
#include "dfre/mission.hpp"
#include "dfre/terrain.hpp"
#include <optional>

namespace dfre {
struct PlayerPose {
    std::array<std::int32_t,3> position{};
    std::array<std::uint32_t,3> angles{}; // Full turn is 2^32; yaw, pitch, roll.
};
struct InitialPlayer {
    std::size_t marker_index=0;
    std::uint32_t marker_object_id=0;
    PlayerPose actor,camera;
};
bool single_player_object_enabled(std::uint32_t flags) noexcept;
PlayerPose mission_object_pose(const MissionObject& object) noexcept;
std::optional<InitialPlayer> initial_player(const MissionData& mission,const TerrainGeometry& terrain);
} // namespace dfre
