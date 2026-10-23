#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockReceiverBindings=MissionNetsockPacketBindings;
//100052E0 working frame:48 hex bytes, including saved registers/return cell.
// Caller destination/capacity/IPout/portout/lengthout are at34/38/3C/40/44.
// recvfrom uses sockaddr at20 and its length word at1C. Keep all bytes current.
std::uint32_t mission_netsock_receiver(const MissionNetsockReceiverBindings&,
    std::uint32_t frame);
//10005280 raw entry:IP at+4; map8 bytes. Nested frame provider uses this layout.
std::uint32_t mission_netsock_ip_filter(const MissionNetsockReceiverBindings&,
    std::uint32_t frame);
//10005830 raw four-byte local, filled by required FIONREAD ioctl.
std::uint32_t mission_netsock_receive_poll(const MissionNetsockReceiverBindings&,
    std::uint32_t frame);
} // namespace dfre
