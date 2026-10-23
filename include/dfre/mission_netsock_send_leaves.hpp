#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockSendLeafBindings=MissionNetsockPacketBindings;
//10005020 working frame:30 hex bytes. Sockaddr at10; return at20;
// caller data/length/IP at24/28/2C. Keep the current sockaddr bytes mapped.
std::uint32_t mission_netsock_encode_direct(const MissionNetsockSendLeafBindings&,std::uint32_t frame);
//10005120 working frame:40 hex bytes. Header bytes at18/19,sockaddr at1C;
// return at2C; caller data/length/IP/compressed flag at30/34/38/3C.
std::uint32_t mission_netsock_encode_extended(const MissionNetsockSendLeafBindings&,std::uint32_t frame);
//10005860 entry:10 hex bytes, data/length/IP at4/8/C. Current codec100096A8
// and actual htons/sendto/last-error services remain required when reached.
std::uint32_t mission_netsock_encode_select(const MissionNetsockSendLeafBindings&,std::uint32_t frame);
} // namespace dfre
