#pragma once
#include "dfre/mission_sound_runtime.hpp"
#include "dfre/mission_tick.hpp"
namespace dfre {
// Keep state alive. The factory can run before init_mission_tick.
// Bind completed and service on the returned object when their bodies are needed.
MissionSoundRuntimeBindings mission_sound_live_bindings(MissionTickState&,
    std::function<std::uint32_t()> script_key);
} // namespace dfre
