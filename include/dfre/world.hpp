#pragma once
#include "dfre/player.hpp"
#include <vector>

namespace dfre {
struct WorldPlacement {
    std::size_t group{},index{};
    std::uint32_t object_id{},type_id{};
    PlayerPose pose;
};
// Initial single-player BMS poses after original terrain height adjustment.
std::vector<WorldPlacement> place_single_player_objects(const MissionData& mission,const TerrainGeometry& terrain);
}
