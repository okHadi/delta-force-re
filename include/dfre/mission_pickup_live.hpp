#pragma once
#include "dfre/actor_pickups.hpp"
#include "dfre/mission_live.hpp"

namespace dfre {
struct MissionPickupLiveBindings {
    std::function<void(MissionTickState&,ActorPickupState&,std::size_t,std::uint32_t,std::uint32_t,std::uint32_t)> client_message;
    std::function<void(MissionTickState&,ActorPickupState&,std::size_t)> resupply;
    std::function<std::uint32_t(MissionTickState&,ActorPickupState&,std::uint32_t)> sound;
    std::function<void(MissionTickState&,ActorPickupState&)> local_flag;
    // Execute full 45DA30 person/character/actor cleanup through its real owner.
    std::function<void(MissionTickState&,ActorPickupState&,const AiActorRef&)> remove;
    std::function<void(MissionTickState&,ActorPickupState&,ActorPickupTransport,std::uint32_t,const std::vector<std::uint8_t>&)> message;
    // Pose is the current target actor +24, all 16 bytes through yaw +30.
    std::function<void(MissionTickState&,ActorPickupState&,const AiActorRef& actor,const AiActorRef& pose,std::uint32_t)> objective;
    std::function<std::optional<AiActorRef>(MissionTickState&,ActorPickupState&,std::uint32_t owner,Fixed3)> allocate;
};
// Keep shared state/clients alive for the returned provider. It owns no client
// storage. Change host/local-player cells through MissionTickState in callbacks.
// Calls use typed actor handles. Stored actor+90 uses the target source address.
// can_use returns the defined restore-home value. Void paths return an ignored
// collision ABI value; actor_target_collision discards those trigger returns.
MissionCollisionEffect mission_pickup_live_effect(ActorPickupState& shared,MissionPickupLiveBindings bindings);
} // namespace dfre
