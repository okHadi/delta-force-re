#pragma once
#include "dfre/mission_event_person.hpp"
namespace dfre {
// Keep state and foreign owners alive. Resolve active definition index0 explicitly.
MissionEventPersonBindings mission_event_live_bindings(MissionTickState&,
    std::function<std::uint32_t(std::uint32_t,std::uint32_t)> foreign_read,
    std::function<void(std::uint32_t,std::uint32_t,std::uint32_t)> foreign_write,
    std::function<std::uint32_t(std::size_t)> definition_address,
    std::function<std::uint16_t()> fp_control);
}
