#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace dfre {
struct MissionScoreboardBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
    std::function<std::uint32_t(std::uint32_t site,const std::vector<std::uint32_t>& args)> call;
};
//46D9F0 message15,46EBC7..46ED00. Map the caller frame through6B,
//with the current packet pointer at+44. Keep stack locals and records shared.
void mission_scoreboard_receive(const MissionScoreboardBindings&,std::uint32_t frame);
}
