#pragma once
#include "dfre/mission_object_live.hpp"
#include "dfre/mission_client_outbox.hpp"
namespace dfre {
struct MissionObjectDestructionBindings {
    std::function<void(std::uint32_t)> before_actor_read;
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> definition;
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> model;
    std::function<std::uint32_t(std::uint32_t)> lookup_4828c0;
    std::function<std::uint32_t()> item_table_base;
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> item_word;
    // Calls are synchronous. Mutable buffers remain valid until the call returns.
    std::function<void(std::uint32_t,const std::array<std::uint32_t,6>&,
                       std::uint8_t*,std::size_t)> call;
};
void mission_object_destroy_generic_raw(MissionTickState&,const AiActorRef&,
    const MissionObjectDestructionBindings&,std::uint32_t incoming_ebx); //453270
void mission_object_destroy_generic(MissionTickState&,const AiActorRef&,
    const MissionObjectDestructionBindings&); //4533F0 caller EBX
void mission_object_destroy_wood(MissionTickState&,const AiActorRef&,
    const MissionObjectDestructionBindings&,bool large); //44ED40/44F270
void mission_object_destroy_broadcast(MissionClientOutbox&,std::uint32_t raw_actor,
    std::uint32_t value); //447980
} // namespace dfre
