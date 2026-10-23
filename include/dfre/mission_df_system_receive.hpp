#pragma once
#include "dfre/mission_game_transport.hpp"
namespace dfre {
// Source4B9400: map0x9C current bytes. Local name record0..87; packet pointer98.
// Keep incoming local bytes and the current packet allocation live through calls.
void mission_df_system_receive(const MissionGameTransportBindings&,
                               std::uint32_t frame);
} // namespace dfre
