#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionObjectTreeBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
// Current source memory includes all256 physical group3 rows and reached definition/frame tables.
std::uint32_t mission_object_tree_signal(const MissionObjectTreeBindings&,std::uint32_t actor); //44FBA0
// Keep the caller's114h-byte image mapped through every synchronous call.
void mission_object_tree_spawn(const MissionObjectTreeBindings&,std::uint32_t actor,
    std::uint32_t frame,std::uint32_t message=0); //451E70
void bind_mission_object_tree_spawn(MissionTickState&,MissionObjectTreeBindings,std::uint32_t frame);
}
