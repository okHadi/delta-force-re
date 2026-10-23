#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockDispatchBindings=MissionNetsockPacketBindings;
//100046C0 entry:C hex bytes; sourceIP/portDWORD at+4/+8.
void mission_netsock_dispatch(const MissionNetsockDispatchBindings&,std::uint32_t frame);
//10004420 entry:10 hex bytes; current packet/IP/portDWORD at+4/+8/+C.
void mission_netsock_data_gateway(const MissionNetsockDispatchBindings&,std::uint32_t frame);
//10003FE0/100041D0 entries:C hex bytes; IP/portDWORD at+4/+8.
std::uint32_t mission_netsock_address_lookup(const MissionNetsockDispatchBindings&,std::uint32_t frame);
std::uint32_t mission_netsock_player_lookup(const MissionNetsockDispatchBindings&,std::uint32_t frame);
//10004220 entry:14 hex bytes; data/length/sender/priority at+4/+8/+C/+10.
void mission_netsock_receive_enqueue(const MissionNetsockDispatchBindings&,std::uint32_t frame);
} // namespace dfre
