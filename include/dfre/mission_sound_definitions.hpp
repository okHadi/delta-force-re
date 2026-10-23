#pragma once
#include "dfre/mission_sound_assets.hpp"
namespace dfre {
// 43E340. Bind43EDB0 to the current ambient definition loader.
// The source table has512 records of40 bytes at4F2EA4.
void mission_sound_definitions_initialize(const MissionSoundAssetBindings&,std::uint32_t name);
}
