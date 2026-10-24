#pragma once
#include "dfre/mission_game_transport.hpp"
namespace dfre {
// Hex raw working frames:
//4B91C0=130(settings0..FF,caller-control100..10F,name110,unused114,
// capacity118,password11C,values120/124/128/12C).
//4B9300=A8(record0..87,caller-control88..97,name98,host9C,dataA0,sizeA4).
//Keep caller-control bytes explicit: the reached DLL Create copies98 bytes.
//4B92F0/4B93D0 use no local frame.
std::uint32_t mission_df_host_control_call(const MissionGameTransportBindings&,
    std::uint32_t entry,std::uint32_t frame=0);
} // namespace dfre
