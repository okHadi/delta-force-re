#pragma once
#include "dfre/mission_muzzle.hpp"
#include "dfre/mission_projectile_spawn.hpp"

namespace dfre {
struct MissionAiShotBindings {
    std::function<void(MissionTickState&,const AiActorRef&)> melee; // 470B00
};
// Keep all referenced owners alive. Use the current combat actor, including
// its temporary yaw and pitch. The combat caller owns their later restoration.
class MissionAiShot {
public:
    MissionAiShot(MissionTickState& state,MissionMuzzle& muzzle,
        MissionProjectileSpawn& spawn,MissionAiShotBindings bindings={});
    bool fire(const AiActorRef& actor); // 45E7A0 -> 45EA00 -> 46FB40
private:
    MissionTickState& state_;
    MissionMuzzle& muzzle_;
    MissionProjectileSpawn& spawn_;
    MissionAiShotBindings bindings_;
};
} // namespace dfre
