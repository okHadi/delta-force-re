#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
// Physical actor index from 7CCA80, with stride 114h. Callers keep -1 as null.
AiActorRef mission_projectile_owner_ref(const MissionTickState&,std::int32_t index);
std::int32_t mission_projectile_owner_index(const MissionTickState&,const AiActorRef&);
// A nonnull source address must name one current, aligned physical record.
AiActorRef mission_projectile_owner_source_ref(const MissionTickState&,std::uint32_t source);
}
