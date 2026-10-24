#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockCreatePlayerBindings=MissionNetsockPacketBindings;
// Hex working frames:14F0=8(outID4);1530=1C(localID0,outID8,recordC,
// flags10,remote14,address18);1630/1700/1780=10(outID4,record8,flagsC).
// 1660=1C(saved-prefix0..3,line4..13,record18);4190=14(IP4,port8,statusC,ID10);
// 4250=8(ID4);4280=10(epoch0,player8,IPC);4340=10(packet4,IP8,portC).
// 4340 reuses portC as an output ID. Keep1660's incoming prefix explicit.
std::uint32_t mission_netsock_create_player_call(const MissionNetsockCreatePlayerBindings&,
    std::uint32_t entry,std::uint32_t frame);
} // namespace dfre
