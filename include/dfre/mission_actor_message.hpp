#pragma once
#include "dfre/projectiles.hpp"
#include <array>
#include <functional>
namespace dfre {
struct MissionActorMessageBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t type,std::array<std::uint8_t,2>& payload)> broadcast; //4423F0
    std::function<void(std::uint32_t sound,Fixed3 position)> positional; //43E7D0
};
void mission_actor_message_send(const MissionActorMessageBindings&,std::uint32_t raw_actor,std::uint32_t message); //445CE0
// Behavior actions supply the already computed unsigned source quotient.
void mission_actor_message_send_index(const MissionActorMessageBindings&,std::uint32_t index,std::uint32_t message);
// The message word is read after current actor/definition/position reads.
void mission_actor_message_receive(const MissionActorMessageBindings&,std::uint32_t raw_actor,const std::uint32_t& message); //43F5A0
}
