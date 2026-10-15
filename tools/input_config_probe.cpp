#include "dfre/assets.hpp"
#include "dfre/input_config.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream source(path, std::ios::binary);
    if (!source) throw dfre::AssetError("Cannot read " + path.string());
    std::ostringstream buffer;
    buffer << source.rdbuf();
    return buffer.str();
}

std::string json_escape(std::string_view text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += "\\u00";
                const char* hex = "0123456789abcdef";
                out += hex[(c >> 4) & 0xf];
                out += hex[c & 0xf];
            } else out += c;
        }
    }
    return out;
}

void print_config(const dfre::GameConfig& config) {
    bool first = true;
    const auto i=[&](const char* name, std::int32_t value) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << '"' << name << "\":" << value;
    };
    const auto s=[&](const char* name, const std::string& value) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << '"' << name << "\":\"" << json_escape(value) << '"';
    };
    std::cout << '{';
    i("cd_play", config.cd_play); i("menu_track", config.menu_track); i("game_track", config.game_track);
    i("cd_volume", config.cd_volume); i("fx_volume", config.fx_volume); i("dialog_volume", config.dialog_volume);
    i("game_res", config.game_res); i("window_size", config.window_size); i("windowed", config.windowed);
    i("renderer", config.renderer); i("max_res", config.max_res); i("gamma", config.gamma); i("turbo", config.turbo);
    i("flipmouse", config.flipmouse); i("crosshairs", config.crosshairs); i("mousescale", config.mousescale);
    i("showgun", config.showgun); i("picinpic", config.picinpic);
    s("login_password", config.login_password); s("game_password", config.game_password);
    s("squad_password", config.squad_password); s("game_name", config.game_name);
    s("phone_number", config.phone_number); s("internet_address", config.internet_address);
    i("mp_service", config.mp_service); i("mp_gametype", config.mp_gametype); i("maxplayers", config.maxplayers);
    i("dedicated", config.dedicated); i("com_port", config.com_port); i("baud_rate", config.baud_rate);
    i("max_lives", config.max_lives); i("max_kills", config.max_kills); i("team_req", config.team_req);
    i("attributes", config.attributes); i("replay", config.replay); i("time_limit", config.time_limit);
    i("koth_limit", config.koth_limit); i("ammo_respawn", config.ammo_respawn); i("timeout", config.timeout);
    i("gpsicon", config.gpsicon); i("showtags", config.showtags); i("contest", config.contest);
    i("map_infrared", config.map_infrared); i("map_iff", config.map_iff); i("map_AWAC", config.map_awac);
    i("preempt_pff", config.preempt_pff); i("whichbrowser", config.whichbrowser);
    s("country", config.country); s("red_password", config.red_password); s("blue_password", config.blue_password);
    i("mp_verbose", config.mp_verbose);
    std::cout << '}';
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) throw dfre::AssetError("input_config_probe GAME_FOLDER [--config PATH] [--keys PATH] [--cpu-family N]");
        const std::filesystem::path game = argv[1];
        std::filesystem::path config_path, keys_path;
        std::int32_t cpu_family = 6;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) config_path = argv[++i];
            else if (arg == "--keys" && i + 1 < argc) keys_path = argv[++i];
            else if (arg == "--cpu-family" && i + 1 < argc) cpu_family = std::stoi(argv[++i]);
            else throw dfre::AssetError("Unknown argument: " + arg);
        }

        dfre::GameConfig config = dfre::default_game_config(cpu_family);
        std::size_t matched = 0;
        std::cout << '{';
        if (!config_path.empty()) {
            matched = dfre::load_game_config_text(config, read_text_file(config_path));
            std::cout << "\"config_source\":\"" << json_escape(config_path.string()) << "\",";
        } else {
            const auto installed = game / "df.cfg";
            if (std::filesystem::exists(installed)) {
                matched = dfre::load_game_config_text(config, read_text_file(installed));
                std::cout << "\"config_source\":\"" << json_escape(installed.string()) << "\",";
            } else {
                std::cout << "\"config_source\":\"defaults\",";
            }
        }
        std::cout << "\"config_matched\":" << matched << ",\"config\":";
        print_config(config);

        // The game table at 0x4e7ddc starts zeroed and is filled only from the
        // file, so rows the file does not mention stay zero.
        dfre::ActionBindings actions{};
        dfre::ActionLoadResult action_load;
        if (!keys_path.empty()) {
            action_load = dfre::load_action_bindings_text(actions, read_text_file(keys_path));
        } else {
            const dfre::AssetStore assets(game);
            const auto bytes = assets.read("dfdk.key");
            action_load = dfre::load_action_bindings_text(actions, std::string(bytes.bytes.begin(), bytes.bytes.end()));
        }
        std::cout << ",\"action_load\":{\"section_found\":" << (action_load.section_found ? "true" : "false")
                  << ",\"applied\":" << action_load.applied << ",\"rejected\":" << action_load.rejected << "},\"actions\":[";
        for (std::size_t id = 0; id < dfre::action_count; ++id) {
            if (id) std::cout << ',';
            std::cout << "{\"id\":" << id << ",\"name\":\"" << json_escape(dfre::action_name(id))
                      << "\",\"key\":" << actions[id].key << ",\"mouse\":" << actions[id].mouse
                      << ",\"joystick\":" << actions[id].joystick << '}';
        }
        std::cout << "],\"actions_equal_shipped_defaults\":"
                  << (actions == dfre::default_action_bindings() ? "true" : "false");

        std::cout << ",\"hotkeys\":[";
        bool first = true;
        for (const auto& command : dfre::hotkey_commands()) {
            if (!first) std::cout << ',';
            first = false;
            std::cout << "{\"id\":" << command.id << ",\"name\":\"" << json_escape(command.name)
                      << "\",\"key\":" << command.key << ",\"mouse\":" << command.mouse
                      << ",\"joystick\":" << command.joystick << ",\"flags\":" << command.flags << '}';
        }
        std::cout << "]}";
        std::cout << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
