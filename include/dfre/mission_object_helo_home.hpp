#pragma once
#include "dfre/mission_object_helo.hpp"
namespace dfre {
std::uint32_t mission_object_helo_home(const MissionObjectHeloBindings&,std::uint32_t actor); //4509F0
//450100: caller frame locals[0,1C), live args actor30/target34/band38/mode3C.
// Keep the40h-byte frame current through every required callback.
void mission_object_helo_altitude(const MissionObjectHeloBindings&,std::uint32_t frame);
}
