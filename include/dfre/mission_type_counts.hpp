#pragma once
#include "dfre/mission_live.hpp"
namespace dfre {
struct MissionTypeCountBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<void(std::uint32_t address,std::uint32_t width,std::uint32_t value)> write;
};
//44A200. Update current +2C counts and wrapped +38 frame ages.
void mission_type_counts(const MissionTypeCountBindings&);
MissionTypeCountBindings mission_type_count_bindings(MissionTickState&,AiTypeSlotTable&);
}
