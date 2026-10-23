#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionObjectSeagullBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
std::uint32_t mission_seagull_flock_first(const MissionObjectSeagullBindings&,std::uint32_t actor); //44FD30
std::uint32_t mission_seagull_flock_index(const MissionObjectSeagullBindings&,std::uint32_t actor); //44FD80
void mission_object_seagull_ai(const MissionObjectSeagullBindings&,std::uint32_t actor,
    std::uint32_t frame); //451FD0; 28 current bytes in the same owner
void mission_object_seagull_move(const MissionObjectSeagullBindings&,std::uint32_t actor); //452310
void bind_mission_object_seagull(MissionTickState&,MissionObjectSeagullBindings,std::uint32_t frame);
}
