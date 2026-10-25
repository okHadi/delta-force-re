#pragma once
#include "dfre/mission_client_added.hpp"
namespace dfre {
//46BFB0:20 hex current local bytes, with no arguments. Tables remain in the
//same raw memory owner. Localization, format and4828C0 stay required.
void mission_weapon_startup(const MissionClientAddedBindings&,
    std::uint32_t frame,std::uint32_t bytes);
} // namespace dfre
