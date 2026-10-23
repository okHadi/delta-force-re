#pragma once
#include "dfre/mission_netsock_packets.hpp"
namespace dfre {
using MissionNetsockCloseBindings = MissionNetsockPacketBindings;
// Raw source calls use one current DLL memory and required OS/thread services.
// 18D0 frame: return, config, unused, callback, userdata, flags (0x18 bytes).
// 34A0 frame: return, IP (8 bytes). 3680 logical frame: IP, port, length (12).
// Other entries do not use a caller frame. release must not throw.
std::uint32_t mission_netsock_close_call(const MissionNetsockCloseBindings&,
                                       std::uint32_t entry,
                                       std::uint32_t frame = 0);
} // namespace dfre
