#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockPeriodicBindings=MissionNetsockPacketBindings;
void mission_netsock_periodic(const MissionNetsockPeriodicBindings&); //10004830
void mission_netsock_connection_usage(const MissionNetsockPeriodicBindings&,std::uint32_t frame); //10002D30 entry8,address+4
void mission_netsock_player_usage(const MissionNetsockPeriodicBindings&,std::uint32_t frame); //10002B80 working68,prior10,player60,reason64; source overwrites player argument after caching it
void mission_netsock_address_expire(const MissionNetsockPeriodicBindings&,std::uint32_t frame); //10004820 entry8,address+4
void mission_netsock_address_remove(const MissionNetsockPeriodicBindings&,std::uint32_t frame); //10004600 entry8,address+4
void mission_netsock_player_remove(const MissionNetsockPeriodicBindings&,std::uint32_t frame); //10004490 entry8,player+4
void mission_netsock_player_log(const MissionNetsockPeriodicBindings&,std::uint32_t frame); //100042E0 working14,time4,playerC,IP10
std::uint32_t mission_netsock_ip_text(const MissionNetsockPeriodicBindings&,std::uint32_t frame); //10005010 entry8,IP4
void mission_netsock_ping_timer(const MissionNetsockPeriodicBindings&,std::uint32_t frame); //100020D0 working10,packet0,nowC
void mission_netsock_client_close(const MissionNetsockPeriodicBindings&); //10001830
void mission_netsock_usage_free(const MissionNetsockPeriodicBindings&); //10002D90
std::uint32_t mission_netsock_close_session(const MissionNetsockPeriodicBindings&); //10001880
void mission_netsock_host_close(const MissionNetsockPeriodicBindings&); //100017F0
void mission_netsock_host_close_notices(const MissionNetsockPeriodicBindings&); //10003E20
void mission_netsock_remove_players(const MissionNetsockPeriodicBindings&); //10003E90
// Sizes/offsets are hex. Share current DLL memory, allocator and all queues.
// Host owns threads and frame lifetime. Client close waits for actual19C14 ack.
// Host close preserves source stop/free order; join remains outside these bodies.
// Normalize only close-notice10003E20->10005240 argument4 to consumed WORD.
// Ping port highbits remain the cached now high16. Keep all real DWORDs intact.
} // namespace dfre
