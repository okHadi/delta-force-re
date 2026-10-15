#include "dfre/lobby.hpp"

#include <cstdio>
#include <cstring>

namespace dfre {
namespace {

// Format strings copied byte for byte from the original df.cfg writer at
// 0x407360. Address after each one is the original string location.
constexpr const char* kLoginPasswordFormat = "login_password   = \"%s\"\n";    // 0x4c8894
constexpr const char* kGamePasswordFormat = "game_password    = \"%s\"\n";     // 0x4c88b0
constexpr const char* kSquadPasswordFormat = "squad_password   = \"%s\"\n";    // 0x4c88cc
constexpr const char* kGameNameFormat = "game_name        = \"%s\"\n";         // 0x4c88e8
constexpr const char* kPhoneNumberFormat = "phone_number     = \"%s\"\n";      // 0x4c8904
constexpr const char* kInternetAddressFormat = "internet_address = \"%s\"\n";  // 0x4c8920
constexpr const char* kMpServiceFormat = "mp_service       = %i\n";            // 0x4c893c
constexpr const char* kMpGametypeFormat = "mp_gametype      = %i\n";           // 0x4c8954
constexpr const char* kMaxplayersFormat = "maxplayers       = %i\n";           // 0x4c896c
constexpr const char* kDedicatedFormat = "dedicated        = %i\n";            // 0x4c8984
constexpr const char* kComPortFormat = "com_port         = %i\n";              // 0x4c899c
constexpr const char* kBaudRateFormat = "baud_rate        = %i\n";             // 0x4c89b4
constexpr const char* kMaxLivesFormat = "max_lives        = %i\n";             // 0x4c89cc
constexpr const char* kMaxKillsFormat = "max_kills        = %i\n";             // 0x4c89e4
constexpr const char* kTeamReqFormat = "team_req         = %i\n";              // 0x4c89fc
constexpr const char* kAttributesFormat = "attributes       = %i\n";           // 0x4c8a14
constexpr const char* kReplayFormat = "replay           = %i\n";               // 0x4c8a2c
constexpr const char* kTimeLimitFormat = "time_limit       = %i\n";            // 0x4c8a44
constexpr const char* kKothLimitFormat = "koth_limit       = %i\n";            // 0x4c8a5c
constexpr const char* kAmmoRespawnFormat = "ammo_respawn     = %i\n";          // 0x4c8a74
constexpr const char* kTimeoutFormat = "timeout          = %i\n";              // 0x4c8a8c
constexpr const char* kGpsiconFormat = "gpsicon          = %i\n";              // 0x4c8aa4
constexpr const char* kShowtagsFormat = "showtags         = %i\n";             // 0x4c8abc
constexpr const char* kContestFormat = "contest          = %i\n";              // 0x4c8ad4
constexpr const char* kCountryFormat = "country\t\t\t = \"%s\"\n";             // 0x4c8b78
constexpr const char* kRedPasswordFormat = "red_password     = \"%s\"\n";      // 0x4c8b8c
constexpr const char* kBluePasswordFormat = "blue_password    = \"%s\"\n";     // 0x4c8ba8
constexpr const char* kMpVerboseFormat = "mp_verbose       = %i\n";            // 0x4c8bc4

std::size_t text_length(const char* field, std::size_t capacity) {
    std::size_t length = 0;
    while (length < capacity && field[length] != '\0') {
        ++length;
    }
    return length;
}

std::string bounded_text(const char* field, std::size_t capacity) {
    return std::string(field, text_length(field, capacity));
}

void append_text_line(std::string& out, const char* format, const char* field, std::size_t capacity) {
    char value[40] = {};
    std::size_t length = text_length(field, capacity);
    if (length > sizeof value - 1) {
        length = sizeof value - 1;
    }
    std::memcpy(value, field, length);
    value[length] = '\0';
    char line[160];
    std::snprintf(line, sizeof line, format, value);
    out += line;
}

void append_int_line(std::string& out, const char* format, std::int32_t value) {
    char line[160];
    std::snprintf(line, sizeof line, format, static_cast<int>(value));
    out += line;
}

// Original copies: internet_address comes from the string at 0x4c8690;
// country, red_password, and blue_password come from 0x4c8698.
constexpr const char* kDefaultInternetAddress = "0.0.0.0";

} // namespace

MultiplayerSettings multiplayer_defaults() noexcept {
    MultiplayerSettings settings;
    settings.mp_service = 4;
    settings.mp_gametype = 0;
    settings.maxplayers = 16;
    settings.dedicated = 0;
    settings.com_port = 0;
    settings.baud_rate = 0;
    settings.max_lives = 20;
    settings.max_kills = 20;
    settings.team_req = 1;
    settings.attributes = 2;
    settings.replay = 0;
    settings.time_limit = 30;
    settings.koth_limit = 30;
    settings.ammo_respawn = 2;
    settings.timeout = 0;
    settings.gpsicon = 1;
    settings.showtags = 0;
    settings.contest = 0;
    settings.flag_5b4 = 1;
    settings.whichbrowser = 0;
    settings.turbo = 0;
    settings.mp_verbose = 1;
    set_settings_text(settings.internet_address.data(), settings.internet_address.size(),
                      kDefaultInternetAddress);
    return settings;
}

std::string multiplayer_config_lines(const MultiplayerSettings& settings) {
    std::string out;
    append_text_line(out, kLoginPasswordFormat, settings.login_password.data(),
                     settings.login_password.size());
    append_text_line(out, kGamePasswordFormat, settings.game_password.data(),
                     settings.game_password.size());
    append_text_line(out, kSquadPasswordFormat, settings.squad_password.data(),
                     settings.squad_password.size());
    append_text_line(out, kGameNameFormat, settings.game_name.data(), settings.game_name.size());
    append_text_line(out, kPhoneNumberFormat, settings.phone_number.data(),
                     settings.phone_number.size());
    append_text_line(out, kInternetAddressFormat, settings.internet_address.data(),
                     settings.internet_address.size());
    append_int_line(out, kMpServiceFormat, settings.mp_service);
    append_int_line(out, kMpGametypeFormat, settings.mp_gametype);
    append_int_line(out, kMaxplayersFormat, settings.maxplayers);
    append_int_line(out, kDedicatedFormat, settings.dedicated);
    append_int_line(out, kComPortFormat, settings.com_port);
    append_int_line(out, kBaudRateFormat, settings.baud_rate);
    append_int_line(out, kMaxLivesFormat, settings.max_lives);
    append_int_line(out, kMaxKillsFormat, settings.max_kills);
    append_int_line(out, kTeamReqFormat, settings.team_req);
    append_int_line(out, kAttributesFormat, settings.attributes);
    append_int_line(out, kReplayFormat, settings.replay);
    append_int_line(out, kTimeLimitFormat, settings.time_limit);
    append_int_line(out, kKothLimitFormat, settings.koth_limit);
    append_int_line(out, kAmmoRespawnFormat, settings.ammo_respawn);
    append_int_line(out, kTimeoutFormat, settings.timeout);
    append_int_line(out, kGpsiconFormat, settings.gpsicon);
    append_int_line(out, kShowtagsFormat, settings.showtags);
    append_int_line(out, kContestFormat, settings.contest);
    append_text_line(out, kCountryFormat, settings.country.data(), settings.country.size());
    append_text_line(out, kRedPasswordFormat, settings.red_password.data(),
                     settings.red_password.size());
    append_text_line(out, kBluePasswordFormat, settings.blue_password.data(),
                     settings.blue_password.size());
    append_int_line(out, kMpVerboseFormat, settings.mp_verbose);
    return out;
}

void set_settings_text(char* field, std::size_t capacity, std::string_view value) noexcept {
    if (capacity == 0) {
        return;
    }
    std::size_t length = value.size();
    if (length > capacity - 1) {
        length = capacity - 1;
    }
    if (length != 0) {
        std::memcpy(field, value.data(), length);
    }
    field[length] = '\0';
}

std::string settings_text(const char* field, std::size_t capacity) {
    return bounded_text(field, capacity);
}

} // namespace dfre
