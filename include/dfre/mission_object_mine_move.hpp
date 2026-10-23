#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionObjectMineMoveBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
};
//451DD0 uses the current physical actor record.
void mission_object_mine_move(const MissionObjectMineMoveBindings&,std::uint32_t actor);
void bind_mission_object_mine_move(MissionTickState&,MissionObjectMineMoveBindings);
}
