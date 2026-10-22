#pragma once
#include "dfre/mission_live.hpp"
#include "dfre/weapons.hpp"

namespace dfre {
using MissionSpawnPacket=std::array<std::uint8_t,15>;
struct MissionProjectileSpawnCells {
    std::uint32_t ring_index=0; // Original A158A4 initial cell.
    std::uint8_t rules_8cf982=0,rules_67dbc9=0;
    std::array<WeaponClassDefinition,kWeaponClassCount> classes=weapon_classes();
};
struct MissionProjectileSpawnBindings {
    std::function<std::int32_t(MissionTickState&,std::int32_t,std::int32_t)> ground;
    std::function<void(MissionTickState&,MissionSpawnPacket&)> host_message; // 445B80
    std::function<void(MissionTickState&,std::uint32_t,const std::vector<std::uint8_t>&)> queue; // 46F3E0
    std::function<void(MissionTickState&,std::size_t,std::uint32_t,std::uint32_t,std::uint32_t)> attached_sound; // 43EF80
    std::function<void(MissionTickState&,std::uint32_t,Fixed3)> positional_sound; // 43E7D0
    std::function<std::int32_t(MissionTickState&,std::uint32_t)> loudness; // 43F4D0
    std::function<void(MissionTickState&,Fixed3,std::int32_t,std::int32_t,std::uint32_t)> alert; // 45FCF0
};
// Keep the mission, cells, and loaded definition vector alive. The mission owns
// the initialized 512-slot projectile pool. Item bindings follow 4633F0.
class MissionProjectileSpawn {
public:
    MissionProjectileSpawn(MissionTickState&,MissionProjectileSpawnCells&,
        const std::vector<RuntimeItemDefinition>&,MissionProjectileSpawnBindings);
    void request(std::uint32_t owner,std::uint32_t kind,MuzzleResult&); // 46FB40
    void receive(MissionSpawnPacket&); // 46EA40 dispatcher body
    // Keep the mutable 15-byte receive buffer alive through its callbacks.
    void receive(std::uint8_t* bytes,std::size_t length);
    std::uint32_t spawn(std::uint32_t owner,std::uint32_t kind,MuzzleResult&); // 462F20
private:
    MissionTickState& state_;
    MissionProjectileSpawnCells& cells_;
    const std::vector<RuntimeItemDefinition>& definitions_;
    MissionProjectileSpawnBindings bindings_;
    std::array<std::uint32_t,3> item_indexes_{};
    AiActorRef owner_ref(std::uint32_t) const;
};
} // namespace dfre
