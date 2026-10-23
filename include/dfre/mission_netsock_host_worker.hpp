#pragma once
#include "dfre/mission_netsock_receiver.hpp"
namespace dfre {
using MissionNetsockHostWorkerBindings=MissionNetsockPacketBindings;
//10004930 full blocking thread entry. Map its current 12-byte logical locals:
// portDWORD+0,IP+4,length+8. Only port+0 is initially cleared by the source.
// Use the same current memory/allocator/frame provider as receiver and queues.
// The production _endthread delegate ends its OS thread. A controlled fixture
// may return; this function then returns that delegate's register value.
std::uint32_t mission_netsock_host_worker(const MissionNetsockHostWorkerBindings&,
    std::uint32_t frame);
} // namespace dfre
