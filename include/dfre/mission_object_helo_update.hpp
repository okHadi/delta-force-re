#pragma once
#include "dfre/mission_object_helo.hpp"
namespace dfre {
// Keep all 0x44 local bytes in the same current raw-memory owner. Use a
// separate frame for each active call. Foreign pose calls receive live pointers.
void mission_object_helo_update(const MissionObjectHeloBindings&,std::uint32_t actor,
    std::uint32_t message,std::uint32_t frame,std::size_t frame_bytes);
void bind_mission_object_helo_update(MissionTickState&,MissionObjectHeloBindings,
    std::uint32_t frame,std::size_t frame_bytes);
} // namespace dfre
