#include "dfre/input_state.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// Line probes for tools/check_input_state_original.py. Each mode reads one case
// per line and prints one result per line.

namespace {

void print_records(const std::vector<dfre::InputRecord>& records) {
    bool first = true;
    for (const auto& record : records) {
        if (!first) std::cout << ';';
        first = false;
        std::cout << record.id << ':' << record.tick << ':' << record.value << ':' << record.extra;
    }
    std::cout << '\n';
}

std::vector<dfre::InputRecord> drain(dfre::InputStage& stage, std::uint32_t tick) {
    return stage.take_due(tick);
}

} // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "table") {
        dfre::CommandTable table;
        for (const auto& row : table.rows()) {
            std::cout << row.id << ':' << row.key << ':' << row.mouse << ':' << row.joystick << ':' << row.flags << '\n';
        }
        return 0;
    }
    if (mode == "set") {
        std::uint32_t index, key, mouse;
        while (std::cin >> index >> key >> mouse) {
            dfre::CommandTable table;
            table.set_binding(index, key, mouse);
            std::cout << table.at(index).key << ' ' << table.at(index).mouse << '\n';
        }
        return std::cin.eof() ? 0 : 2;
    }
    if (mode == "bind") {
        bool processed = false;
        while (true) {
            dfre::ActionBindings bindings{};
            bool complete = true;
            for (std::size_t i = 0; i < dfre::action_count; ++i) {
                std::uint32_t key, mouse, joy;
                if (!(std::cin >> key >> mouse >> joy)) { complete = false; break; }
                bindings[i].key = key;
                bindings[i].mouse = mouse;
                bindings[i].joystick = joy;
            }
            if (!complete) return processed ? 0 : 2;
            processed = true;
            dfre::CommandTable table;
            table.apply_profile_bindings(bindings);
            bool first = true;
            for (const auto& pair : dfre::profile_command_map()) {
                const auto index = table.find_by_name(pair.command);
                if (!first) std::cout << ' ';
                first = false;
                std::cout << index << ':' << table.at(index).key << ':' << table.at(index).mouse;
            }
            std::cout << '\n';
        }
        return 0;
    }
    if (mode == "active") {
        dfre::CommandTable table;
        const auto active = table.active_command_indices();
        for (std::size_t i = 0; i < active.size(); ++i) {
            if (i) std::cout << ' ';
            std::cout << active[i];
        }
        std::cout << '\n';
        return 0;
    }
    if (mode == "held") {
        std::uint32_t tick, priority, count;
        while (std::cin >> tick >> priority >> count) {
            std::array<std::uint8_t, 256> keys{};
            for (std::uint32_t i = 0; i < count; ++i) {
                std::uint32_t key;
                std::cin >> key;
                keys[key & 0xff] = 1;
            }
            dfre::InputStage stage;
            stage.activate();
            (void)stage.push_held_keys(keys, tick, std::uint16_t(priority));
            std::cout << (stage.focus_flag() ? 1 : 0) << '|';
            print_records(drain(stage, 0));
        }
        return std::cin.eof() ? 0 : 2;
    }
    if (mode == "mouse") {
        std::int32_t x, y, scale;
        std::uint32_t flip, steady, buttons, tick;
        while (std::cin >> x >> y >> scale >> flip >> steady >> buttons >> tick) {
            dfre::InputStage stage;
            stage.activate();
            dfre::InputMouseLook look;
            look.x = x;
            look.y = y;
            look.scale = scale;
            look.flip = flip != 0;
            look.steady = steady != 0;
            (void)stage.push_mouse(look, buttons, tick);
            print_records(drain(stage, 0));
        }
        return std::cin.eof() ? 0 : 2;
    }
    if (mode == "event") {
        std::uint32_t key, state, flag, modifiers, tick;
        while (std::cin >> key >> state >> flag >> modifiers >> tick) {
            dfre::InputStage stage;
            stage.activate();
            dfre::InputKeyEvent event;
            event.key = std::uint8_t(key);
            event.state = std::uint8_t(state);
            event.flag = std::uint8_t(flag);
            event.modifiers = std::uint8_t(modifiers);
            (void)stage.push_key_event(event, tick);
            print_records(drain(stage, 0));
        }
        return std::cin.eof() ? 0 : 2;
    }
    if (mode == "pulse") {
        std::uint32_t event, menu_flag, actor_flags;
        while (std::cin >> event >> menu_flag >> actor_flags) {
            dfre::InputStage stage;
            stage.activate();
            (void)stage.push_mouse_pulse(event, std::uint8_t(menu_flag), actor_flags);
            print_records(drain(stage, 0));
        }
        return std::cin.eof() ? 0 : 2;
    }
    if (mode == "filter") {
        std::uint32_t key, type_flag, digit_flag, page_flag, menu_flag, hud_menu, hud_menu2;
        std::uint32_t type_key_a, type_key_b, actor_flags;
        while (std::cin >> key >> type_flag >> digit_flag >> page_flag >> menu_flag >> hud_menu >> hud_menu2
                       >> type_key_a >> type_key_b >> actor_flags) {
            dfre::InputStage stage;
            dfre::GlobalKeyState state;
            state.type_flag = std::uint8_t(type_flag);
            state.digit_flag = std::uint8_t(digit_flag);
            state.page_flag = std::uint8_t(page_flag);
            state.menu_flag = std::uint8_t(menu_flag);
            state.hud_menu = std::uint8_t(hud_menu);
            state.hud_menu2 = std::uint8_t(hud_menu2);
            state.type_key_a = std::uint16_t(type_key_a);
            state.type_key_b = std::uint16_t(type_key_b);
            state.actor_flags = actor_flags;
            const auto result = stage.global_key(std::uint16_t(key), state);
            std::cout << (result.handled ? 1 : 0) << ':' << static_cast<int>(result.effect) << ':'
                      << result.argument << ':' << (result.pushed_record ? result.pushed.id : ~0u) << ':'
                      << (result.dispatched ? result.dispatched_id : ~0u) << '\n';
        }
        return std::cin.eof() ? 0 : 2;
    }
    if (mode == "frame") {
        while (true) {
            dfre::FrameInput input;
            std::uint32_t input_mode, live, mouse_enabled, priority;
            if (!(std::cin >> input.tick >> input_mode >> live >> mouse_enabled >> priority
                           >> input.mouse_buttons >> input.look.x >> input.look.y >> input.look.scale)) {
                return std::cin.eof() ? 0 : 2;
            }
            std::uint32_t flip, steady;
            std::cin >> flip >> steady;
            input.look.flip = flip != 0;
            input.look.steady = steady != 0;
            std::uint32_t type_flag, digit_flag, page_flag, menu_flag, hud_menu, hud_menu2, type_key_a, type_key_b, actor_flags;
            std::cin >> type_flag >> digit_flag >> page_flag >> menu_flag >> hud_menu >> hud_menu2
                    >> type_key_a >> type_key_b >> actor_flags;
            input.global.type_flag = std::uint8_t(type_flag);
            input.global.digit_flag = std::uint8_t(digit_flag);
            input.global.page_flag = std::uint8_t(page_flag);
            input.global.menu_flag = std::uint8_t(menu_flag);
            input.global.hud_menu = std::uint8_t(hud_menu);
            input.global.hud_menu2 = std::uint8_t(hud_menu2);
            input.global.type_key_a = std::uint16_t(type_key_a);
            input.global.type_key_b = std::uint16_t(type_key_b);
            input.global.actor_flags = actor_flags;
            input.input_mode = std::uint8_t(input_mode);
            input.live_input = live != 0;
            input.mouse_enabled = mouse_enabled != 0;
            input.priority_key = std::uint16_t(priority);
            std::uint32_t count;
            std::cin >> count;
            for (std::uint32_t i = 0; i < count; ++i) {
                std::uint32_t key;
                std::cin >> key;
                input.keys[key & 0xff] = 1;
            }
            std::cin >> count;
            for (std::uint32_t i = 0; i < count; ++i) {
                std::uint32_t key, state, flag, modifiers;
                std::cin >> key >> state >> flag >> modifiers;
                dfre::InputKeyEvent event;
                event.key = std::uint8_t(key);
                event.state = std::uint8_t(state);
                event.flag = std::uint8_t(flag);
                event.modifiers = std::uint8_t(modifiers);
                input.events.push_back(event);
            }
            std::cin >> count;
            dfre::InputStage stage;
            stage.activate();
            for (std::uint32_t i = 0; i < count; ++i) {
                std::uint32_t event, menu, actor;
                std::cin >> event >> menu >> actor;
                (void)stage.push_mouse_pulse(event, std::uint8_t(menu), actor);
            }
            const auto output = stage.begin_frame(input);
            bool first = true;
            const auto emit = [&](const std::vector<dfre::InputRecord>& records) {
                for (const auto& record : records) {
                    if (!first) std::cout << ';';
                    first = false;
                    std::cout << record.id << ':' << record.tick << ':' << record.value << ':' << record.extra;
                }
            };
            emit(output.immediate);
            emit(drain(stage, input.tick));
            std::cout << '\n';
        }
        return 0;
    }
    std::cerr << "input_state_probe MODE\nModes: set bind active held mouse event pulse filter frame\n";
    return 2;
}
