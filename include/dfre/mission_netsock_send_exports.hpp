#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockSendExportBindings=MissionNetsockPacketBindings;
// Raw cdecl frames include the return cell. Sizes/offsets are hex.
//10001CF0:10 bytes; data/length/peer at4/8/C.
std::uint32_t mission_netsock_send_to_client(const MissionNetsockSendExportBindings&,std::uint32_t frame);
//10001DC0:C bytes; data/length at4/8.
std::uint32_t mission_netsock_send_to_server(const MissionNetsockSendExportBindings&,std::uint32_t frame);
//100019E0:8 bytes; peer at4. Keep current count/base/active reads.
std::uint32_t mission_netsock_send_player_lookup(const MissionNetsockSendExportBindings&,std::uint32_t frame);
// Share current packet scratch19C60, all queues and the canonical allocator.
// Keep source forward copies, wrapped WORD lengths and current tail bytes.
} // namespace dfre
