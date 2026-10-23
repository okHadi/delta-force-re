#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace dfre {
struct MissionPeriodicBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>& args)> call;
};
// Keep each mapped frame and every source owner current through callbacks.
void mission_periodic_scoreboard(const MissionPeriodicBindings&,std::uint32_t frame); //4438C0; 0x130-byte frame
void mission_periodic_poll_clients(const MissionPeriodicBindings&); //448C30
void mission_periodic_clear_actor_flags(const MissionPeriodicBindings&); //445C80
void mission_periodic_advertise(const MissionPeriodicBindings&,std::uint32_t frame); //446200; 0x500-byte frame
void mission_periodic_publish_flag(const MissionPeriodicBindings&,std::uint32_t frame); //4482E0; 0x20-byte frame
void mission_periodic_team_scores(const MissionPeriodicBindings&); //446DA0
std::int32_t mission_periodic_compare_clients(const MissionPeriodicBindings&,
    std::uint32_t first,std::uint32_t second); //448130
} // namespace dfre
