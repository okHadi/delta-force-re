#pragma once
#include "dfre/mission_sound_assets.hpp"
namespace dfre {
// Current raw RTXT storage.499410 supplies a new unfixed file allocation.
std::uint32_t mission_rtxt_load(const MissionSoundAssetBindings&,std::uint32_t filename); //499F1F
std::uint32_t mission_rtxt_fixup(const MissionSoundAssetBindings&,std::uint32_t allocation); //499EA0
std::uint32_t mission_rtxt_lookup(const MissionSoundAssetBindings&,std::uint32_t table,
    std::uint32_t group,std::uint32_t key); //49A092
std::uint32_t mission_rtxt_entry(const MissionSoundAssetBindings&,std::uint32_t table,
    std::uint32_t index); //49A076
}
