#pragma once
#include "dfre/mission_game_transport.hpp"
namespace dfre {
//497370:8-byte raw frame(selector+4).4B8F20:8-byte frame(descriptor+4).
//4974A0/4B8F40 use no local frame. Selection and cleanup expose state only
//and return0. The two TCP/IP parents preserve their defined source status.
std::uint32_t mission_df_network_select_call(const MissionGameTransportBindings&,
    std::uint32_t entry,std::uint32_t frame=0);
} // namespace dfre
