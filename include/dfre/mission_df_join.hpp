#pragma once
#include "dfre/mission_game_transport.hpp"
namespace dfre {
// Source4B9090 uses a current0x118-byte frame: settings0..FF,
// selected-session pointer110, password pointer114. Preserve current aliases.
std::uint32_t mission_df_join(const MissionGameTransportBindings&,
                             std::uint32_t frame);
// Source4B9080 tail thunk. Bind current4C5544 StopEnum explicitly.
std::uint32_t mission_df_join_stop_enum(const MissionGameTransportBindings&);
} // namespace dfre
