#pragma once
#include "dfre/mission_client_outbox.hpp"
#include <functional>
#include <vector>
namespace dfre {
struct MissionPeerRequestBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
};
// 447150: trigger current owned charge actors through their real +110 body.
void mission_peer_trigger_charges(const MissionPeerRequestBindings&,std::uint32_t owner_actor_address);
// 447760: reserve a current shared strike timer, then enqueue 2D or 2E.
void mission_peer_request_strike(const MissionPeerRequestBindings&,MissionClientOutbox&,std::size_t client_slot);
// 445B80: append the mutable 15-byte shot to the shared 100-record ledger.
void mission_peer_record_shot(const MissionPeerRequestBindings&,std::uint8_t*,std::size_t);
}
