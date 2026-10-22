#pragma once

#include "dfre/input_config.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace dfre {

// Live command table. The original keeps it at 0x4fb2fc: 121 rows of 0x2c
// bytes with id, name, key (+0x24), mouse (+0x26), joystick (+0x28) and flags
// (+0x2a). Rows with flags bit 2 are not player commands.
struct CommandBinding {
    std::uint32_t id=0;
    std::string_view name;
    std::uint16_t key=0,mouse=0,joystick=0,flags=0;
};

// 0x41ce20: DFDK.KEY action row -> command name. Action rows sit at profile
// offset 0x6a0 + action*12 in the order of the shipped DFDK.KEY file.
struct ProfileCommandPair {
    std::size_t action=0;
    std::string_view command;
};
const std::vector<ProfileCommandPair>& profile_command_map();

class CommandTable {
public:
    CommandTable();
    const std::vector<CommandBinding>& rows() const noexcept{return rows_;}
    CommandBinding& at(std::size_t index){return rows_.at(index);}
    const CommandBinding& at(std::size_t index) const{return rows_.at(index);}
    // 0x41cd30 strcasecmp scan; returns npos when the name is absent.
    std::size_t find_by_name(std::string_view name) const;
    const CommandBinding* find_by_id(std::uint32_t id) const;
    // 0x41cd70: zeroes key and mouse, then writes the key word and maps the
    // mouse button 1|2|3 to the table masks 1|2|0x10.
    void set_binding(std::size_t index,std::uint32_t key,std::uint32_t mouse);
    // 0x41ce20 with the shipped 47 pairs; rows missing from the table are skipped.
    void apply_profile_bindings(const ActionBindings& bindings);
    // 0x47a300: indices of rows whose flags bit 2 is clear, in table order.
    std::vector<std::uint32_t> active_command_indices() const;

private:
    std::vector<CommandBinding> rows_;
};

// 0x479fa0/0x47a000/0x47a070: the 0xbd4850 action queue, 32 records of
// {id, tick, value, extra}. Records are due when tick <= query tick; a query
// tick of 0 reports every record.
struct InputRecord {
    std::uint32_t id=0,tick=0,value=0,extra=0;
};
inline bool operator==(const InputRecord& a,const InputRecord& b){
    return a.id==b.id&&a.tick==b.tick&&a.value==b.value&&a.extra==b.extra;
}

class InputQueue {
public:
    static constexpr std::size_t capacity=32;
    bool push(const InputRecord& record); // drops the record when full
    bool pop(InputRecord& record);
    std::size_t due_count(std::uint32_t tick) const;
    std::size_t size() const noexcept{return count_;}
    const InputRecord* records() const noexcept{return records_.data();}
    void clear() noexcept{count_=0;}
private:
    std::array<InputRecord,capacity> records_{};
    std::size_t count_=0;
};

// Key event from the 0x498a70 ring: key, byte1, byte2 (0x80 when the key is
// extended) and byte3 modifiers (shift 1, ctrl 4).
struct InputKeyEvent {
    std::uint8_t key=0,state=0,flag=0,modifiers=0;
};

// Flags and keys read by the 0x4790a0 menu/chat filter.
struct GlobalKeyState {
    std::uint8_t type_flag=0;    // 0x67db6c
    std::uint8_t digit_flag=0;   // 0x67db90
    std::uint8_t page_flag=0;    // 0x67db7c
    std::uint8_t menu_flag=0;    // 0x67dbc0
    std::uint8_t hud_menu=0;     // 0x67f520
    std::uint8_t hud_menu2=0;    // 0x67f53c
    std::uint16_t type_key_a=0;  // 0xbd4820, default 0x59 'Y'
    std::uint16_t type_key_b=0;  // 0xbd4824, default 0x4e 'N'
    std::uint32_t actor_flags=0; // [[0xc23820]+0x6c]
};

enum class GlobalKeyEffect : std::uint8_t {
    none,dispatch_exit,clear_typing,digit,flight_page,menu_page,enter_flag,dispatch_space
};

struct GlobalKeyResult {
    bool handled=false;
    GlobalKeyEffect effect=GlobalKeyEffect::none;
    std::uint32_t argument=0;      // 4717e0 digit, or 0/1 page direction for 42ad50/429700
    InputRecord pushed;            // 0x479fa0(0xd, 0, 0, 0) when pushed_record
    bool pushed_record=false;      // queued, not dispatched
    bool dispatched=false;         // 477d40(id, 0, 0) called directly
    std::uint32_t dispatched_id=0;
};

// 0x479630 mouse axes: raw 0xd0934c/0xd09350 values, the 0x67f500 scale and
// the 0x67f4fc flip flag; steady is the 0x484740 result.
struct InputMouseLook {
    std::int32_t x=0,y=0,scale=0;
    bool flip=false,steady=false;
};

// One frame of inputs for 0x479750 plus the live half of 0x479450.
struct FrameInput {
    std::uint32_t tick=0;
    std::array<std::uint8_t,256> keys{};
    std::uint32_t mouse_buttons=0;    // 0xd09348
    std::uint32_t joystick_buttons=0; // 0xd09944
    InputMouseLook look;
    std::vector<InputKeyEvent> events;   // drained in ring order
    std::uint8_t input_mode=0;        // 0xbd4f78: 0 game, 1 chat, other none
    bool live_input=true;             // 0xbd482c == 0
    bool mouse_enabled=false;         // 0x503130 == 0 && 0xcc0884 != 0
    std::uint16_t priority_key=0;     // 0x4fcbde >> 16, zero in this build
    GlobalKeyState global;
    // Optional live sources. Read each key and sample stage from current cells.
    std::function<FrameInput()> current;
    std::function<InputKeyEvent()> next_event; // zero word ends the ring drain
    std::function<void(const GlobalKeyResult&)> on_global_key;
    std::function<void(const InputKeyEvent&)> on_chat_key;
    std::function<void()> on_mouse_sampled;
};

struct FrameOutput {
    std::vector<InputRecord> queued;      // 0x479fa0 pushes in call order
    std::vector<InputRecord> immediate;   // 477d40 calls made outside the queue
    std::vector<GlobalKeyEffect> effects; // filter side effects in order
    std::vector<std::uint32_t> digits;    // 4717e0 arguments
    std::vector<std::int32_t> pages;      // 42ad50 / 429700 arguments
    std::vector<InputKeyEvent> chat_keys;    // 0x479a80 chat key stream
    bool enter_flag_set=false;            // 0x67dbbc set by the menu filter
};

class InputStage {
public:
    InputStage();
    CommandTable& table() noexcept{return table_;}
    const CommandTable& table() const noexcept{return table_;}
    const std::vector<std::uint32_t>& active_commands() const noexcept{return active_;}
    // 0x47a300 list build plus the 0x477d00 queue reset.
    void activate();
    const InputQueue& queue() const noexcept{return queue_;}
    bool focus_flag() const noexcept{return focus_flag_;} // 0xc23a10

    // 0x47a150 mouse message callback; event codes come from 0x497a40
    // (2 left, 8 right, 0x20 middle, 0x100 and 0x200 wheel-ish pulses).
    bool push_mouse_pulse(std::uint32_t event,std::uint8_t menu_flag=0,std::uint32_t actor_flags=0);
    // 0x479380 single key event; returns the number of pushed records.
    std::size_t push_key_event(const InputKeyEvent& event,std::uint32_t tick);
    // 0x4790a0 menu/chat filter.
    GlobalKeyResult global_key(std::uint16_t key,const GlobalKeyState& state);
    // 0x479630 mouse axes and mouse-button commands.
    std::size_t push_mouse(const InputMouseLook& look,std::uint32_t mouse_buttons,std::uint32_t tick);
    // 0x4795a0 held keys and the 0xc23a10 focus flag.
    std::size_t push_held_keys(const std::array<std::uint8_t,256>& keys,std::uint32_t tick,
                               std::uint16_t priority_key=0);
    // 0x47a290: command active through key state, mouse bits or joystick bits.
    bool command_active(std::size_t index,const std::array<std::uint8_t,256>& keys,
                        std::uint32_t mouse_buttons,std::uint32_t joystick_buttons) const;

    // 0x479750 and the live half of 0x479450.
    FrameOutput begin_frame(const FrameInput& input);
    std::vector<InputRecord> take_due(std::uint32_t tick);

private:
    CommandTable table_;
    std::vector<std::uint32_t> active_;
    InputQueue queue_;
    bool focus_flag_=false;
};

} // namespace dfre
