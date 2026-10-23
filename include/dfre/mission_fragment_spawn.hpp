#pragma once
#include "dfre/mission_projectile_spawn.hpp"
namespace dfre {
struct MissionFragmentSpawnBindings {
    std::function<void(std::uint32_t,std::int32_t&,std::int32_t&)> sincos_4776c0;
};
// Share the mission's512-slot pool and A158A4 cells with MissionProjectileSpawn.
class MissionFragmentSpawn {
public:
    MissionFragmentSpawn(MissionTickState&,MissionProjectileSpawnCells&,MissionFragmentSpawnBindings);
    std::uint32_t spawn(const std::array<std::uint32_t,6>& position,std::uint32_t source_owner,std::uint32_t speed); //463200
private:
    MissionTickState& state_;
    MissionProjectileSpawnCells& cells_;
    MissionFragmentSpawnBindings bindings_;
};
}
