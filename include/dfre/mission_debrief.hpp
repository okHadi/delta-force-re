#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionDebriefBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>&)> call;
};
// Keep current result/profile/menu/RTXT owners and the 64h-byte entry frame alive
// through synchronous callbacks. Each reached foreign body requires a binding.
void mission_debrief_enter(const MissionDebriefBindings&,std::uint32_t frame); //41F390
std::uint32_t mission_debrief_update(const MissionDebriefBindings&); //41F510
void mission_debrief_render(const MissionDebriefBindings&); //41F570
void mission_debrief_leave(const MissionDebriefBindings&); //41F5E0
void mission_debrief_accept(const MissionDebriefBindings&,std::uint32_t button,std::uint32_t action); //41F700
void mission_debrief_quit(const MissionDebriefBindings&,std::uint32_t button,std::uint32_t action); //420E50
}
