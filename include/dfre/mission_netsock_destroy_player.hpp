#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockDestroyPlayerBindings=MissionNetsockPacketBindings;
// Hex raw frames:1B10/1B40/1B80=8(peer4);4510/45C0=10(status4,IP8,portC);
//4550/4660=10(packet4,IP8,portC). Keep source wait cells in current memory.
std::uint32_t mission_netsock_destroy_player_call(const MissionNetsockDestroyPlayerBindings&,
    std::uint32_t entry,std::uint32_t frame);
} // namespace dfre
