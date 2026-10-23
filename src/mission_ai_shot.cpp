#include "dfre/mission_ai_shot.hpp"
#include "dfre/player_weapon_runtime.hpp"
#include <cstring>
#include <utility>

namespace dfre {
MissionAiShot::MissionAiShot(MissionTickState& state,MissionMuzzle& muzzle,
    MissionProjectileSpawn& spawn,MissionAiShotBindings bindings)
    :state_(state),muzzle_(muzzle),spawn_(spawn),bindings_(std::move(bindings)){}
bool MissionAiShot::fire(const AiActorRef& ref){
    if(ref.group!=3 || ref.index>=256 || !state_.actor_at(ref))
        throw MissionTickError("AI shot needs a current group-three actor");
    const auto handle=actor_handle(ref.group,ref.index);
    auto image=snapshot_mission_actor(state_,ref);
    return muzzle_.animated({&image,handle},[&](ProjectileHitActor,const MuzzleResult& result){
        const auto current=snapshot_mission_actor(state_,ref);
        std::uint32_t weapon;std::memcpy(&weapon,current.data()+0x94,4);
        PlayerShotBindings shot;
        shot.random=[&]{return behavior_rng_next(state_.behavior);};
        shot.burst_storage=[&]()->std::uint32_t&{
            const auto* actor=state_.actor_at(ref);
            if(!actor || !actor->person_slot || *actor->person_slot>=state_.people.size())
                throw MissionTickError("AI shot needs its current person");
            return state_.person_extra(*actor->person_slot).field_6c;
        };
        shot.projectile=[&](std::uint32_t kind,MuzzleResult& pose){spawn_.request(handle,kind,pose);};
        shot.melee=[&]{
            if(!bindings_.melee)throw MissionTickError("AI melee body 470B00 is not bound");
            bindings_.melee(state_,ref);
        };
        auto pose=result;execute_player_shot(weapon,pose,shot);
    },[&](ProjectileHitActor){image=snapshot_mission_actor(state_,ref);});
}
} // namespace dfre
