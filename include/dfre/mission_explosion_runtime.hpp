#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
// Source4657D0. The canonical vector owns the active9E2E40/A15898 queue.
bool mission_explosion_enqueue(MissionTickState&,std::uint32_t kind,Fixed3 position,std::uint32_t owner_identity);
// Convert a raw source actor address only when the source gates accept a row.
bool mission_explosion_enqueue_source(MissionTickState&,std::uint32_t kind,Fixed3 position,std::uint32_t source_owner);
}
