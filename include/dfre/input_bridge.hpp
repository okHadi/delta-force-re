#pragma once

#include "dfre/input_state.hpp"
#include "dfre/platform.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace dfre {

// Caller-provided source state for one frame.
struct InputSourceFlags {
    std::uint8_t input_mode=0;        // 0xbd4f78: 0 game, 1 chat, other none
    bool live_input=true;             // 0xbd482c == 0 samples live input
    bool windowed=false;              // 0x503130 != 0 stops mouse sampling
    bool window_active=false;         // 0xcc0884 != 0 (WM_SETFOCUS / WM_KILLFOCUS)
    std::uint16_t priority_key=0;     // 0x4fcbde >> 16
    std::uint32_t joystick_buttons=0; // 0xd09944
    std::int32_t mouse_scale=0;       // 0x67f500
    bool mouse_flip=false;            // 0x67f4fc != 0
    bool steady=false;                // 0x484740() != 0
    GlobalKeyState global;            // 0x4790a0 flags and [[0xc23820]+0x6c]
    bool keyboard_ring_blocked=false; // 0xd096bc !=0 stops ring writes, not VK writes
};

// One frame of the original 0x479750 + live 0x479450 path.
struct BridgeFrame {
    std::vector<InputRecord> commands;   // ordered 477d40 argument records
    std::vector<InputRecord> immediate;  // 477d40 calls made outside the queue
    std::vector<GlobalKeyEffect> effects; // 0x4790a0 side effects in order
    std::vector<std::uint32_t> digits;    // 4717e0 arguments
    std::vector<std::int32_t> pages;      // 42ad50 / 429700 arguments
    std::vector<InputKeyEvent> chat_keys; // 0x479a80 chat key stream
    bool enter_flag_set=false;            // 0x67dbbc set by the menu filter
    bool focus_flag=false;                // 0xc23a10
};

// Persistent join between the GameWindow InputEvent stream (Windows VK values,
// extended flag, buttons 1 left / 2 right / 3 middle, pointer_delta and
// focus_lost) and the original input stage. The Game owns one instance, feeds
// every poll and calls frame(tick) once per update with the current source
// flags. See docs/input-bridge.md.
class InputBridge {
public:
    // Applies the profile DFDK.KEY bindings (0x41ce20) and builds the polled
    // command list (0x47a300). The profile comes from the caller; nothing is
    // assumed here.
    explicit InputBridge(const ActionBindings& profile_bindings);
    // Explicitly the shipped DFDK.KEY rows of Dfbase.pff, for tests and probes.
    static InputBridge with_shipped_profile();

    void apply_profile(const ActionBindings& profile_bindings);

    // 0x498ae0 key press (sets the VK state, then queues the ring event with the
    // modifier snapshot), 0x498a60 key release, 0x497a40 mouse button pulse,
    // 0x490367 focus loss.
    void feed(const InputEvent& event);
    void feed(const std::vector<InputEvent>& events);
    void feed_character(std::uint8_t character); // 498B00 writes byte1 of the last ring slot
    void focus_lost();
    // 47A290 accepts a table row index. Supply loaded rows outside the profile
    // table through this provider; command IDs are separate values.
    void set_query_row_source(std::function<std::optional<CommandBinding>(std::size_t)> source){query_row_=std::move(source);}
    bool query_active(std::size_t row_index) const;
    void set_live_sources(std::function<InputSourceFlags()> source,
        std::function<void(const GlobalKeyResult&)> global_effect={},
        std::function<void(const InputKeyEvent&)> chat={});

    void set_source_flags(const InputSourceFlags& flags){flags_=flags;}
    const InputSourceFlags& source_flags() const noexcept{return flags_;}

    // 0x479750 + the live half of 0x479450. Consumes the queued ring events and
    // pointer deltas for this tick. The one-argument form uses the stored flags;
    // the two-argument form takes the frame's flags from the caller.
    BridgeFrame frame(std::uint32_t tick);
    BridgeFrame frame(std::uint32_t tick,const InputSourceFlags& flags);

    const InputStage& stage() const noexcept{return stage_;}
    InputStage& stage() noexcept{return stage_;}
    const CommandTable& command_table() const noexcept{return stage_.table();}
    const std::array<std::uint8_t,256>& key_state() const noexcept{return keys_;}
    std::uint32_t mouse_buttons() const noexcept{return mouse_mask_;}
    std::int32_t pending_delta_x() const noexcept{return delta_x_;}
    std::int32_t pending_delta_y() const noexcept{return delta_y_;}
    bool focus_flag() const noexcept{return stage_.focus_flag();}
    bool quit_requested() const noexcept{return quit_;}
    int pointer_x() const noexcept{return pointer_x_;}
    int pointer_y() const noexcept{return pointer_y_;}

    // Original masks for the table: 1 left, 2 right, 0x10 middle.
    static std::uint32_t mouse_mask(unsigned button);
    // Original 0x497a40 callback codes: 2 left, 8 right, 0x20 middle, 0 none.
    static std::uint32_t mouse_pulse(unsigned button);

private:
    void queue_key_press(int key,bool extended);
    InputStage stage_;
    std::array<std::uint8_t,256> keys_{};
    std::uint32_t mouse_mask_=0;
    std::int32_t delta_x_=0,delta_y_=0;
    int pointer_x_=0,pointer_y_=0;
    std::array<InputKeyEvent,128> ring_{};
    std::size_t ring_read_=0,ring_write_=0;
    std::optional<std::size_t> ring_last_;
    std::function<std::optional<CommandBinding>(std::size_t)> query_row_;
    std::function<InputSourceFlags()> sources_;
    std::function<void(const GlobalKeyResult&)> global_effect_;
    std::function<void(const InputKeyEvent&)> chat_;
    InputSourceFlags flags_;
    bool quit_=false;
};

// Optional, API-stable adapter: hand the ordered records to any consumer. The
// Game passes a lambda that calls the ported dispatcher, for example
// dispatch_commands(frame.commands,[&](const InputRecord& record){
//     dispatch_game_command(record.id,std::int32_t(record.value),globals,actor,bridge.command_table());
// });
template <class Apply>
void dispatch_commands(const std::vector<InputRecord>& commands,Apply&& apply){
    for(const auto& record:commands)apply(record);
}

} // namespace dfre
