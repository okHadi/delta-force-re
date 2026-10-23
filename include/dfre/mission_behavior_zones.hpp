#pragma once
#include "dfre/actor_queries.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
namespace dfre {
struct MissionBehaviorZoneBindings {
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> read;
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> write;
};
void mission_behavior_zones_reset(const MissionBehaviorZoneBindings&); //45FD80
bool mission_behavior_zone_add(const MissionBehaviorZoneBindings&,std::uint32_t x,
    std::uint32_t y,std::uint32_t z,std::uint32_t radius,std::uint32_t type,std::uint32_t flags); //45FCF0
struct MissionBehaviorZones {
    std::uint32_t count = 0;
    std::array<std::array<std::uint32_t,8>,200> records{};
    MissionBehaviorZoneBindings bindings(std::uint32_t& host_gate);
    std::vector<ActorZoneRect> active_records() const;
};
}
