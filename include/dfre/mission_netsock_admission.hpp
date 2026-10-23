#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockAdmissionBindings=MissionNetsockPacketBindings;
// 3F00/40C0: entry10 hex, packet4/IP8/portDWORDC.
// 3ED0/4090: entryC, IP4/portDWORD8.
// 3F80: entry14, statusWORD4/reasonWORD8/IPC/portDWORD10.
// Return4090's current address pointer; other bodies have no return contract.
std::uint32_t mission_netsock_admission_call(const MissionNetsockAdmissionBindings&,
    std::uint32_t entry,std::uint32_t frame);
} // namespace dfre
