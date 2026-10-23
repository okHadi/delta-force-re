#pragma once
#include "dfre/mission_actor_lifecycle.hpp"
#include "dfre/mission_client_outbox.hpp"
namespace dfre {
struct MissionEventActorFlagBindings {
    std::function<std::int32_t(std::size_t group)> current_count;
    std::function<void(std::uint32_t address)> before_read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> before_write;
    // Current actor +110 body, called synchronously with message1.
    std::function<void(const AiActorRef&,std::uint32_t callback,std::uint32_t message)> update;
    std::function<void(const AiActorRef&)> remove_45da30;
    std::function<void(std::uint32_t type,std::uint8_t* bytes,std::uint32_t length)> broadcast_442630;
};
// 45F4C0. Pass the one shared A158BC damage cell by reference.
void mission_event_actor_flags(MissionTickState&,std::uint32_t side,std::uint32_t& damage_a158bc,
    const MissionEventActorFlagBindings&);
// 45F5B0 and its host-gated4471D0 helper.
void mission_event_actor_remove(MissionTickState&,std::uint32_t side,const MissionEventActorFlagBindings&);
void mission_event_actor_remove_one(MissionTickState&,const AiActorRef&,const MissionEventActorFlagBindings&);
// Keep the shared lifecycle and outbox owners alive. Bind each reached update.
MissionEventActorFlagBindings mission_event_actor_flag_bindings(MissionActorLifecycle&,MissionClientOutbox&,
    std::function<void(const AiActorRef&,std::uint32_t,std::uint32_t)> update={});
}
