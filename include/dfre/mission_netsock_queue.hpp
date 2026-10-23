#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
struct MissionNetsockQueueBindings : MissionNetsockPacketMemoryBindings {
    std::function<std::uint32_t(std::uint32_t target,const std::vector<std::uint32_t>&)> call;
    std::function<void(std::uint32_t target,const std::vector<std::uint32_t>&)> before_call;
};
// Raw entry frames include the return cell. Bind all dynamic bytes through the
// same canonical memory backend as MissionNetsockPacketMemory and queue pop.
std::uint32_t mission_netsock_queue_initialize(const MissionNetsockQueueBindings&,
    std::uint32_t frame); //10003A60,queue at+4; map8 bytes
void mission_netsock_queue_enqueue(const MissionNetsockQueueBindings&,
    std::uint32_t frame); //10003B20,data/length/queue/address/port/priority at4..18; map1C
void mission_netsock_queue_destroy(const MissionNetsockQueueBindings&,
    std::uint32_t frame); //10003AD0,queue at+4; map8 bytes
} // namespace dfre
