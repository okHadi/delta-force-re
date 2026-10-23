#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionObjectHeloBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
// Route source addresses to current actor, person, definition and global owners.
void mission_object_helo_turn(const MissionObjectHeloBindings&,std::uint32_t actor); // 44FF30
void mission_object_helo_speed(const MissionObjectHeloBindings&,std::uint32_t actor,std::uint32_t target); // 44FFC0
void mission_object_helo_integrate(const MissionObjectHeloBindings&,std::uint32_t actor); // 450010
void mission_object_helo_bank(const MissionObjectHeloBindings&,std::uint32_t actor); // 450090
void mission_object_helo_move(const MissionObjectHeloBindings&,std::uint32_t actor); // 451590
void bind_mission_object_helo_move(MissionTickState&,MissionObjectHeloBindings);
} // namespace dfre
