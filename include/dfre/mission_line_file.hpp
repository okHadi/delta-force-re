#pragma once
#include "dfre/mission_sound_assets.hpp"
namespace dfre {
struct MissionLineFileBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>&)> call;
    //4565F0: the source caller passes the output area in ESI.
    std::function<void(std::uint32_t line,std::uint32_t output)> tokenize;
};
//4568E0. Keep its0x3C0-byte frame and caller arguments mapped through callbacks.
// The filename and callback cells are at frame+0x3D4 and frame+0x3D8.
std::uint32_t mission_line_file(const MissionLineFileBindings&,std::uint32_t frame);
}
