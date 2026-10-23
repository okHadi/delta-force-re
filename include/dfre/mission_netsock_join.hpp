#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockJoinBindings=MissionNetsockPacketBindings;
// Select the thirteen source entries listed in the module document.
// Raw entry arguments start at+4. Sizes/offsets are hex.
//1420/38F0/3770:8 bytes.3840/37F0/3630/3540:C bytes.35A0:10 bytes.
//3960:logicalC bytes,IP0,portDWORD4,length8; preserve incoming bytes.
//14C0/57F0/3A10/3570 are frameless and use frame0.
// Keep one current DLL/table/socket/allocator owner and external thread joins.
std::uint32_t mission_netsock_join_call(const MissionNetsockJoinBindings&,
    std::uint32_t source_entry,std::uint32_t frame=0);
} // namespace dfre
