#pragma once
#include "dfre/mission_peer_parser.hpp"
#include "dfre/mission_peer_requests.hpp"
#include "dfre/mission_projectile_spawn.hpp"
namespace dfre {
// The state, spawn owner, request bindings and callback payloads stay live.
class MissionPeerProjectile {
public:
    MissionPeerProjectile(MissionTickState&,MissionProjectileSpawn&,const MissionPeerRequestBindings&);
    static bool owns(std::uint32_t address) noexcept;
    std::uint32_t call(const MissionPeerParserCall&);
private:
    MissionTickState& state_;
    MissionProjectileSpawn& spawn_;
    const MissionPeerRequestBindings& requests_;
};
}
