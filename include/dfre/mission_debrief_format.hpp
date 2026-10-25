#pragma once
#include "dfre/mission_debrief.hpp"
namespace dfre {
// Closed493A80 service for the fourteen source Debrief/startup/Ready literals. Keep literal bytes
// immutable and their reads pure through the call. Args are captured immutable
// scalars, not a mutable CRT argument frame. Strings use current byte reads.
std::uint32_t mission_debrief_format(const MissionDebriefBindings&,
    const std::vector<std::uint32_t>& args);
}
