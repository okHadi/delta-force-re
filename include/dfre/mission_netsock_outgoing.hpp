#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockOutgoingBindings=MissionNetsockPacketBindings;
//100048C0 logical12-byte local frame:portDWORD+0,IP+4,length+8.
void mission_netsock_outgoing(const MissionNetsockOutgoingBindings&,std::uint32_t frame);
//10003DD0/10005240/100058D0 entries:14 hex bytes.
// data/length/IP/portDWORD are at+4/+8/+C/+10. The port wrappers read WORD+10.
std::uint32_t mission_netsock_send_select(const MissionNetsockOutgoingBindings&,std::uint32_t frame);
std::uint32_t mission_netsock_send_direct(const MissionNetsockOutgoingBindings&,std::uint32_t frame);
std::uint32_t mission_netsock_send_compressed(const MissionNetsockOutgoingBindings&,std::uint32_t frame);
// Bind the actual10005020 and10005860 leaf sends when reached. Keep their
// socket, work buffer, codec and OS import state with the same current owner.
} // namespace dfre
