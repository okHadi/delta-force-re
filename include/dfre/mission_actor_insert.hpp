#pragma once
#include "dfre/ai.hpp"
#include <functional>

namespace dfre {
struct MissionActorInsertBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t)> write;
    std::function<std::uint32_t(std::uint32_t actor)> person; //44A0C0
};
// Source and destination memory stay current through each read, store and callback.
std::uint32_t mission_actor_insert(const MissionActorInsertBindings&,
    std::uint32_t source,std::uint32_t mode); //45D970
struct MissionTransientActorPool {
    std::array<AiActorImage,1024> records{}; //822E80 raw bytes; live typed records belong to AiActorTables::extra
    std::uint32_t count_8cfa14=0;
};
// Standalone raw memory. Live bindings also need the canonical typed actor owner.
// Keep the pool and all delegated memory owners alive through each call.
MissionActorInsertBindings mission_transient_actor_bindings(MissionTransientActorPool&,
    MissionActorInsertBindings delegated);
void mission_transient_actor_clear(MissionTransientActorPool&); //449620 pool clear
} // namespace dfre
