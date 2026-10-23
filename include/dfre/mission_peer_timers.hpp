#pragma once
#include <array>
#include <cstdint>
#include <functional>
namespace dfre {
using MissionPeerExpiryPosition=std::array<std::uint32_t,7>;
struct MissionPeerTimerBindings {
    std::function<std::uint32_t(std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> height; //47DF40
    std::function<void(std::uint32_t,MissionPeerExpiryPosition&)> expire; //4470C0
    std::function<std::uint16_t()> fp_control; // current x87 control, read view
};
// Source4477F0. Bind current cells6E5778..6E5880 to their shared owner.
void mission_peer_timers(const MissionPeerTimerBindings&);
std::uint32_t mission_peer_expiry_height(std::uint32_t,std::uint16_t);
}
