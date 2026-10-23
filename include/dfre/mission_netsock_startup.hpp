#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockStartupBindings=MissionNetsockPacketBindings;
std::uint32_t mission_netsock_startup_init(const MissionNetsockStartupBindings&); //10001000
std::uint32_t mission_netsock_startup_tcpip(const MissionNetsockStartupBindings&,std::uint32_t frame); //10001180 entry8,address+4
std::uint32_t mission_netsock_startup_socket(const MissionNetsockStartupBindings&,std::uint32_t frame); //10004D90 entry10,mode/portptr/sendWORD+4/8/C
std::uint32_t mission_netsock_startup_bind(const MissionNetsockStartupBindings&,std::uint32_t frame); //10004C80 working28,mode/portptr20/24,BOOL8,sockaddrC
std::uint32_t mission_netsock_startup_buffer(const MissionNetsockStartupBindings&); //10004C00
std::uint32_t mission_netsock_startup_get_send_buffer(const MissionNetsockStartupBindings&,std::uint32_t frame); //10004AF0 local8,len0/value4
std::uint32_t mission_netsock_startup_set_send_buffer(const MissionNetsockStartupBindings&,std::uint32_t frame); //10004B30 workingC,BOOL0,arg8
std::uint32_t mission_netsock_startup_get_receive_buffer(const MissionNetsockStartupBindings&,std::uint32_t frame); //10004B90 local8,len0/value4
std::uint32_t mission_netsock_startup_set_receive_buffer(const MissionNetsockStartupBindings&,std::uint32_t frame); //10004BD0 entry8,arg4
std::uint32_t mission_netsock_startup_resolve(const MissionNetsockStartupBindings&,std::uint32_t frame); //10004FA0 entry8,address4
std::uint32_t mission_netsock_startup_codec(const MissionNetsockStartupBindings&,std::uint32_t frame); //10005660 working22C,arg228,name8,finddata10C/name120
// Frame sizes/offsets above are hex. Keep current PacketMemory and its shared
// allocator alive. Imported services, enum stop and unknown codecs are required.
// Normalize only InitTCPIP->10004D90 argument3 and10004C80->htons argument1
// to their verified consumed WORD widths. Keep all real DWORD state unchanged.
} // namespace dfre
