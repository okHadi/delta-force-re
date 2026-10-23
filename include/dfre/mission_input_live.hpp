#pragma once
#include "dfre/input_bridge.hpp"
#include "dfre/mission_tick.hpp"

namespace dfre {
struct MissionInputLiveBindings {
    std::function<std::uint32_t(std::uint32_t address,std::uint32_t width)> read;
    std::function<bool()> window_active;
    std::function<void(std::uint32_t command)> dispatch;
    std::function<void(std::uint32_t digit)> digit;
    std::function<void(std::uint32_t direction)> flight_page;
    std::function<void(std::uint32_t direction)> menu_page;
    std::function<void(const InputKeyEvent&)> chat;
};
// Read current canonical cells each time InputBridge requests its source state.
InputSourceFlags mission_input_live_sources(const MissionTickState&,const MissionInputLiveBindings&);
// Call synchronously from InputBridge; returned effect lists are observations.
void mission_input_live_global(MissionTickState&,const MissionInputLiveBindings&,const GlobalKeyResult&);
void mission_input_live_chat(const MissionInputLiveBindings&,const InputKeyEvent&);
} // namespace dfre
