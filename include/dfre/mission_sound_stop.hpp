#pragma once
#include "dfre/mission_sound_bank.hpp"
namespace dfre {
//43F020: clear matching current registrations, then the current actor link.
void mission_sound_stop(const MissionSoundBankBindings&,std::uint32_t actor,std::uint32_t scale);
}
