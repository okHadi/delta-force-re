#pragma once
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionEventQueryBindings {
    // Read the current signed source count. The default is the current table size.
    std::function<std::int32_t(std::size_t group)> current_count;
    // Run before each reached source cell read. Keep the mission state alive.
    std::function<void(std::uint32_t source_address)> before_read;
};
struct MissionEventRect {
    std::int32_t side=0,min_x=0,min_y=0,max_x=0,max_y=0;
};
// 460330. Bounds are inclusive.
std::uint32_t mission_event_rect(MissionTickState&,const MissionEventRect&,
    const MissionEventQueryBindings& = {});
// 460560: compare signed actor +72 words across a current raw +90 link.
std::uint32_t mission_event_linked_sides(MissionTickState&,std::int32_t,std::int32_t,
    const MissionEventQueryBindings& = {});
}
