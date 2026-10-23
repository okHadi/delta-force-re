#pragma once
#include "dfre/mission_object_helo.hpp"
namespace dfre {
//450830. Map the current actor/person/definition and all28 local bytes in
// one raw-memory owner. Bind450630,477620,477770,477860 and45EA00.
void mission_object_helo_shot(const MissionObjectHeloBindings&,std::uint32_t actor,
    std::uint32_t frame,std::size_t frame_bytes);
} // namespace dfre
