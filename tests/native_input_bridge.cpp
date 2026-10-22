#include "dfre/gameplay_dispatch.hpp"
#include "dfre/input_bridge.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

dfre::InputEvent key_event(dfre::InputKind kind, int vk, bool extended = false) {
    dfre::InputEvent event{kind};
    event.key = vk;
    event.extended = extended;
    return event;
}
dfre::InputEvent key_down(int vk, bool extended = false) { return key_event(dfre::InputKind::key_down, vk, extended); }
dfre::InputEvent key_up(int vk) { return key_event(dfre::InputKind::key_up, vk); }
dfre::InputEvent pointer_event(dfre::InputKind kind, unsigned button) {
    dfre::InputEvent event{kind};
    event.button = button;
    return event;
}
dfre::InputEvent delta_event(int dx, int dy) {
    dfre::InputEvent event{dfre::InputKind::pointer_delta};
    event.x = dx;
    event.y = dy;
    return event;
}
dfre::InputEvent focus_lost_event() { return dfre::InputEvent{dfre::InputKind::focus_lost}; }

dfre::InputSourceFlags active_flags() {
    dfre::InputSourceFlags flags;
    flags.live_input = true;
    flags.windowed = false;
    flags.window_active = true;
    flags.mouse_scale = 1;
    return flags;
}

std::vector<std::uint32_t> ids(const std::vector<dfre::InputRecord>& records) {
    std::vector<std::uint32_t> out;
    for (const auto& record : records) out.push_back(record.id);
    return out;
}

bool contains(const std::vector<std::uint32_t>& values, std::uint32_t value) {
    for (const auto candidate : values) if (candidate == value) return true;
    return false;
}

// Minimal GameWindow substitute; no SDL, no real cursor or window.
class MockWindow final : public dfre::GameWindow {
public:
    std::vector<dfre::InputEvent> queued;
    std::vector<dfre::InputEvent> poll() override {
        std::vector<dfre::InputEvent> result = std::move(queued);
        queued.clear();
        return result;
    }
    void present(const std::vector<std::uint32_t>&) override {}
    std::uint32_t ticks() const override { return 0; }
    void yield() const override {}
};

void check_bindings() {
    auto bridge = dfre::InputBridge::with_shipped_profile();
    const auto forward = bridge.command_table().find_by_name("move_forward");
    require(forward != std::string_view::npos, "move_forward row");
    require(bridge.command_table().at(forward).key == 38, "Shipped forward VK_UP 38");
    const auto ctrl_fire = bridge.command_table().find_by_name("attack_1");
    require(bridge.command_table().at(ctrl_fire).key == 17 && bridge.command_table().at(ctrl_fire).mouse == 1,
            "Shipped fire Ctrl 17 and left mouse");
    require(bridge.command_table().at(bridge.command_table().find_by_name("NoRun")).key == 16,
            "Shipped NoRun VK_SHIFT 16");
    require(bridge.stage().active_commands().size() == 24, "Polled list from 0x47a300");

    auto remapped = dfre::default_action_bindings();
    remapped[2] = {87, 0, 0}; // move_forward -> 'W'
    dfre::InputBridge w_bridge(remapped);
    require(w_bridge.command_table().at(w_bridge.command_table().find_by_name("move_forward")).key == 87,
            "Profile remap applies by name");
    w_bridge.set_source_flags(active_flags());
    w_bridge.feed(key_down(87));
    auto frame = w_bridge.frame(1);
    require(contains(ids(frame.commands), 143), "Remapped W pushes forward");
    require(!contains(ids(frame.commands), 38), "Old key is not a command id");
    dfre::InputBridge up_bridge(remapped);
    up_bridge.set_source_flags(active_flags());
    up_bridge.feed(key_down(38));
    require(!contains(ids(up_bridge.frame(1).commands), 143), "VK_UP no longer bound after remap");
}

void check_held_and_release() {
    auto bridge = dfre::InputBridge::with_shipped_profile();
    bridge.feed(key_down(38));
    // Per-frame flags from the caller (the two-argument form).
    auto first = bridge.frame(10, active_flags());
    require(first.commands.size() == 2, "Forward frame pushes the tick record and the held record");
    require(first.commands[0].id == 0xA3 && first.commands[0].tick == 10 && first.commands[0].value == 0,
            "0x479750 tick record first");
    require(first.commands[1].id == 143 && first.commands[1].tick == 10, "Held forward record");
    auto second = bridge.frame(11);
    require(contains(ids(second.commands), 143), "Held key keeps pushing every frame");
    bridge.feed(key_up(38));
    auto released = bridge.frame(12);
    require(ids(released.commands) == std::vector<std::uint32_t>{0xA3}, "Release stops the held record");
    bridge.feed(key_down(38));
    require(contains(ids(bridge.frame(13).commands), 143), "Press again resumes");
    // Extended VK flag does not change the command path.
    auto up_bridge = dfre::InputBridge::with_shipped_profile();
    up_bridge.set_source_flags(active_flags());
    up_bridge.feed(key_down(38, true));
    require(contains(ids(up_bridge.frame(1).commands), 143), "Extended VK_UP still pushes forward");
}

void check_discrete_pulses() {
    auto bridge = dfre::InputBridge::with_shipped_profile();
    bridge.set_source_flags(active_flags());
    bridge.feed(key_down(17)); // Ctrl fire
    auto pressed = bridge.frame(20);
    require(ids(pressed.commands) == std::vector<std::uint32_t>({0xA3, 140}),
            "One event pushes one record; attack_1 wins over attack_2");
    bridge.feed(key_up(17));
    require(ids(bridge.frame(21).commands) == std::vector<std::uint32_t>{0xA3}, "Released fire pushes nothing");
    bridge.feed(key_down(17));
    bridge.feed(key_down(17)); // platform repeat or a second press
    const auto repeated = bridge.frame(22);
    std::size_t fire_count = 0;
    for (const auto id : ids(repeated.commands)) if (id == 140) ++fire_count;
    require(fire_count == 2, "Each queued key event pushes one record in the same frame");

    auto prone = dfre::InputBridge::with_shipped_profile();
    prone.set_source_flags(active_flags());
    prone.feed(key_down(34)); // VK_NEXT Prone (Page Down)
    require(contains(ids(prone.frame(1).commands), 162), "Prone is a discrete key event");
    prone.feed(key_up(34));
    require(ids(prone.frame(2).commands) == std::vector<std::uint32_t>{0xA3}, "Prone releases");
}

void check_modifiers() {
    auto bridge = dfre::InputBridge::with_shipped_profile();
    bridge.set_source_flags(active_flags());
    bridge.feed(key_down(0x10)); // Shift: the held NoRun (Walk) command, no discrete event
    auto shift_frame = bridge.frame(30);
    std::size_t prones = 0;
    for (const auto id : ids(shift_frame.commands)) if (id == 162) ++prones;
    require(ids(shift_frame.commands) == std::vector<std::uint32_t>({0xA3, 159}) && prones == 0,
            "Shift holds NoRun and is not a discrete command");
    bridge.feed(key_down(80)); // PrintScreen needs Shift (flags 0x40)
    require(contains(ids(bridge.frame(31).commands), 2), "Shift-gated PrintScreen fires with Shift held");
    bridge.feed(key_up(80));
    bridge.feed(key_up(0x10));
    bridge.feed(key_down(80));
    const auto unshifted = bridge.frame(32);
    require(!contains(ids(unshifted.commands), 2), "PrintScreen stays idle without Shift");
    require(!contains(ids(unshifted.commands), 159), "NoRun released with Shift");
    // PrintScreen ('P', VK 80) shares its key with the held CameraUp command.
    require(contains(ids(unshifted.commands), 412), "Held CameraUp shares the P key");

    auto ctrl = dfre::InputBridge::with_shipped_profile();
    ctrl.set_source_flags(active_flags());
    ctrl.feed(key_down(0x11)); // Ctrl
    require(contains(ids(ctrl.frame(1).commands), 140), "Ctrl press fires the Ctrl-bound command");
    ctrl.feed(key_down(34)); // Prone with Ctrl held is suppressed
    require(ids(ctrl.frame(2).commands) == std::vector<std::uint32_t>{0xA3},
            "Ctrl suppresses plain discrete keys");
    ctrl.feed(key_up(0x11));
    ctrl.feed(key_down(34));
    require(contains(ids(ctrl.frame(3).commands), 162), "Prone fires once Ctrl is released");
}

void check_mouse() {
    auto bridge = dfre::InputBridge::with_shipped_profile();
    bridge.set_source_flags(active_flags());
    bridge.feed(pointer_event(dfre::InputKind::pointer_down, 1));
    require(bridge.mouse_buttons() == 1, "Left mouse sets mask bit 0");
    auto left = bridge.frame(40);
    require(ids(left.commands) == std::vector<std::uint32_t>({140, 0xA3}),
            "Mouse pulse is queued before the frame records");
    require(left.commands[0].tick == 0, "0x47a150 pulses carry tick zero");
    bridge.feed(pointer_event(dfre::InputKind::pointer_up, 1));
    require(bridge.mouse_buttons() == 0, "Left mouse up clears mask");
    require(ids(bridge.frame(41).commands) == std::vector<std::uint32_t>{0xA3}, "No held record for the mouse");
    bridge.feed(pointer_event(dfre::InputKind::pointer_down, 2));
    require(contains(ids(bridge.frame(42).commands), 6), "Right click pulses scope");
    bridge.feed(pointer_event(dfre::InputKind::pointer_up, 2));
    bridge.feed(pointer_event(dfre::InputKind::pointer_down, 3));
    require(bridge.mouse_buttons() == 0x10, "Middle mouse uses the 0x10 mask");
    require(contains(ids(bridge.frame(43).commands), 162), "Middle click pulses Prone");
    bridge.feed(pointer_event(dfre::InputKind::pointer_up, 3));

    auto menu = dfre::InputBridge::with_shipped_profile();
    auto flags = active_flags();
    flags.global.menu_flag = 1;
    menu.set_source_flags(flags);
    menu.feed(pointer_event(dfre::InputKind::pointer_down, 2));
    require(contains(ids(menu.frame(1).commands), 0xD), "Right click inside a menu queues 0xd");
    menu.feed(pointer_event(dfre::InputKind::pointer_down, 1));
    require(!contains(ids(menu.frame(2).commands), 0xD), "Left click keeps the bound command in a menu");

    auto legacy = dfre::InputBridge::with_shipped_profile();
    legacy.set_source_flags(active_flags());
    legacy.feed(pointer_event(dfre::InputKind::pointer_down, 0));
    require(legacy.mouse_buttons() == 1, "Legacy zero button is left");
    require(contains(ids(legacy.frame(1).commands), 140), "Legacy left click pulses fire");
}

void check_mouse_look() {
    auto bridge = dfre::InputBridge::with_shipped_profile();
    bridge.set_source_flags(active_flags());
    bridge.feed(delta_event(1, 0));
    auto turned = bridge.frame(50);
    require(turned.commands.size() == 2 && turned.commands[1].id == 0x9E && turned.commands[1].value == 0x4000,
            "Positive X becomes turn_right_abs with scale 1");
    require(bridge.pending_delta_x() == 0, "Deltas are consumed per frame");
    bridge.feed(delta_event(0, -1));
    auto looked = bridge.frame(51);
    require(looked.commands[1].id == 0x9B && looked.commands[1].value == 0x4000,
            "Flipped Y becomes look_up_abs");
    auto flipped = dfre::InputBridge::with_shipped_profile();
    auto flags = active_flags();
    flags.mouse_flip = true;
    flipped.set_source_flags(flags);
    flipped.feed(delta_event(0, -1));
    require(flipped.frame(1).commands[1].id == 0x9C, "flipmouse keeps the sign");
    auto steady = dfre::InputBridge::with_shipped_profile();
    flags = active_flags();
    flags.steady = true;
    steady.set_source_flags(flags);
    steady.feed(delta_event(1, 0));
    require(steady.frame(1).commands[1].value == 0x1000, "Steady divides the look step by four");
    auto gated = dfre::InputBridge::with_shipped_profile();
    flags = active_flags();
    flags.window_active = false;
    gated.set_source_flags(flags);
    gated.feed(delta_event(4, 4));
    require(ids(gated.frame(1).commands) == std::vector<std::uint32_t>{0xA3}, "Inactive window stops mouse sampling");
    require(gated.pending_delta_x() == 4, "Gated mouse path keeps unread deltas");
}

void check_focus() {
    auto bridge = dfre::InputBridge::with_shipped_profile();
    bridge.set_source_flags(active_flags());
    bridge.feed(key_down(38));
    bridge.feed(delta_event(3, 3));
    bridge.feed(pointer_event(dfre::InputKind::pointer_down, 1));
    auto active = bridge.frame(60);
    require(contains(ids(active.commands), 140) && contains(ids(active.commands), 143),
            "Fire and forward before focus loss");
    bridge.feed(focus_lost_event());
    require(bridge.key_state()[38] == 0 && bridge.mouse_buttons() == 1 && !bridge.source_flags().window_active &&
            bridge.pending_delta_x() == 0 && bridge.pending_delta_y() == 0,
            "Focus loss clears keys and window activity");
    auto cleared = bridge.frame(61);
    require(ids(cleared.commands) == std::vector<std::uint32_t>{0xA3},
            "Stale-held state is not queued after focus loss");
    bridge.feed(key_down(38));
    require(contains(ids(bridge.frame(62).commands), 143), "Input works again after focus loss");

    auto stale = dfre::InputBridge::with_shipped_profile();
    stale.set_source_flags(active_flags());
    stale.feed(key_down(17)); // queued ring event
    stale.feed(focus_lost_event());
    require(ids(stale.frame(63).commands) == std::vector<std::uint32_t>({0xA3,140}),
            "Focus loss preserves the queued key event");
    stale.feed(key_up(17));
    require(!contains(ids(stale.frame(64).commands), 140), "The key event is consumed once");
}

void check_frame_flags() {
    auto bridge = dfre::InputBridge::with_shipped_profile();
    auto flags = active_flags();
    flags.live_input = false;
    bridge.set_source_flags(flags);
    bridge.feed(key_down(38));
    bridge.feed(key_down(34));
    bridge.feed(delta_event(5, 5));
    // 0x479750 only gates 0x479630 (mouse) on [0xbd482c]; key events and held
    // keys keep running while the demo/replay flag is set.
    auto offline = bridge.frame(70);
    require(contains(ids(offline.commands), 143), "Held keys still run without live input");
    require(contains(ids(offline.commands), 162), "Key events still push without live input");
    require(!contains(ids(offline.commands), 0x9E), "No look records without live input");

    auto chat = dfre::InputBridge::with_shipped_profile();
    flags = active_flags();
    flags.input_mode = 1;
    chat.set_source_flags(flags);
    chat.feed(key_down(0x41)); // 'A' chat key
    chat.feed(delta_event(1, 0));
    chat.feed(key_down(38));
    auto chatting = chat.frame(71);
    require(chatting.chat_keys.size() == 2, "Chat mode records the key stream");
    require(contains(ids(chatting.commands), 0x9E), "Chat mode still samples the mouse gate");
    require(!contains(ids(chatting.commands), 143), "Chat mode stops held commands");

    auto windowed = dfre::InputBridge::with_shipped_profile();
    flags = active_flags();
    flags.windowed = true;
    windowed.set_source_flags(flags);
    windowed.feed(delta_event(1, 0));
    require(ids(windowed.frame(1).commands) == std::vector<std::uint32_t>{0xA3},
            "Windowed mode disables the 0x479630 mouse path");
}

void check_mock_window() {
    MockWindow window;
    window.queued.push_back(key_down(38));
    window.queued.push_back(delta_event(1, 0));
    window.queued.push_back(dfre::InputEvent{dfre::InputKind::quit});
    auto bridge = dfre::InputBridge::with_shipped_profile();
    bridge.set_source_flags(active_flags());
    bridge.feed(window.poll());
    require(bridge.quit_requested(), "Quit event is surfaced");
    const auto frame = bridge.frame(80);
    require(contains(ids(frame.commands), 143) && contains(ids(frame.commands), 0x9E),
            "GameWindow stream joins the stage");
}

void check_adapter_and_dispatch() {
    using dfre::DispatchActor;
    using dfre::DispatchCallKind;
    using dfre::DispatchGlobals;
    DispatchGlobals globals;
    DispatchActor actor;
    auto bridge = dfre::InputBridge::with_shipped_profile();
    bridge.set_source_flags(active_flags());
    const auto apply = [&](const dfre::BridgeFrame& frame, std::vector<dfre::DispatchCall>& calls) {
        const auto run = [&](const dfre::InputRecord& record) {
            const auto result = dfre::dispatch_game_command(record.id, std::int32_t(record.value), globals, actor,
                                                            bridge.command_table());
            calls.insert(calls.end(), result.calls.begin(), result.calls.end());
        };
        for (const auto& record : frame.immediate) run(record);
        dfre::dispatch_commands(frame.commands, run);
    };
    std::vector<dfre::DispatchCall> calls;

    bridge.feed(key_down(38));
    apply(bridge.frame(90), calls);
    require(globals.control_c23a00 == 0x2, "Forward sets movement bit 1 after the 0xa3 reset");
    globals.control_c23a00 = 0xffff;
    bridge.feed(key_up(38));
    apply(bridge.frame(91), calls);
    require(globals.control_c23a00 == 0, "Release frame's 0xa3 record clears the control word");

    bridge.feed(key_down(40));
    apply(bridge.frame(92), calls);
    require(globals.control_c23a00 == 0x4, "Move back sets bit 2");
    globals.control_c23a00 = 0;
    bridge.feed(key_up(40));
    bridge.feed(key_down(32)); // jump
    apply(bridge.frame(93), calls);
    require(globals.control_c23a00 == 0x400, "Jump bit");
    globals.control_c23a00 = 0;
    bridge.feed(key_up(32));

    // Polled profile rows: VK_LEFT 37 slide_left bit 3, VK_RIGHT 39 slide_right
    // bit 4, Numpad4 100 turn_left 8, Numpad6 102 turn_right 9, Shift 16 NoRun
    // bit 0. Crouch VK_END 35 is discrete and sets the pulse global 0xc23a04.
    const std::pair<int, std::uint32_t> keyed[] = {
        {37, 0x8}, {39, 0x10}, {100, 0x100}, {102, 0x200}, {16, 0x1},
    };
    std::uint32_t expected = 0;
    for (const auto& [vk, bit] : keyed) {
        globals.control_c23a00 = 0;
        bridge.feed(key_down(vk));
        apply(bridge.frame(94), calls);
        require(globals.control_c23a00 == bit, "Held command bit from the profile binding");
        expected |= bit;
        bridge.feed(key_up(vk));
        apply(bridge.frame(95), calls);
        require(globals.control_c23a00 == 0, "Each release clears through the tick record");
    }
    require(expected == (0x1u | 0x8u | 0x10u | 0x100u | 0x200u), "Keyed coverage");

    globals.pulse_c23a04 = 0;
    bridge.feed(key_down(35)); // VK_END Crouch
    apply(bridge.frame(95), calls);
    require(globals.pulse_c23a04 == 1, "Crouch key event reaches the pulse global");
    bridge.feed(key_up(35));

    globals.control_c23a00 = 0;
    bridge.feed(key_down(38));
    apply(bridge.frame(96), calls);
    bridge.feed(key_up(38));
    bridge.feed(key_down(17)); // fire pulse
    calls.clear();
    apply(bridge.frame(97), calls);
    bool fired = false;
    for (const auto& call : calls) if (call.kind == DispatchCallKind::weapon_fire) fired = true;
    require(fired, "Left/Ctrl fire dispatches weapon_fire");
    require(globals.control_c23a00 == 0, "Previous forward bit was cleared by the tick record");
    bridge.feed(key_up(17));

    actor.look_yaw_30 = 0;
    actor.look_pitch_34 = 0x100;
    bridge.feed(delta_event(1, -2));
    apply(bridge.frame(98), calls);
    require(actor.look_yaw_30 == std::uint32_t(0) - std::uint32_t(0x4000), "turn_right_abs subtracts yaw");
    require(actor.look_pitch_34 == 0x100 + 0x8000, "look_up_abs adds pitch");

    int seen = 0;
    dfre::dispatch_commands(bridge.frame(99).commands, [&](const dfre::InputRecord&) { ++seen; });
    require(seen == 1, "Adapter visits every ordered record");
}

// One realistic multi-frame sequence through the real dispatcher: walk, look,
// fire, release, focus loss, re-acquire.
void check_sequence() {
    using dfre::DispatchActor;
    using dfre::DispatchCallKind;
    using dfre::DispatchGlobals;
    DispatchGlobals globals;
    DispatchActor actor;
    auto bridge = dfre::InputBridge::with_shipped_profile();
    auto flags = active_flags();
    bridge.set_source_flags(flags);
    const auto run = [&](const dfre::BridgeFrame& frame, std::size_t* fires) {
        const auto apply = [&](const dfre::InputRecord& record) {
            const auto result = dfre::dispatch_game_command(record.id, std::int32_t(record.value), globals, actor,
                                                            bridge.command_table());
            if (fires) for (const auto& call : result.calls) if (call.kind == DispatchCallKind::weapon_fire) ++*fires;
        };
        for (const auto& record : frame.immediate) apply(record);
        dfre::dispatch_commands(frame.commands, apply);
    };
    std::size_t fires = 0;

    bridge.feed(key_down(38)); // walk forward
    run(bridge.frame(100), &fires);
    require(globals.control_c23a00 == 0x2, "Sequence: forward held");
    bridge.feed(delta_event(2, 0)); // mouse turn
    run(bridge.frame(101), &fires);
    require(globals.control_c23a00 == 0x2, "Sequence: forward survives a look-only frame");
    require(actor.look_yaw_30 == std::uint32_t(0) - std::uint32_t(0x8000), "Sequence: yaw turned by 2 steps");
    bridge.feed(key_down(17)); // fire
    run(bridge.frame(102), &fires);
    require(fires == 1 && globals.control_c23a00 == 0x2, "Sequence: fire pulse while walking");
    bridge.feed(key_up(17));
    bridge.feed(key_up(38));
    run(bridge.frame(103), &fires);
    require(globals.control_c23a00 == 0, "Sequence: release clears movement through the tick record");
    bridge.feed(focus_lost_event());
    run(bridge.frame(104), nullptr);
    require(globals.control_c23a00 == 0, "Sequence: focus loss keeps the cleared state");
    bridge.feed(key_down(38));
    run(bridge.frame(105), nullptr);
    require(globals.control_c23a00 == 0x2, "Sequence: input resumes after focus loss");
    bridge.feed(key_up(38));
    run(bridge.frame(106), nullptr);
    require(globals.control_c23a00 == 0, "Sequence: final release");
    require(fires == 1, "Sequence: exactly one fire");
}
void check_ring_and_live_sources(){
    {
        auto bridge=dfre::InputBridge::with_shipped_profile();auto flags=active_flags();flags.keyboard_ring_blocked=true;
        bridge.set_source_flags(flags);bridge.feed(key_down(34));bridge.feed(key_down(38));
        const auto frame=bridge.frame(1);
        require(bridge.key_state()[34] && !contains(ids(frame.commands),162) && contains(ids(frame.commands),143),"Blocked ring keeps VK writes and held input");
    }
    for(const auto count:{128,129}){
        auto bridge=dfre::InputBridge::with_shipped_profile();bridge.set_source_flags(active_flags());
        for(int i=0;i<count;++i)bridge.feed(key_down(17));
        require(bridge.frame(1).commands.size()==std::size_t(count==128?1:2),"128-slot ring modulo state");
    }
    auto bridge=dfre::InputBridge::with_shipped_profile();auto flags=active_flags();flags.global.digit_flag=1;
    unsigned effects=0,chats=0;
    bridge.set_live_sources([&]{return flags;},[&](const auto& effect){
        require(effect.effect==dfre::GlobalKeyEffect::digit && effect.argument==1,"Current digit effect");
        ++effects;flags.input_mode=1;flags.global.digit_flag=0;
    },[&](const auto& event){
        require(event.key=='2',"Current chat event");++chats;flags.input_mode=0;bridge.feed(key_down(34));
    });
    bridge.feed(key_down('1'));bridge.feed(key_down('2'));
    const auto frame=bridge.frame(10);
    require(effects==1 && chats==1 && contains(ids(frame.commands),162),"Synchronous source mutation and appended ring event");
    bridge.feed(key_down(17));
    require(bridge.query_active(bridge.command_table().find_by_name("attack_1")),"Query uses current VK state");
    bool failed=false;try{bridge.query_active(0x8c);}catch(const std::out_of_range&){failed=true;}
    require(failed,"Query needs its row beyond the profile table");
    bridge.set_query_row_source([](auto index)->std::optional<dfre::CommandBinding>{
        if(index==0x8c)return dfre::CommandBinding{};return std::nullopt;
    });
    require(!bridge.query_active(0x8c),"Row 140 stays distinct from command ID140");
    flags.joystick_buttons=8;bridge.set_query_row_source([](auto)->std::optional<dfre::CommandBinding>{
        dfre::CommandBinding row;row.joystick=8;return row;
    });
    require(bridge.query_active(0x8c),"Query reads current joystick source");
    auto error=dfre::InputBridge::with_shipped_profile();auto state=active_flags();state.global.digit_flag=1;
    error.set_live_sources([&]{return state;},[&](const auto&){state.global.digit_flag=0;throw std::runtime_error("effect stopped");});
    error.feed(key_down('1'));error.feed(key_down(34));failed=false;
    try{error.frame(1);}catch(const std::runtime_error&){failed=true;}
    require(failed && contains(ids(error.frame(2).commands),162),"Provider error preserves the unread ring event");
}

void check_split_frame() {
    auto bridge=dfre::InputBridge::with_shipped_profile();
    bridge.feed(key_down(38));
    const auto frame=bridge.begin_frame(10,active_flags());
    require(frame.commands.empty()&&bridge.stage().queue().size()==2,
            "Input sampling must keep commands in the shared queue");
    bridge.feed(pointer_event(dfre::InputKind::pointer_down,1));
    require(bridge.pop_due(9)==std::nullopt,"Future head record must stay queued");
    const auto first=bridge.pop_due(10);
    require(first&&first->id==0xa3,"Split frame starts with the tick command");
    bridge.feed(pointer_event(dfre::InputKind::pointer_down,1));
    std::vector<std::uint32_t> remaining;
    while(const auto record=bridge.pop_due(10))remaining.push_back(record->id);
    require(remaining==std::vector<std::uint32_t>({143,140,140}),
            "Split drain must include commands added between dispatches");
    require(bridge.stage().queue().size()==0,"Split drain left due commands");
}

} // namespace

int main() {
    try {
        check_bindings();
        check_held_and_release();
        check_discrete_pulses();
        check_modifiers();
        check_mouse();
        check_mouse_look();
        check_focus();
        check_frame_flags();
        check_mock_window();
        check_adapter_and_dispatch();
        check_sequence();
        check_ring_and_live_sources();
        check_split_frame();
        std::cout << "input_bridge checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
