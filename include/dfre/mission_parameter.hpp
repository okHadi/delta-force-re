#pragma once
#include "dfre/mission_debrief.hpp"
namespace dfre {
// Use current source cells and the caller's parameter allocation. Each reached
// access requires a provider; the table initializer has a separate lifetime.
std::uint32_t mission_parameter_set(const MissionDebriefBindings&,
    std::uint32_t index,std::uint32_t value); //4C4F40 /4B69D0
std::uint32_t mission_parameter_get(const MissionDebriefBindings&,
    std::uint32_t index); //4C4F60 /4B69F4
}
