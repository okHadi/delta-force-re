#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockClientWorkerBindings=MissionNetsockPacketBindings;
// Select only the twenty source entries listed in the module document.
// All sizes and offsets below are hex. Entry arguments start at+4.
// 3300: logical C bytes, portWORD+2,IP+4,length+8. Preserve incoming bytes.
// 1FE0: C bytes,IP+4,portWORD+8. 3290: C locals,port0,IP4,length8.
// 2E40:14 bytes;2E70:C bytes;3260 and other one-argument bodies:8 bytes.
// Frameless bodies use frame0. The host owns memory, OS threads and join.
std::uint32_t mission_netsock_client_worker_call(const MissionNetsockClientWorkerBindings&,
    std::uint32_t source_entry,std::uint32_t frame=0);
} // namespace dfre
