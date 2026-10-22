#include "dfre/input_state.hpp"
#include "dfre/platform.hpp"
#include "dfre/movement.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void check_command_table() {
    dfre::CommandTable table;
    require(table.rows().size() == 121, "Command table row count");
    require(table.find_by_name("move_forward") != std::string_view::npos, "Name lookup");
    require(table.find_by_name("MOVE_FORWARD") != std::string_view::npos, "Name lookup is case insensitive");
    require(table.find_by_name("not_a_command") == std::string_view::npos, "Unknown name");

    const auto run = table.find_by_name("move_forward");
    table.set_binding(run, 200, 0);
    require(table.at(run).key == 200 && table.at(run).mouse == 0, "Key binding");
    table.set_binding(run, 0, 3);
    require(table.at(run).key == 0 && table.at(run).mouse == 0x10, "Middle mouse maps to 0x10");
    table.set_binding(run, 40, 2);
    require(table.at(run).key == 40 && table.at(run).mouse == 2, "Key and mouse together");
    table.set_binding(run, 300, 9);
    require(table.at(run).key == 300 && table.at(run).mouse == 0, "Unknown mouse value clears the mask");
    table.set_binding(9999, 1, 1);

    const auto profile = dfre::default_action_bindings();
    table.apply_profile_bindings(profile);
    require(table.at(table.find_by_name("move_forward")).key == 38, "Forward from the profile row");
    require(table.at(table.find_by_name("attack_1")).key == 17 && table.at(table.find_by_name("attack_1")).mouse == 1,
            "Fire keeps the Ctrl key and the left mouse bit");
    require(table.at(table.find_by_name("attack_2")).key == 17, "Fire also binds attack_2");
    require(table.at(table.find_by_name("Prone")).mouse == 0x10, "Prone middle button");
    require(table.at(table.find_by_name("scope")).key == 83 && table.at(table.find_by_name("scope")).mouse == 2,
            "Scope keeps S and right mouse");
    require(table.at(table.find_by_name("server")).key == 36, "Server Screen from the profile row");
    require(table.at(table.find_by_name("fullScreen")).key == 0x91, "Rows outside the map keep the shipped binding");
    require(table.at(table.find_by_name("NoRun")).key == 16, "NoRun is bound but stays out of the active list");

    auto mouse_only = dfre::default_action_bindings();
    mouse_only[2] = {0, 3, 0};
    table.apply_profile_bindings(mouse_only);
    require(table.at(table.find_by_name("move_forward")).key == 0, "Mouse-only row clears the key");
    require(table.at(table.find_by_name("move_forward")).mouse == 0x10, "Mouse-only row sets the middle bit");

    const auto active = table.active_command_indices();
    require(active.size() == 24, "Active command count");
    require(active.front() == 75, "First polled command is move_back");
    for (const auto index : active) require((table.at(index).flags & 4) != 0, "Active rows all carry flags bit 2");
    require(table.find_by_id(143) != nullptr && table.find_by_id(9999) == nullptr, "Id lookup");

    const auto& pairs = dfre::profile_command_map();
    require(pairs.size() == 47, "Profile command pair count");
    require(pairs[0].action == 0 && pairs[0].command == "slide_left", "First profile pair");
    require(pairs[8].action == 8 && pairs[8].command == "attack_1" && pairs[9].command == "attack_2",
            "Fire maps to both attack commands");
    require(pairs.back().action == 45 && pairs.back().command == "server", "Last profile pair");
}

void check_queue() {
    dfre::InputQueue queue;
    require(queue.size() == 0 && queue.due_count(5) == 0, "Empty queue");
    for (std::uint32_t i = 0; i < 40; ++i) queue.push({i, 10 + i, i, 0});
    require(queue.size() == dfre::InputQueue::capacity, "Queue caps at 32 records");
    require(queue.due_count(0) == 32, "Tick zero reports every record");
    require(queue.due_count(9) == 0, "No record is due before its tick");
    require(queue.due_count(12) == 3, "Leading records are due");
    require(queue.due_count(100) == 32, "All records are due at a high tick");
    dfre::InputRecord record;
    require(queue.pop(record) && record.id == 0 && record.tick == 10, "FIFO pop");
    require(queue.due_count(12) == 2, "Due count after one pop");
    require(queue.push({99, 0, 0, 0}) && queue.size() == 32, "Push after pop");
    queue.clear();
    require(queue.size() == 0 && !queue.pop(record), "Clear and empty pop");
    queue.push({1,0,0,0});queue.push({2,0xffffffffu,0,0});
    require(queue.due_count(0x80000000u)==0,"Signed minimum query keeps a zero-time head");
    require(queue.due_count(0xffffffffu)==0,"Signed negative query keeps a zero-time head");
    require(queue.due_count(0)==2,"Zero query keeps its all-records rule");
    queue.clear();queue.push({1,0x80000000u,0,0});queue.push({2,0xffffffffu,0,0});queue.push({3,1,0,0});
    require(queue.due_count(0x80000000u)==1,"Signed minimum tick is due at equality");
    require(queue.due_count(0xffffffffu)==2,"Signed negative ticks precede positive ticks");
    require(queue.due_count(1)==3,"Signed positive query includes negative timestamps");
}

void check_held_keys() {
    dfre::InputStage stage;
    stage.activate();
    std::array<std::uint8_t, 256> keys{};
    keys[38] = 1; // move_forward, a polled command
    keys[17] = 1; // fire (Ctrl) is a discrete command, not polled
    require(stage.push_held_keys(keys, 20) == 1, "Held keys push only polled commands");
    const auto records = stage.take_due(20);
    require(records.size() == 1, "One held record");
    require(records[0].id == 143 && records[0].tick == 20 && records[0].value == 0, "Forward record");
    require(stage.focus_flag(), "Priority key zero sets the focus flag");

    dfre::InputStage gated;
    gated.activate();
    auto row = gated.table().find_by_name("move_forward");
    std::uint16_t saved = gated.table().at(row).flags;
    gated.table().at(row).flags = std::uint16_t(saved | 0x80);
    std::array<std::uint8_t, 256> alt{};
    alt[38] = 1;
    require(gated.push_held_keys(alt, 21) == 0, "Alt-gated row is skipped without Alt");
    alt[0xC2] = 1;
    require(gated.push_held_keys(alt, 22) == 1, "Alt-gated row fires with Alt");
    gated.table().at(row).flags = saved;
}

void check_mouse() {
    dfre::InputStage stage;
    stage.activate();
    dfre::InputMouseLook look;
    look.x = 1;
    look.scale = 1;
    look.flip = true;
    const auto pushed = stage.push_mouse(look, 0, 30);
    require(pushed == 1, "One absolute look record");
    const auto records = stage.take_due(30);
    require(records.size() == 1 && records[0].id == 0x9E && records[0].tick == 30, "Positive X is turn_right_abs");
    require(records[0].value == 0x4000, "Look value is axis * (scale << 14)");
    look.x=0;look.y=(-2147483647-1);look.flip=false;
    stage.push_mouse(look,0,30);
    const auto wrapped=stage.take_due(30);
    require(wrapped.size()==1 && wrapped[0].id==0x9c && wrapped[0].value==0,"INT_MIN Y wraps before look scaling");

    look.x = -2;
    look.y = -3;
    look.scale = 0x80;
    look.flip = false;
    stage.push_mouse(look, 0, 31);
    const auto flipped = stage.take_due(31);
    require(flipped.size() == 2 && flipped[0].id == 0x9D && flipped[1].id == 0x9B,
            "Negative X and flipped Y use 0x9D and 0x9B");
    require(std::int32_t(flipped[0].value) == 0x400000 && std::int32_t(flipped[1].value) == 0x600000,
            "Negated magnitudes keep the scale shift");

    dfre::InputStage steady;
    steady.activate();
    dfre::InputMouseLook slow;
    slow.x = 1;
    slow.scale = 1;
    slow.flip = true;
    slow.steady = true;
    steady.push_mouse(slow, 0, 32);
    require(std::int32_t(steady.take_due(32)[0].value) == 0x1000, "Steady divides the step by four");

    dfre::InputStage buttons;
    buttons.activate();
    // No shipped polled command carries a mouse bit, so bind one to move_forward
    // and check the per-frame mouse-held loop (0x479630).
    const auto forward = buttons.table().find_by_name("move_forward");
    buttons.table().at(forward).mouse = dfre::mouse_left;
    look = {};
    look.flip = true;
    buttons.push_mouse(look, dfre::mouse_left, 40);
    const auto left = buttons.take_due(40);
    require(left.size() == 1 && left[0].id == 143, "Polled mouse command pushes while held");
}

void check_key_events() {
    dfre::InputStage stage;
    stage.activate();
    dfre::InputKeyEvent event;
    event.key = 34; // VK_HOME bound to Prone, a discrete command
    require(stage.push_key_event(event, 50) == 1, "Plain key event pushes once");
    auto records = stage.take_due(50);
    require(records.size() == 1 && records[0].id == 162 && records[0].tick == 50, "Event record");

    event.modifiers = 1; // shift held
    require(stage.push_key_event(event, 51) == 0, "Modifiers suppress plain keys");
    event.key = 17; // Ctrl bound to attack_1 and attack_2; one event pushes one
    event.modifiers = 4;
    require(stage.push_key_event(event, 52) == 1, "Ctrl-bound command fires with Ctrl held");
    require(stage.take_due(52)[0].id == 140, "First matching command wins");

    dfre::InputKeyEvent polled;
    polled.key = 38; // move_forward is a polled command, not a key event
    require(stage.push_key_event(polled, 53) == 0, "Polled commands ignore key events");

    dfre::InputStage print;
    print.activate();
    dfre::InputKeyEvent shot;
    shot.key = 80; // PrintScreen, flags 0x40
    require(print.push_key_event(shot, 60) == 0, "Shift-gated command stays idle");
    shot.modifiers = 1;
    require(print.push_key_event(shot, 61) == 1, "Shift-gated command fires with Shift");
    const auto shot_records = print.take_due(61);
    require(shot_records.size() == 1 && shot_records[0].id == 2, "PrintScreen record");

    dfre::InputStage none;
    none.activate();
    dfre::InputKeyEvent unbound;
    unbound.key = 0xFE; // no shipped row uses this VK
    require(none.push_key_event(unbound, 1) == 0, "Unbound key pushes nothing");
}

void check_global_key() {
    dfre::InputStage stage;
    stage.activate();
    dfre::GlobalKeyState state;
    state.type_flag = 1;
    state.type_key_a = 0x59;
    state.type_key_b = 0x4E;
    auto result = stage.global_key(0x59, state);
    require(result.handled && result.dispatched && result.dispatched_id == 3, "Typing confirm dispatches exit");
    result = stage.global_key(0x4E, state);
    require(result.handled && result.effect == dfre::GlobalKeyEffect::clear_typing, "Typing cancel clears the flag");
    result = stage.global_key(0x41, state);
    require(!result.handled, "Typing state swallows other keys");

    state = {};
    state.digit_flag = 1;
    result = stage.global_key('7', state);
    require(result.handled && result.effect == dfre::GlobalKeyEffect::digit && result.argument == 7, "Digit select");
    result = stage.global_key('0', state);
    require(result.handled && result.argument == 0xA, "Zero selects 0xa");
    result = stage.global_key('A', state);
    require(!result.handled, "Digit state swallows letters");

    state = {};
    state.page_flag = 1;
    require(stage.global_key(0x21, state).argument == 0, "Page up flight page");
    require(stage.global_key(0x22, state).argument == 1, "Page down flight page");
    require(!stage.global_key(0x20, state).handled, "Page state swallows space");

    state = {};
    state.menu_flag = 1;
    result = stage.global_key(0x20, state);
    require(result.handled && result.pushed_record && result.pushed.id == 0xD, "Menu space queues action 0xd");
    result = stage.global_key(0x0D, state);
    require(result.handled && result.effect == dfre::GlobalKeyEffect::enter_flag, "Menu enter sets the flag");
    require(!stage.global_key(0x21, state).handled, "Menu without hud flags ignores page keys");

    state.hud_menu = 1;
    state.hud_menu2 = 1;
    require(stage.global_key(0x21, state).effect == dfre::GlobalKeyEffect::menu_page, "Hud menu pages");
    require(!stage.global_key(0x20, state).handled, "Hud page state swallows space");

    state = {};
    state.hud_menu = 1;
    state.actor_flags = 4;
    result = stage.global_key(0x20, state);
    require(result.handled && result.pushed_record, "Hud space queues action 0xd");
    require(!stage.global_key(0x20, dfre::GlobalKeyState{}).handled, "Idle state ignores keys");
}

void check_mouse_pulse() {
    dfre::InputStage stage;
    stage.activate();
    require(!stage.push_mouse_pulse(4), "Left mouse up matches no branch");
    require(!stage.push_mouse_pulse(1), "Mouse move matches no branch");
    require(stage.take_due(0).empty(), "Button ups and moves push nothing");
    require(stage.push_mouse_pulse(2), "Left mouse down");
    auto records = stage.take_due(0);
    require(records.size() == 1 && records[0].id == 140 && records[0].tick == 0, "Left click queues attack_1 at tick zero");
    require(stage.push_mouse_pulse(8), "Right mouse down");
    records = stage.take_due(0);
    require(records.size() == 1 && records[0].id == 6, "Right click queues scope");
    require(stage.push_mouse_pulse(8, 1), "Right click in a menu");
    require(stage.take_due(0)[0].id == 0xD, "Menu right click queues 0xd");
    require(stage.push_mouse_pulse(0x20), "Middle mouse down");
    require(stage.take_due(0)[0].id == 162, "Middle click queues Prone");
    require(stage.push_mouse_pulse(0x100), "Wheel event 0x100");
    require(stage.take_due(0)[0].id == 0xC0, "Wheel event pushes 0xc0");
    require(stage.push_mouse_pulse(0x200), "Wheel event 0x200");
    require(stage.take_due(0)[0].id == 0xBF, "Wheel event pushes 0xbf");
}

void check_frame() {
    dfre::InputStage stage;
    stage.activate();
    dfre::FrameInput input;
    input.tick = 100;
    input.keys[38] = 1;   // forward
    input.mouse_buttons = dfre::mouse_left;
    input.live_input = true;
    input.mouse_enabled = true;
    input.look.x = 0x20;
    input.look.scale = 0x80;
    input.look.flip = true;
    dfre::InputKeyEvent fire;
    fire.key = 17;
    input.events.push_back(fire);
    const auto output = stage.begin_frame(input);
    require(output.queued.size() == 4, "Frame queues tick, event, look and held records");
    require(output.queued[0].id == 0xA3 && output.queued[0].tick == 100, "Frame tick record");
    require(output.queued[1].id == 140 && output.queued[1].tick == 100, "Event record first");
    require(output.queued[2].id == 0x9E, "Look record after the events");
    require(output.queued[3].id == 143, "Held polled command last");
    const auto due = stage.take_due(100);
    require(due.size() == output.queued.size(), "Every queued record is due at the same tick");
    for (std::size_t i = 0; i < due.size(); ++i) require(due[i] == output.queued[i], "Dispatch order is queue order");
    require(stage.queue().size() == 0, "Queue drained");

    dfre::FrameInput disabled;
    disabled.tick = 101;
    disabled.keys[38] = 1;
    disabled.live_input = false;
    disabled.mouse_enabled = true;
    const auto idle = stage.begin_frame(disabled);
    require(idle.queued.size() == 2, "No mouse sampling without live input; held keys still push");
    stage.take_due(101);

    dfre::FrameInput chat;
    chat.tick = 102;
    chat.input_mode = 1;
    chat.keys[38] = 1;
    chat.events.push_back(fire);
    const auto chat_output = stage.begin_frame(chat);
    require(chat_output.chat_keys.size() == 1, "Chat mode records key events");
    require(chat_output.queued.size() == 1 && chat_output.queued[0].id == 0xA3, "Chat mode skips commands but samples mouse gates");
    stage.take_due(102);

    dfre::FrameInput gated;
    gated.tick = 103;
    gated.live_input = true;
    gated.mouse_enabled = false;
    gated.look.x = 5;
    const auto gated_output = stage.begin_frame(gated);
    require(gated_output.queued.size() == 1 && gated_output.queued[0].id == 0xA3, "Mouse gate stops look sampling");
}

void check_command_active() {
    dfre::InputStage stage;
    stage.activate();
    std::array<std::uint8_t, 256> keys{};
    const auto index = stage.table().find_by_name("attack_1");
    require(!stage.command_active(index, keys, 0, 0), "Fire idle");
    keys[17] = 1;
    require(stage.command_active(index, keys, 0, 0), "Fire active through Ctrl");
    keys[17] = 0;
    require(stage.command_active(index, keys, dfre::mouse_left, 0), "Fire active through the mouse");
    const auto scope = stage.table().find_by_name("scope");
    keys[83] = 1;
    require(stage.command_active(scope, keys, 0, 0), "Scope active through S");
}

} // namespace

int main() {
    try {
        check_command_table();
        check_queue();
        check_held_keys();
        check_mouse();
        check_key_events();
        check_global_key();
        check_mouse_pulse();
        check_frame();
        check_command_active();
        std::cout << "input_state checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
