#pragma once
#include "dfre/mission_object_helo.hpp"
namespace dfre {
// All output addresses and the 16-byte frame use the same current memory owner.
// False marker paths leave the output words unchanged.
std::uint32_t mission_object_helo_marker(const MissionObjectHeloBindings&,std::uint32_t actor,
    std::uint32_t angle,std::uint32_t distance,std::uint32_t radius,std::uint32_t yaw); //45C6A0
std::uint32_t mission_object_helo_condition(const MissionObjectHeloBindings&,std::uint32_t actor,
    std::uint32_t frame,std::size_t bytes); //450AE0
}
