#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionObjectMineUpdateBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
//451D40. Keep the current eight-byte sine/cosine frame in the same owner.
void mission_object_mine_update(const MissionObjectMineUpdateBindings&,
    std::uint32_t actor,std::uint32_t frame);
void bind_mission_object_mine_update(MissionTickState&,MissionObjectMineUpdateBindings,
    std::uint32_t frame);
}
