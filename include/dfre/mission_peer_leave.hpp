#pragma once
#include "dfre/mission_periodic_services.hpp"
namespace dfre {
// Keep the caller's 206-byte packet and all current owners mapped through calls.
void mission_peer_reset(const MissionPeriodicBindings&,std::uint32_t frame); //4401E0
std::uint32_t mission_peer_leave_drain(const MissionPeriodicBindings&); //403A50 /447F60
}
