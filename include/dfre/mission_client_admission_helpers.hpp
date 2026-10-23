#pragma once
#include "dfre/mission_client_added.hpp"
namespace dfre {
void mission_client_clear_references(const MissionClientAddedBindings&,std::uint32_t client); //442450
std::uint32_t mission_client_name_message(const MissionClientAddedBindings&,
    std::uint32_t format,std::uint32_t name); //43F770; returns current78EE54
std::uint32_t mission_client_peer_name(const MissionClientAddedBindings&,
    std::uint32_t peer); //497870; required current table+6C call
} // namespace dfre
