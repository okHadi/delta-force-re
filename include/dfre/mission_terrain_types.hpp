#pragma once
#include "dfre/assets.hpp"
#include <array>
#include <functional>
namespace dfre {
struct MissionTerrainTypeBindings {
 std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
 std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
};
// 47B4A0. Read current palette/count/row bytes and publish all 256 type bytes.
void mission_terrain_classify(const MissionTerrainTypeBindings&,std::uint32_t palette);
// 47AAA0 defaults + 47AB00 type rows + 47B4A0, using the raw color-map palette.
std::array<std::uint8_t,256> mission_terrain_types(const PcxImage&,const TerrainConfig&);
}
