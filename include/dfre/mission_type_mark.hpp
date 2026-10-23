#pragma once
#include "dfre/mission_type_counts.hpp"
namespace dfre {
//44A3E0. The caller supplies the source scalar index bits.
void mission_type_mark(const MissionTypeCountBindings&,std::uint32_t index);
}
