#pragma once
#include "dfre/mission_object_helo.hpp"
namespace dfre {
// Bind current graphic+50/LOD+A4 hardpoint memory and all matrix services in
// the same raw-memory owner. Use separate frames for active calls.
void mission_object_helo_smoke_pose(const MissionObjectHeloBindings&,std::uint32_t actor,
    std::uint32_t xyz,std::uint32_t frame,std::size_t frame_bytes); // 4505D0,40h frame
void mission_object_helo_weapon_pose(const MissionObjectHeloBindings&,std::uint32_t actor,
    std::uint32_t pose,std::uint32_t frame,std::size_t frame_bytes); // 450630,80h frame
} // namespace dfre
