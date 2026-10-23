#pragma once
#include "dfre/mission_live.hpp"
namespace dfre {
struct MissionEventActorOrderBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
};
//45C5E0. Return a list-entry index. Equal distances select the later entry.
std::uint32_t mission_event_closest_marker(const MissionEventActorOrderBindings&,
    std::uint32_t source_actor,std::uint32_t list);
//45F670. Set orders on current group3, then group0 records.
void mission_event_actor_orders(const MissionEventActorOrderBindings&,
    std::int32_t side,std::uint32_t list);
// Bind current counted actors, person records and contiguous marker-list words.
MissionEventActorOrderBindings mission_event_actor_order_bindings(MissionTickState&);
}
