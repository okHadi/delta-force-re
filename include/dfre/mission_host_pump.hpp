#pragma once
#include <cstdint>
#include <array>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dfre {
class MissionHostPumpError : public std::runtime_error {
public: using std::runtime_error::runtime_error;
};
struct MissionHostMessage { std::uint32_t kind=0; std::uintptr_t token=0; };
struct MissionHostRead { bool success=false; std::uint32_t count=0; };
struct MissionHostPumpBindings {
    // Addresses name source cells; the host supplies current shared storage.
    std::function<std::uint32_t(std::uint32_t)> word;
    std::function<void(std::uint32_t,std::uint32_t)> store;
    std::function<std::uint8_t(std::uint32_t)> byte;
    std::function<void(std::uint32_t,std::uint8_t)> store_byte;
    std::function<std::optional<MissionHostMessage>()> peek_remove;
    std::function<void(const MissionHostMessage&)> translate,dispatch;
    // Source service or callback address and source-order arguments.
    // 493CD0 CD play; 4B5243 PCM queue (ESI buffer, ECX count, EBX zero);
    // 49EFD0 MIDI stop; 49EF20 MIDI restart.
    std::function<std::uint32_t(std::uint32_t,const std::vector<std::uint32_t>&)> call;
    // ReadFile writes up to capacity bytes at buffer through shared byte storage.
    std::function<MissionHostRead(std::uint32_t file,std::uint32_t buffer,std::uint32_t capacity)> read_file;
    std::function<void(std::uint32_t file,std::uint32_t absolute)> seek_file;
    // timeGetDevCaps output min/max; failed calls require defined output bytes.
    std::function<std::array<std::uint32_t,2>()> timer_caps;
};
// False means WM_QUIT reached; no audio upkeep runs on that exit.
bool mission_host_pump(const MissionHostPumpBindings&);
// Use after a platform message drain that did not receive quit.
void mission_host_upkeep(const MissionHostPumpBindings&);
void mission_host_cd_upkeep(const MissionHostPumpBindings&);
void mission_host_pcm_upkeep(const MissionHostPumpBindings&);
void mission_host_midi_upkeep(const MissionHostPumpBindings&);
// Exact 4906A0 callback body. Timer delivery is a host responsibility.
void mission_host_clock_callback(const MissionHostPumpBindings&);
// 490750: calls imported timeBeginPeriod (slot4C658C) and timeSetEvent
// (slot4C65A0) through call. Timer handle zero is retained as in the source.
void mission_host_clock_configure(const MissionHostPumpBindings&);
// Deliver callbacks from a real wrapping millisecond clock. Set period to the
// installed timer interval (CC0868, or 25 when it is zero). First sample anchors.
// This delivery policy preserves elapsed periods; it does not model WinMM jitter.
class MissionHostClockDelivery {
public:
    explicit MissionHostClockDelivery(std::uint32_t period_ms);
    std::uint32_t sample(std::uint32_t now_ms,const MissionHostPumpBindings&);
    void reset() noexcept;
private:
    std::uint32_t period_,last_=0,remainder_=0;
    bool anchored_=false;
};
} // namespace dfre
