#pragma once
#include "dfre/mission_debrief.hpp"
namespace dfre {
// Execute 407C61..407C86 with current memory and one captured profile pointer.
void mission_profile_start_mask(const MissionDebriefBindings&);
// Execute 403AA1..403AC8. Return the captured 503328 branch value.
// The required 49A620 call clears the caller-owned 514h-byte result image.
std::uint32_t mission_profile_start_result(const MissionDebriefBindings&);
}
