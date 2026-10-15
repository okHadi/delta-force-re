#include "dfre/input_config.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void check_defaults(std::int32_t cpu_family, std::int32_t renderer, std::int32_t max_res) {
    const auto config = dfre::default_game_config(cpu_family);
    require(config.cd_play == 0 && config.menu_track == 2 && config.game_track == 2, "Music defaults");
    require(config.cd_volume == 255 && config.fx_volume == 255 && config.dialog_volume == 200, "Volume defaults");
    require(config.game_res == 3 && config.window_size == 1 && config.windowed == 0, "Display defaults");
    require(config.renderer == renderer && config.max_res == max_res, "CPU family defaults");
    require(config.gamma == 0x80 && config.turbo == 0, "Gamma and turbo defaults");
    require(config.flipmouse == 0 && config.crosshairs == 1 && config.mousescale == 0x80, "Mouse defaults");
    require(config.showgun == 1 && config.picinpic == 0, "Gun defaults");
    require(config.login_password.empty() && config.game_password.empty() && config.squad_password.empty(), "Password defaults");
    require(config.game_name.empty() && config.phone_number.empty(), "Name and phone defaults");
    require(config.internet_address == "0.0.0.0", "Internet address default");
    require(config.mp_service == 4 && config.mp_gametype == 0 && config.maxplayers == 0x10, "Multiplayer defaults");
    require(config.dedicated == 0 && config.com_port == 0 && config.baud_rate == 0, "Serial defaults");
    require(config.max_lives == 20 && config.max_kills == 20 && config.team_req == 1, "Rules defaults");
    require(config.attributes == 2 && config.replay == 0, "Attributes defaults");
    require(config.time_limit == 30 && config.koth_limit == 30 && config.ammo_respawn == 2, "Timing defaults");
    require(config.timeout == 0 && config.gpsicon == 1 && config.showtags == 0 && config.contest == 0, "GPS defaults");
    require(config.map_infrared == 1 && config.map_iff == 1 && config.map_awac == 1, "Map defaults");
    require(config.preempt_pff == 0 && config.whichbrowser == 0 && config.mp_verbose == 1, "System defaults");
    require(config.country.empty() && config.red_password.empty() && config.blue_password.empty(), "System text defaults");
    require(config.runtime_ced58 == 0 && config.runtime_ced60 == 0 && config.runtime_ced64 == 0 && config.runtime_ced68 == 0, "Runtime zero defaults");
    require(config.window_scale == 0x10000 && config.map_scale_lo == 0 && config.map_scale_hi == 1, "Scale defaults");
}

void check_parser() {
    {
        auto config = dfre::default_game_config(6);
        const std::string text =
            "// MUSIC\r\n"
            "gamma            = 7\r\n"
            "GAMMA = 8\r\n"
            "gamma 9\r\n"
            "  gamma = 10\r\n"
            "unknown_key = 55\n"
            "\n"
            "// trailing comment\n";
        // "GAMMA=8" would stay one token and never match; "gamma 9" has no third
        // token and re-reads the earlier third-token slot; a key with leading
        // spaces does match because spaces separate tokens.
        require(dfre::load_game_config_text(config, text) == 4, "Matched gamma lines");
        require(config.gamma == 10, "Last matching line wins");
    }
    {
        auto config = dfre::default_game_config(6);
        const std::string text =
            "gamma            = 7\r\n"
            "GAMMA=8\r\n";
        require(dfre::load_game_config_text(config, text) == 1, "Only the spaced assignment matches");
        require(config.gamma == 7, "GAMMA=8 is a single token and does not match");
    }
    {
        auto config = dfre::default_game_config(6);
        const std::string text = "gamma = 5\ngamma 9\n";
        require(dfre::load_game_config_text(config, text) == 2, "Two gamma lines");
        require(config.gamma == 5, "Two-token line re-reads the earlier third token");
    }
    {
        auto config = dfre::default_game_config(6);
        const std::string text = "gamma = 1,2\nmousescale = \"3\" stuff\n";
        require(dfre::load_game_config_text(config, text) == 2, "Comma and quote matches");
        require(config.gamma == 1, "First numeric token after '=' is the value");
        require(config.mousescale == 3, "Quoted value then trailing text still converts");
    }
    {
        auto config = dfre::default_game_config(6);
        require(dfre::load_game_config_text(config, "maxplayers = 99\n") == 1, "Maxplayers match");
        require(config.maxplayers == 16, "Maxplayers clamps to 0x10");
        require(dfre::load_game_config_text(config, "maxplayers = -4\n") == 1, "Negative maxplayers match");
        require(config.maxplayers == -4, "Negative maxplayers is kept");
    }
    {
        auto config = dfre::default_game_config(6);
        require(dfre::load_game_config_text(config, "gamma = 4294967296\n") == 1, "Wrap match");
        require(config.gamma == 0, "atoi wraps at 32 bits");
        require(dfre::load_game_config_text(config, "gamma = 2147483648\n") == 1, "Sign match");
        require(config.gamma == static_cast<std::int32_t>(0x80000000u), "atoi keeps the low 32 bits");
        require(dfre::load_game_config_text(config, "gamma = -5\n") == 1, "Negative value match");
        require(config.gamma == -5, "Negative values are kept");
    }
    {
        auto config = dfre::default_game_config(6);
        const std::string text =
            "login_password = \"low case\"\n"
            "game_password = \"Low Case\"\n"
            "squad_password = \"Low Case\"\n"
            "game_name = \"low case\"\n"
            "country = \"usa\"\n"
            "red_password = \"low case\"\n"
            "blue_password = \"low case\"\n"
            "internet_address = \"10.0.0.1\"\n";
        require(dfre::load_game_config_text(config, text) == 8, "String matches");
        require(config.login_password == "LOW CASE", "login_password uppercases");
        require(config.game_password == "LOW CASE", "game_password uppercases");
        require(config.squad_password == "LOW CASE", "squad_password uppercases");
        require(config.game_name == "low case", "game_name keeps case");
        require(config.country == "usa", "country keeps case");
        require(config.red_password == "LOW CASE", "red_password uppercases");
        require(config.blue_password == "LOW CASE", "blue_password uppercases");
        require(config.internet_address == "10.0.0.1", "Quoted address keeps case");
    }
    {
        auto config = dfre::default_game_config(6);
        config.game_name = "Old Name";
        // The tokenizer also processes the terminating NUL, so a line that ends
        // with a separator or a closing quote gains one last empty token.
        require(dfre::load_game_config_text(config, "game_name = \"\"\n") == 1, "Empty quoted value match");
        require(config.game_name.empty(), "Empty quoted value clears the field");
        require(dfre::load_game_config_text(config, "gamma = \n") == 1, "Trailing separator match");
        require(config.gamma == 0, "Trailing separator makes an empty value");
        config.game_name = "Old Name";
        // "= \"\"\r" gives the tokenizer a third token: the carriage return that
        // follows the closing quote. The shipped CRLF df.cfg behaves this way.
        require(dfre::load_game_config_text(config, "game_name = \"\"\r\n") == 1, "CRLF empty quoted value match");
        require(config.game_name == "\r", "Carriage return after the closing quote becomes the value");
    }
}

void check_save() {
    dfre::GameConfig config = dfre::default_game_config(6);
    config.cd_volume = 200;
    config.internet_address = "1.2.3.4";
    config.country = "usa";
    const std::string text = dfre::save_game_config_text(config);
    require(text.find("cd_volume        = 200\n") != std::string::npos, "Padded integer line");
    require(text.find("internet_address = \"1.2.3.4\"\n") != std::string::npos, "Quoted string line");
    require(text.find("country\t\t\t = \"usa\"\n") != std::string::npos, "Country uses the original tab format");
    require(text.rfind("\n\n") == text.size() - 2, "File ends with a blank line");
    auto reloaded = dfre::default_game_config(0);
    require(dfre::load_game_config_text(reloaded, text) == 51, "Saved text loads all fifty-one keys");
    require(reloaded.cd_volume == 200 && reloaded.internet_address == "1.2.3.4", "Round-trip values");
    config.game_name = "A Squad";
    config.red_password = "secret";
    auto again = dfre::default_game_config(0);
    require(dfre::load_game_config_text(again, dfre::save_game_config_text(config)) == 51, "Second round trip");
    require(again.game_name == "A Squad" && again.red_password == "SECRET", "Second round-trip values");
}

void check_actions() {
    const auto defaults = dfre::default_action_bindings();
    const std::uint32_t expected[dfre::action_count][3] = {
        {37,0,0},{39,0,0},{38,0,0},{40,0,0},{34,3,0},{35,0,0},{46,0,0},{32,0,0},
        {17,1,0},{83,2,0},{16,0,0},{8,0,0},{66,0,0},{49,0,0},{50,0,0},{51,0,0},
        {52,0,0},{53,0,0},{54,0,0},{55,0,0},{56,0,0},{57,0,0},{48,0,0},{187,0,0},
        {189,0,0},{84,0,0},{89,0,0},{87,0,0},{65,0,0},{86,0,0},{68,0,0},{77,0,0},
        {104,0,0},{98,0,0},{102,0,0},{100,0,0},{9,0,0},{79,0,0},{82,0,0},{70,0,0},
        {71,0,0},{67,0,0},{72,0,0},{81,0,0},{69,0,0},{36,0,0},
    };
    for (std::size_t id = 0; id < dfre::action_count; ++id) {
        require(defaults[id].key == expected[id][0] && defaults[id].mouse == expected[id][1] &&
                defaults[id].joystick == expected[id][2], "Shipped action defaults");
    }
    require(dfre::action_name(4) == "Go Prone" && dfre::action_name(45) == "Server Screen", "Action names");
    require(dfre::action_name(46).empty(), "Unknown action name");

    {
        const std::string text =
            "; comment\r\n"
            "KEY=0,1,2,3\r\n"
            "[OPTIONS]\r\n"
            "KEY=5,9,9,9\r\n"
            "[KEYS]\r\n"
            "; FORMAT comment\r\n"
            "KEY = 1, 100, 2, 3\r\n"
            "key=2,50,0,0\r\n"
            "KEY=3,51,52\r\n"
            "KEY=47,1,1,1\r\n"
            "KEY=2,60,0,0   ; last wins\r\n"
            "\r\n"
            "KEY=4, bogus ,0,0\r\n"
            "[NEXT]\r\n"
            "KEY=6,1,1,1\r\n";
        auto bindings = dfre::default_action_bindings();
        const auto result = dfre::load_action_bindings_text(bindings, text);
        require(result.section_found, "KEYS section found");
        require(result.applied == 4, "Applied key lines");
        require(result.rejected == 2, "Rejected malformed and out-of-range key lines");
        require(bindings[0].key == 37, "Lines before [KEYS] leave the table alone");
        require(bindings[1].key == 100 && bindings[1].mouse == 2 && bindings[1].joystick == 3, "Spaced fields parse");
        require(bindings[2].key == 60 && bindings[2].mouse == 0, "Duplicate index overwrites");
        require(bindings[3].key == 40, "Missing fields are rejected");
        require(bindings[4].key == 0, "bogus fields convert through atoi");
        require(bindings[5].key == 35, "Skipped lines keep the shipped default");
        require(bindings[6].key == 46, "Lines after the next section are ignored");
    }
    {
        auto bindings = dfre::default_action_bindings();
        require(!dfre::load_action_bindings_text(bindings, "KEY=0,1,1,1\r\n").section_found, "Section is required");
        // 0x49b010 uppercases the whole file, so a lowercase section matches.
        require(dfre::load_action_bindings_text(bindings, "[keys]\r\nkey=0,9,8,7\r\n").section_found, "Lowercase section matches");
        require(bindings[0].key == 9 && bindings[0].mouse == 8 && bindings[0].joystick == 7, "Lowercase key line matches");
        // Without any CRLF pair the loader reads no strings at all.
        require(!dfre::load_action_bindings_text(bindings, "[KEYS]\nKEY=1,2,3,4\n").section_found, "LF-only file loads nothing");
        // Only as many strings as CRLF pairs are read.
        require(dfre::load_action_bindings_text(bindings, "[KEYS]\r\nKEY=1,2,3,4\nKEY=5,6,7,8\n").applied == 0,
                "Line without CRLF is not read");
        // Tabs become spaces before the reader sees the line.
        auto tabs = dfre::default_action_bindings();
        require(dfre::load_action_bindings_text(tabs, "[KEYS]\r\nKEY\t=\t2\t,\t10\t,\t1\t,\t0\r\n").applied == 1,
                "Tab separators load");
        require(tabs[2].key == 10 && tabs[2].mouse == 1, "Tab fields parse");
    }
}

void check_hotkeys() {
    const auto& commands = dfre::hotkey_commands();
    require(commands.size() == 121, "Command table row count");
    const auto* scope = dfre::find_hotkey_command(6);
    require(scope != nullptr && scope->name == "scope" && scope->key == 83 && scope->mouse == 2 && scope->flags == 1, "Scope row");
    const auto* forward = dfre::find_hotkey_command(143);
    require(forward != nullptr && forward->name == "move_forward" && forward->key == 38, "Move forward row");
    const auto* prone = dfre::find_hotkey_command(162);
    require(prone != nullptr && prone->key == 34 && prone->mouse == 16 && prone->flags == 1, "Prone row uses the 0x10 middle bit");
    const auto* camera = dfre::find_hotkey_command(414);
    require(camera != nullptr && camera->key == 219 && camera->flags == 21, "Camera zoom row");
    require(dfre::find_hotkey_command(999) == nullptr, "Unknown command id");

    std::array<std::uint8_t,256> keys{};
    require(!dfre::hotkey_active(*scope,keys,0,0), "Idle hotkey");
    keys[83] = 1;
    require(dfre::hotkey_active(*scope,keys,0,0), "Keyboard hotkey");
    keys[83] = 0;
    require(dfre::hotkey_active(*scope,keys,dfre::mouse_right,0), "Right mouse hotkey");
    require(!dfre::hotkey_active(*scope,keys,dfre::mouse_left,0), "Left mouse does not fire scope");
    require(!dfre::hotkey_active(*scope,keys,dfre::mouse_middle,0), "Middle mouse does not fire scope");
    auto scope_copy = *scope;
    scope_copy.key = 0;
    require(!dfre::hotkey_active(scope_copy,keys,0,0), "Key zero is not evaluated");
    const auto* attack = dfre::find_hotkey_command(140);
    require(dfre::hotkey_active(*attack,keys,0,1), "Joystick hotkey");
    scope_copy.key = 300;
    require(!dfre::hotkey_active(scope_copy,keys,0,0), "Out-of-range key stays inactive");
}

} // namespace

int main() {
    try {
        check_defaults(5,0,3);
        check_defaults(6,1,4);
        check_defaults(15,1,4);
        check_parser();
        check_save();
        check_actions();
        check_hotkeys();
        std::cout << "input_config checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
