#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionObjectSmokeBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
void mission_object_smoke_generator(const MissionObjectSmokeBindings&,std::uint32_t actor); //42A520
void mission_object_smoke_move(const MissionObjectSmokeBindings&,std::uint32_t actor); //42A590
// Use the same physical primary and extra actor owners as the memory bindings.
void bind_mission_object_smoke(MissionTickState&,MissionObjectSmokeBindings);
}
