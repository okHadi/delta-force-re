#include "dfre/gameplay_dispatch.hpp"
#include "dfre/assets.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

// Line protocol: one dispatch case per line, whitespace separated numbers in the
// exact order printed by the "fields" mode (except id and value first).
// Output: one JSON object per line.
namespace {
using dfre::DispatchGlobals;

struct Case {
    std::uint32_t id = 0;
    std::int32_t value = 0;
    DispatchGlobals g;
    dfre::DispatchActor a;
};

void print_fields() {
    std::cout << "id value actor_flags ";
#define P(name) std::cout << #name << ' '
    P(pause_6e4714); P(pause2_6e4718); P(menu_flag_67dbc0); P(multiplayer_67f520);
    P(mission_67f528); P(mission2_67f52c); P(server_67dbb8); P(command_tick_6e471c);
    P(tick_base_bd4844); P(resolution_index_67f4f4); P(resolution_max_503134);
    P(lb_size_67f4f8); P(mouse_disabled_503130); P(camera_mode_bd0040);
    P(saturation_67f4f0); P(mouse_scale_67f500); P(fov_c23a28); P(map_terrain_c23a2c);
    P(map_elevation_c23a30); P(fo_toggle_c23a34); P(hud_color_index_67f508);
    P(hud_detail_67f50c); P(toggle_67f4fc); P(toggle_67f504); P(toggle_67f510);
    P(toggle_67f514); P(toggle_67f518); P(toggle_67f564); P(toggle_503428);
    P(toggle_67db78); P(toggle_67db7c); P(toggle_67db80); P(toggle_67db84);
    P(toggle_67db88); P(toggle_67db8c); P(toggle_67db90); P(flag_67db70);
    P(flag_67db74); P(type_flag_67db6c); P(toggle_6e558c); P(toggle_6e5590);
    P(toggle_6e5594); P(menu_state_67dbbc); P(flags_67dbc8); P(flags_67dbc9);
    P(screenshot_67dbf8); P(screenshot_bfbb00); P(control_c23a00); P(pulse_c23a04);
    P(pulse_c23a08); P(pulse_c23a0c); P(camera_6ba424); P(camera_6ba428);
    P(camera_6ba42c);
#undef P
    std::cout << "look_yaw_30 look_pitch_34 stance_74 hud0 hud1 hud2 hud3 hud4 hud5\n";
}
std::uint32_t number(const std::string& text) {
    return std::uint32_t(std::strtoul(text.c_str(), nullptr, 0));
}
void print_state(const Case& c) {
    const auto& g = c.g;
    std::cout << "\"state\":{";
    bool first_state = true;
#define SEP() do { if (!first_state) std::cout << ','; first_state = false; } while (false)
#define P32(name) SEP(); std::cout << "\"" #name "\":" << g.name;
#define P8(name) SEP(); std::cout << "\"" #name "\":" << unsigned(g.name);
    P32(multiplayer_67f520); P32(mission_67f528); P32(mission2_67f52c);
    P32(menu_flag_67dbc0); P32(server_67dbb8); P32(resolution_index_67f4f4);
    P32(resolution_max_503134); P32(lb_size_67f4f8); P32(mouse_disabled_503130);
    P32(command_tick_6e471c); P32(tick_base_bd4844); P32(pause_6e4714);
    P32(pause2_6e4718); P32(toggle_6e558c); P32(toggle_6e5590); P32(toggle_6e5594);
    P32(saturation_67f4f0); P32(mouse_scale_67f500); P32(toggle_67f4fc);
    P32(toggle_67f504); P32(hud_color_index_67f508); P32(hud_color_value_67db58);
    P32(hud_detail_67f50c); P32(toggle_67f510); P32(toggle_67f514); P32(toggle_67f518);
    P32(toggle_67f564); P32(toggle_503428); P32(toggle_67db78); P32(toggle_67db7c);
    P32(toggle_67db80); P32(toggle_67db84); P32(toggle_67db88); P32(toggle_67db8c);
    P32(toggle_67db90); P32(flag_67db70); P32(flag_67db74); P32(type_flag_67db6c);
    P32(menu_state_67dbbc); P8(flags_67dbc8); P8(flags_67dbc9);
    P32(screenshot_67dbf8); P32(screenshot_bfbb00); P32(fo_toggle_c23a34);
    P32(fov_c23a28); P32(map_terrain_c23a2c); P32(map_elevation_c23a30);
    P32(camera_mode_bd0040); P32(control_c23a00); P32(pulse_c23a04);
    P32(pulse_c23a08); P32(pulse_c23a0c); P32(camera_6ba424); P32(camera_6ba428);
    P32(camera_6ba42c);
#undef P32
#undef P8
#undef SEP
    std::cout << "},\"actor\":{\"flags_6c\":" << c.a.flags_6c
              << ",\"look_yaw_30\":" << c.a.look_yaw_30
              << ",\"look_pitch_34\":" << c.a.look_pitch_34
              << ",\"stance_74\":" << c.a.stance_74 << "}";
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "fields") { print_fields(); return 0; }
        if (argc == 2 && std::string(argv[1]) == "kinds") {
            std::cout << "ui_button\ndialog\ndialog_text\nmessage\nformat_int\ngame_event\n"
                         "weapon_fire\nweapon_reload\nweapon_select\nweapon_cycle\nweapon_aim\n"
                         "weapon_supply\nweapon_unknown\nscreen_mode\nactor_call\n"
                         "multiplayer_call\nteam_check\nmission_call\ncamera_mode\ncamera_picture\n"
                         "screenshot\nunknown\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "commands") {
            const dfre::CommandTable commands;
            std::cout << '[';
            bool first = true;
            for (const auto& row : commands.rows()) {
                if (!first) std::cout << ','; first = false;
                std::cout << "{\"id\":" << row.id << ",\"key\":" << row.key
                          << ",\"mouse\":" << row.mouse << ",\"joystick\":" << row.joystick
                          << ",\"flags\":" << row.flags << '}';
            }
            std::cout << "]\n";
            return 0;
        }
        if (argc != 2 || std::string(argv[1]) != "run")
            throw dfre::AssetError("gameplay_dispatch_probe fields|kinds|commands|run");
        const dfre::CommandTable commands;
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            std::istringstream input(line);
            std::vector<std::uint32_t> token;
            std::string word;
            while (input >> word) token.push_back(number(word));
            if (token.size() != 66) throw dfre::AssetError("wrong token count");
            Case c;
            std::size_t at = 0;
            c.id = token[at++];
            c.value = std::int32_t(token[at++]);
            c.a.flags_6c = token[at++];
            auto& g = c.g;
#define P(name) g.name = token[at++];
            P(pause_6e4714); P(pause2_6e4718); P(menu_flag_67dbc0); P(multiplayer_67f520);
            P(mission_67f528); P(mission2_67f52c); P(server_67dbb8); P(command_tick_6e471c);
            P(tick_base_bd4844); P(resolution_index_67f4f4); P(resolution_max_503134);
            P(lb_size_67f4f8); P(mouse_disabled_503130); P(camera_mode_bd0040);
            P(saturation_67f4f0); P(mouse_scale_67f500); P(fov_c23a28); P(map_terrain_c23a2c);
            P(map_elevation_c23a30); P(fo_toggle_c23a34); P(hud_color_index_67f508);
            P(hud_detail_67f50c); P(toggle_67f4fc); P(toggle_67f504); P(toggle_67f510);
            P(toggle_67f514); P(toggle_67f518); P(toggle_67f564); P(toggle_503428);
            P(toggle_67db78); P(toggle_67db7c); P(toggle_67db80); P(toggle_67db84);
            P(toggle_67db88); P(toggle_67db8c); P(toggle_67db90); P(flag_67db70);
            P(flag_67db74); P(type_flag_67db6c); P(toggle_6e558c); P(toggle_6e5590);
            P(toggle_6e5594); P(menu_state_67dbbc); P(flags_67dbc8); P(flags_67dbc9);
            P(screenshot_67dbf8); P(screenshot_bfbb00); P(control_c23a00); P(pulse_c23a04);
            P(pulse_c23a08); P(pulse_c23a0c); P(camera_6ba424); P(camera_6ba428);
            P(camera_6ba42c);
#undef P
            c.a.look_yaw_30 = token[at++];
            c.a.look_pitch_34 = token[at++];
            c.a.stance_74 = token[at++];
            for (auto& slot : g.hud_color_table_67db40) slot = token[at++];
            const auto result = dfre::dispatch_game_command(c.id, c.value, c.g, c.a, commands);
            std::cout << "{\"gated\":" << (result.gated ? 1 : 0) << ',';
            print_state(c);
            std::cout << ",\"calls\":[";
            bool first = true;
            for (const auto& item : result.calls) {
                if (!first) std::cout << ','; first = false;
                std::cout << "{\"kind\":" << unsigned(item.kind) << ",\"address\":" << item.address
                          << ",\"threaded\":" << (item.threaded_result ? 1 : 0) << ",\"args\":[";
                for (std::size_t i = 0; i < item.arguments.size(); ++i) {
                    if (i) std::cout << ',';
                    std::cout << item.arguments[i];
                }
                std::cout << "]}";
            }
            std::cout << "]}\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
