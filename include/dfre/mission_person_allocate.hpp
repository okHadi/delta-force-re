#pragma once
#include "dfre/mission_actor_create.hpp"
namespace dfre {
//44A0C0. Use the same current memory and clear service as the actor factory.
std::uint32_t mission_person_allocate(const MissionActorCreateBindings&,std::uint32_t owner);
} // namespace dfre
