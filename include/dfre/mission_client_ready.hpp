#pragma once
#include "dfre/mission_client_added.hpp"
namespace dfre {
//446A30:128 hex mapped bytes; locals10C,slot120,packet124.
//The source clears only E2 local bytes. Keep all caller bytes current.
void mission_client_ready(const MissionClientAddedBindings&,
    std::uint32_t frame,std::uint32_t bytes);
//447E80: one current source client and its current inventory tables.
void mission_client_resupply(const MissionClientAddedBindings&,std::uint32_t client);
//447FB0:4C hex mapped bytes; local text40,slot48.
void mission_client_reject(const MissionClientAddedBindings&,
    std::uint32_t frame,std::uint32_t bytes);
} // namespace dfre
