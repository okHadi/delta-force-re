#include "dfre/lobby.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace {
bool require(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

const char* expected_default_lines() {
    return "login_password   = \"\"\n"
           "game_password    = \"\"\n"
           "squad_password   = \"\"\n"
           "game_name        = \"\"\n"
           "phone_number     = \"\"\n"
           "internet_address = \"0.0.0.0\"\n"
           "mp_service       = 4\n"
           "mp_gametype      = 0\n"
           "maxplayers       = 16\n"
           "dedicated        = 0\n"
           "com_port         = 0\n"
           "baud_rate        = 0\n"
           "max_lives        = 20\n"
           "max_kills        = 20\n"
           "team_req         = 1\n"
           "attributes       = 2\n"
           "replay           = 0\n"
           "time_limit       = 30\n"
           "koth_limit       = 30\n"
           "ammo_respawn     = 2\n"
           "timeout          = 0\n"
           "gpsicon          = 1\n"
           "showtags         = 0\n"
           "contest          = 0\n"
           "country\t\t\t = \"\"\n"
           "red_password     = \"\"\n"
           "blue_password    = \"\"\n"
           "mp_verbose       = 1\n";
}
} // namespace

int main() {
    using dfre::MultiplayerSettings;

    if (!require(sizeof(MultiplayerSettings) == dfre::kMultiplayerSettingsSize,
                 "Record size must stay 0x5c4.")) return 1;

    const MultiplayerSettings defaults = dfre::multiplayer_defaults();

    // Layout probes that also pin the original global addresses 0x4ced94..0x4cf2e0.
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&defaults);
    if (!require(bytes[0x148] == 4, "mp_service default must be 4.")) return 1;
    if (!require(bytes[0x150] == 16, "maxplayers default must be 16.")) return 1;
    if (!require(bytes[0x160] == 20 && bytes[0x164] == 20, "Lives and kills default to 20.")) return 1;
    if (!require(bytes[0x168] == 1, "team_req default must be 1.")) return 1;
    if (!require(bytes[0x16c] == 2, "attributes default must be 2.")) return 1;
    if (!require(bytes[0x178] == 30 && bytes[0x17c] == 30, "Limits default to 30.")) return 1;
    if (!require(bytes[0x180] == 2, "ammo_respawn default must be 2.")) return 1;
    if (!require(bytes[0x188] == 1, "gpsicon default must be 1.")) return 1;
    if (!require(bytes[0x5b4] == 1, "flag_5b4 default must be 1.")) return 1;
    if (!require(bytes[0x5c0] == 1, "mp_verbose default must be 1.")) return 1;

    if (!require(defaults.mp_gametype == 0 && defaults.dedicated == 0 &&
                 defaults.com_port == 0 && defaults.baud_rate == 0 &&
                 defaults.replay == 0 && defaults.timeout == 0 &&
                 defaults.showtags == 0 && defaults.contest == 0 &&
                 defaults.whichbrowser == 0 && defaults.turbo == 0,
                 "Unlisted multiplayer fields default to zero.")) return 1;
    if (!require(defaults.unset_170 == 0, "unset_170 default must be zero.")) return 1;
    if (!require(dfre::settings_text(defaults.internet_address.data(),
                                     defaults.internet_address.size()) == "0.0.0.0",
                 "internet_address default must be 0.0.0.0.")) return 1;
    if (!require(dfre::settings_text(defaults.login_password.data(),
                                     defaults.login_password.size()).empty(),
                 "login_password default must be empty.")) return 1;
    if (!require(dfre::settings_text(defaults.country.data(), defaults.country.size()).empty(),
                 "country default must be empty.")) return 1;
    for (std::uint8_t byte : defaults.head) {
        if (!require(byte == 0, "head must stay zero.")) return 1;
    }
    for (std::uint8_t byte : defaults.untouched) {
        if (!require(byte == 0, "untouched must stay zero.")) return 1;
    }
    for (char byte : defaults.game_name) {
        if (!require(byte == 0, "game_name must stay empty.")) return 1;
    }

    if (!require(dfre::multiplayer_config_lines(defaults) == expected_default_lines(),
                 "Default config lines differ from the original writer output.")) return 1;

    MultiplayerSettings custom;
    dfre::set_settings_text(custom.game_name.data(), custom.game_name.size(), "Delta \"One\" 100%");
    dfre::set_settings_text(custom.login_password.data(), custom.login_password.size(), "login");
    dfre::set_settings_text(custom.squad_password.data(), custom.squad_password.size(), "squad");
    dfre::set_settings_text(custom.red_password.data(), custom.red_password.size(), "red");
    dfre::set_settings_text(custom.blue_password.data(), custom.blue_password.size(), "blue");
    dfre::set_settings_text(custom.country.data(), custom.country.size(), "US");
    dfre::set_settings_text(custom.internet_address.data(), custom.internet_address.size(),
                            "127.0.0.1");
    custom.mp_service = 2;
    custom.mp_gametype = 5;
    custom.maxplayers = 8;
    custom.dedicated = 1;
    custom.max_lives = 0;
    custom.max_kills = 0xff;
    custom.attributes = 2306;
    custom.time_limit = 45;
    custom.koth_limit = 15;
    custom.ammo_respawn = 0;
    custom.gpsicon = 2;
    custom.mp_verbose = 0;
    const std::string custom_lines = dfre::multiplayer_config_lines(custom);
    if (!require(custom_lines.find("game_name        = \"Delta \"One\" 100%\"\n") != std::string::npos,
                 "Quoted game_name must copy through.")) return 1;
    if (!require(custom_lines.find("internet_address = \"127.0.0.1\"\n") != std::string::npos,
                 "Custom address must copy through.")) return 1;
    if (!require(custom_lines.find("country\t\t\t = \"US\"\n") != std::string::npos,
                 "Country line must keep original tabs.")) return 1;
    if (!require(custom_lines.find("attributes       = 2306\n") != std::string::npos,
                 "attributes must print as a decimal int.")) return 1;
    if (!require(custom_lines.find("max_kills        = 255\n") != std::string::npos,
                 "max_kills must print as a decimal int.")) return 1;
    if (!require(custom_lines.find("mp_verbose       = 0\n") != std::string::npos,
                 "mp_verbose must print as a decimal int.")) return 1;
    if (!require(custom_lines.find("mp_gametype      = 5\n") != std::string::npos,
                 "mp_gametype must print as a decimal int.")) return 1;

    // Text field edge cases.
    char field[8] = {};
    dfre::set_settings_text(field, sizeof field, "12345678");
    if (!require(std::string(field, 7) == "1234567" && field[7] == '\0',
                 "Long text must truncate and terminate.")) return 1;
    dfre::set_settings_text(field, sizeof field, "");
    if (!require(field[0] == '\0', "Empty text must clear the field.")) return 1;
    field[0] = 'x';
    dfre::set_settings_text(field, 0, "y");
    if (!require(field[0] == 'x', "Zero capacity must not write.")) return 1;
    char unterminated[4] = {'a', 'b', 'c', 'd'};
    if (!require(dfre::settings_text(unterminated, sizeof unterminated) == "abcd",
                 "Missing NUL must stop at capacity.")) return 1;
    return 0;
}
