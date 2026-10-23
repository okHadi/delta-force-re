#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionTerrainMarkBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    // Calls complete synchronously. 45D970 receives its mutable0x114 image here.
    std::function<void(std::uint32_t site,const std::array<std::uint32_t,6>&,
        std::uint8_t* image,std::size_t length)> call;
};
void mission_terrain_mark(MissionTickState&,Fixed3,const MissionTerrainMarkBindings&); //45DAB0
} // namespace dfre
