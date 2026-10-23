#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockHostSetupBindings=MissionNetsockPacketBindings;
std::uint32_t mission_netsock_host_session(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //100012E0 entry8,session+4
std::uint32_t mission_netsock_host_address(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //10004050 entryC,IP/port+4/8
void mission_netsock_host_address_init(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //10004020 entry10,row/IP/port+4/8/C
std::uint32_t mission_netsock_host_start(const MissionNetsockHostSetupBindings&); //10004A40
void mission_netsock_usage_initialize(const MissionNetsockHostSetupBindings&); //10002510
std::uint32_t mission_netsock_usage_config(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //10002330 working218,FILE10,int14,line18,token118
std::uint32_t mission_netsock_usage_integer(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //10002240 entry14,line/number/key/out+4/8/C/10
std::uint32_t mission_netsock_usage_token(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //10002290 entry14,same arguments
void mission_netsock_usage_appid(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //10002310 entryC,destination/source+4/8
void mission_netsock_host_drop_seed(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //100021D0 working104,line4
void mission_netsock_usage_tick(const MissionNetsockHostSetupBindings&); //100028C0
std::uint32_t mission_netsock_usage_grow(const MissionNetsockHostSetupBindings&); //100025A0
void mission_netsock_usage_build(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //10002710 working78,found10,key14,prior24,reason74
void mission_netsock_usage_send(const MissionNetsockHostSetupBindings&,std::uint32_t frame); //10002620 working48,packet8,row40,retry44
// Sizes/offsets are hex. Keep one current DLL image and delegated allocator.
// Map all incoming scratch bytes, including usage_build key+E/+F. Its sprintf
// writes14 bytes and the source compares/copies16 bytes. Preserve OS/thread
// calls, source plain flags and current time. Host owns launch, stop and join.
} // namespace dfre
