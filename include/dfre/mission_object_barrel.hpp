#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionObjectBarrelBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
// The caller owns the current 24-byte frame and its memory providers through all calls.
void mission_object_barrel(const MissionObjectBarrelBindings&,std::uint32_t actor,
    std::uint32_t frame,std::uint32_t message=0); //45BF80
void bind_mission_object_barrel(MissionTickState&,MissionObjectBarrelBindings,
    std::uint32_t frame);
}
