#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
namespace dfre {
struct MissionPeerExpiryRecord { std::uint32_t* words=nullptr;std::size_t count=0; };
struct MissionPeerExpiryBindings {
    std::function<std::uint32_t()> enabled_67f528;
    std::function<std::uint16_t()> fp_control;
    std::function<void(std::uint32_t,std::uint8_t*,std::size_t)> enqueue; //442630
    std::function<void(std::uint32_t,MissionPeerExpiryRecord,std::uint32_t)> apply; //460950
};
std::uint32_t mission_peer_request_height(std::uint32_t,std::uint16_t);
// Source4470C0. Input stays writable and stable through both required calls.
void mission_peer_expiry(const MissionPeerExpiryBindings&,std::uint32_t actor_address,MissionPeerExpiryRecord);
struct MissionPeerExpiryEffectBindings {
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
    std::function<void(std::array<std::uint32_t,6>&,std::uint32_t,std::uint32_t)> spawn; //463200
};
// Source460950. Word5 of its local record is caller stack data.
void mission_peer_expiry_effect(const MissionPeerExpiryEffectBindings&,std::uint32_t actor_address,
    MissionPeerExpiryRecord,std::int32_t count,std::uint32_t caller_scratch_tail);
}
