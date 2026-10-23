#pragma once
#include "dfre/mission_sound_bank.hpp"
#include "dfre/projectiles.hpp"
namespace dfre {
//43E940. Height reads use the current BD5284 source map through shared bindings.
std::uint32_t mission_sound_occlusion(const MissionSoundBankBindings&,Fixed3 source,Fixed3 listener);
//43E7D0. Params is a separate32-byte caller-owned block kept through all calls.
// Map6E06A8/AC/B0/B4 to the shared MissionAudioFrame listener owners.
void mission_sound_positional_request(const MissionSoundBankBindings&,std::uint32_t id,
    std::uint32_t x,std::uint32_t y,std::uint32_t z,std::uint32_t params);
}
