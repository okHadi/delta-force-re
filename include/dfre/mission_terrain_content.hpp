#pragma once
#include "dfre/mission_client_added.hpp"
#include "dfre/terrain.hpp"
#include <array>
namespace dfre {
// 456F30. Each source byte and table word is a current shared-memory read.
// A zero count follows the source do-while loop; providers must bound their memory.
std::uint32_t mission_terrain_crc(const MissionClientAddedBindings&,std::uint32_t pointer,std::uint32_t count);
// Nine BD67D4..BD67F4 values for a successfully loaded terrain startup.
// Read original decoded assets; runtime supplies current config and optional gates.
std::array<std::uint32_t,9> mission_terrain_content(const AssetStore&,const TerrainRuntime&);
}
