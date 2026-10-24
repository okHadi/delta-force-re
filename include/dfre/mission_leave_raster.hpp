#pragma once
#include "dfre/mission_debrief.hpp"
namespace dfre {
// Keep scalar arguments, caller descriptor (18h bytes), raster rows, and pixel
// storage stable through synchronous calls. Memory and backend providers are required.
std::uint32_t mission_leave_frame(const MissionDebriefBindings&,std::uint32_t destination,std::uint32_t descriptor); //404540
void mission_leave_present(const MissionDebriefBindings&); //404610
void mission_raster_rows(const MissionDebriefBindings&,std::uint32_t destination,std::uint32_t descriptor); //4719F0
std::uint32_t mission_frame_pointer(const MissionDebriefBindings&); //496620
std::uint32_t mission_frame_zero(); //496630
void mission_display_present(const MissionDebriefBindings&); //496C50
void mission_display_rows(const MissionDebriefBindings&,std::uint32_t first,std::uint32_t last); //496C70
}
