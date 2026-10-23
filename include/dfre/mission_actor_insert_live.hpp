#pragma once
#include "dfre/mission_actor_insert.hpp"
#include "dfre/mission_tick.hpp"

namespace dfre {
// Explicit449620 clear of the canonical1024 physical extra records.
// Keep the separate8CFA14 count owner current across this clear.
void prepare_mission_extra_actor_pool(MissionTickState&);
// Return the exact physical record base for sound and other shared memory readers.
// Invalid widths and cross-record ranges report an error. Other addresses return nullopt.
std::optional<std::uint32_t> mission_extra_actor_record_base(std::uint32_t address,std::uint32_t width);
// Bind after prepare. Keep state, count and delegated memory owners alive.
// Source, definitions, primary group0 and44A0C0 use delegated providers.
MissionActorInsertBindings mission_actor_insert_live_bindings(MissionTickState&,
    std::uint32_t& count_8cfa14,MissionActorInsertBindings delegated,
    std::function<std::uint32_t(std::uint32_t raw_definition)> definition_index);
} // namespace dfre
