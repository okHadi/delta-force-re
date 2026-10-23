#pragma once
#include "dfre/mission_sound_bank.hpp"
namespace dfre {
//43EF80. Bind6E0700 to the current MissionAudioFrame registration owner.
//43EE80 loads a reached ambient sample through the required call provider.
void mission_sound_attach(const MissionSoundBankBindings&,std::uint32_t actor,
    std::uint32_t id,std::uint32_t scale,std::uint32_t ceiling);
std::uint32_t mission_sound_loudness(const MissionSoundBankBindings&,std::uint32_t id); //43F4D0
}
