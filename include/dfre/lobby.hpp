#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace dfre {

// Original multiplayer part of the game settings record.
//
// Df.exe image base 0x400000; the record lives at 0x4ced20 with size 0x5c4.
// It is zeroed and filled by the settings initializer at 0x407120 and read by
// the df.cfg writer at 0x407360. Offsets below are record-relative so the
// exact original addresses stay traceable: field offset + 0x4ced20.
inline constexpr std::size_t kMultiplayerSettingsSize = 0x5c4;

struct MultiplayerSettings {
    // 0x000..0x074: music, display, and control fields. This module does not
    // model them; raw bytes keep every multiplayer offset exact.
    std::array<std::uint8_t, 0x74> head{};

    std::array<char, 32> login_password{};    // 0x074 -> 0x4ced94 login_password
    std::array<char, 32> game_password{};     // 0x094 -> 0x4cedb4 game_password
    std::array<char, 16> squad_password{};    // 0x0b4 -> 0x4cedd4 squad_password
    std::array<char, 16> red_password{};      // 0x0c4 -> 0x4cede4 red_password
    std::array<char, 16> blue_password{};     // 0x0d4 -> 0x4cedf4 blue_password
    std::array<char, 32> game_name{};         // 0x0e4 -> 0x4cee04 game_name
    std::array<char, 4> country{};            // 0x104 -> 0x4cee24 country
    std::array<char, 32> phone_number{};      // 0x108 -> 0x4cee28 phone_number
    std::array<char, 32> internet_address{};  // 0x128 -> 0x4cee48 internet_address

    std::int32_t mp_service = 0;     // 0x148 -> 0x4cee68 default 4
    std::int32_t mp_gametype = 0;    // 0x14c -> 0x4cee6c default 0
    std::int32_t maxplayers = 0;     // 0x150 -> 0x4cee70 default 16
    std::int32_t dedicated = 0;      // 0x154 -> 0x4cee74 default 0
    std::int32_t com_port = 0;       // 0x158 -> 0x4cee78 default 0
    std::int32_t baud_rate = 0;      // 0x15c -> 0x4cee7c default 0
    std::int32_t max_lives = 0;      // 0x160 -> 0x4cee80 default 20
    std::int32_t max_kills = 0;      // 0x164 -> 0x4cee84 default 20
    std::int32_t team_req = 0;       // 0x168 -> 0x4cee88 default 1
    std::int32_t attributes = 0;     // 0x16c -> 0x4cee8c default 2
    std::int32_t unset_170 = 0;      // 0x170 no df.cfg key; default 0
    std::int32_t replay = 0;         // 0x174 -> 0x4cee94 default 0
    std::int32_t time_limit = 0;     // 0x178 -> 0x4cee98 default 30
    std::int32_t koth_limit = 0;     // 0x17c -> 0x4cee9c default 30
    std::int32_t ammo_respawn = 0;   // 0x180 -> 0x4ceea0 default 2
    std::int32_t timeout = 0;        // 0x184 -> 0x4ceea4 default 0
    std::int32_t gpsicon = 0;        // 0x188 -> 0x4ceea8 default 1
    std::int32_t showtags = 0;       // 0x18c -> 0x4ceeac default 0
    std::int32_t contest = 0;        // 0x190 -> 0x4ceeb0 default 0

    std::array<std::uint8_t, 0x5b4 - 0x194> untouched{};  // 0x194..0x5b4

    std::int32_t flag_5b4 = 0;      // 0x5b4 -> 0x4cf2d4 default 1; no df.cfg key
    std::int32_t whichbrowser = 0;  // 0x5b8 -> 0x4cf2d8 default 0
    std::int32_t turbo = 0;         // 0x5bc -> 0x4cf2dc default 0
    std::int32_t mp_verbose = 0;    // 0x5c0 -> 0x4cf2e0 default 1
};

static_assert(sizeof(MultiplayerSettings) == kMultiplayerSettingsSize,
              "MultiplayerSettings must keep the original 0x5c4 byte record size.");
static_assert(offsetof(MultiplayerSettings, login_password) == 0x074, "login_password offset");
static_assert(offsetof(MultiplayerSettings, game_password) == 0x094, "game_password offset");
static_assert(offsetof(MultiplayerSettings, squad_password) == 0x0b4, "squad_password offset");
static_assert(offsetof(MultiplayerSettings, red_password) == 0x0c4, "red_password offset");
static_assert(offsetof(MultiplayerSettings, blue_password) == 0x0d4, "blue_password offset");
static_assert(offsetof(MultiplayerSettings, game_name) == 0x0e4, "game_name offset");
static_assert(offsetof(MultiplayerSettings, country) == 0x104, "country offset");
static_assert(offsetof(MultiplayerSettings, phone_number) == 0x108, "phone_number offset");
static_assert(offsetof(MultiplayerSettings, internet_address) == 0x128, "internet_address offset");
static_assert(offsetof(MultiplayerSettings, mp_service) == 0x148, "mp_service offset");
static_assert(offsetof(MultiplayerSettings, mp_gametype) == 0x14c, "mp_gametype offset");
static_assert(offsetof(MultiplayerSettings, maxplayers) == 0x150, "maxplayers offset");
static_assert(offsetof(MultiplayerSettings, dedicated) == 0x154, "dedicated offset");
static_assert(offsetof(MultiplayerSettings, com_port) == 0x158, "com_port offset");
static_assert(offsetof(MultiplayerSettings, baud_rate) == 0x15c, "baud_rate offset");
static_assert(offsetof(MultiplayerSettings, max_lives) == 0x160, "max_lives offset");
static_assert(offsetof(MultiplayerSettings, max_kills) == 0x164, "max_kills offset");
static_assert(offsetof(MultiplayerSettings, team_req) == 0x168, "team_req offset");
static_assert(offsetof(MultiplayerSettings, attributes) == 0x16c, "attributes offset");
static_assert(offsetof(MultiplayerSettings, replay) == 0x174, "replay offset");
static_assert(offsetof(MultiplayerSettings, time_limit) == 0x178, "time_limit offset");
static_assert(offsetof(MultiplayerSettings, koth_limit) == 0x17c, "koth_limit offset");
static_assert(offsetof(MultiplayerSettings, ammo_respawn) == 0x180, "ammo_respawn offset");
static_assert(offsetof(MultiplayerSettings, timeout) == 0x184, "timeout offset");
static_assert(offsetof(MultiplayerSettings, gpsicon) == 0x188, "gpsicon offset");
static_assert(offsetof(MultiplayerSettings, showtags) == 0x18c, "showtags offset");
static_assert(offsetof(MultiplayerSettings, contest) == 0x190, "contest offset");
static_assert(offsetof(MultiplayerSettings, flag_5b4) == 0x5b4, "flag_5b4 offset");
static_assert(offsetof(MultiplayerSettings, whichbrowser) == 0x5b8, "whichbrowser offset");
static_assert(offsetof(MultiplayerSettings, turbo) == 0x5bc, "turbo offset");
static_assert(offsetof(MultiplayerSettings, mp_verbose) == 0x5c0, "mp_verbose offset");

// Values the original initializer at 0x407120 leaves in the record for the
// fields above. Strings are fixed text fields; set them with set_settings_text.
MultiplayerSettings multiplayer_defaults() noexcept;

// Lines the original df.cfg writer at 0x407360 emits for the multiplayer
// fields: 24 lines after "// MULTIPLAYER" and 4 lines in "// SYSTEM".
std::string multiplayer_config_lines(const MultiplayerSettings& settings);

// Copy at most capacity-1 characters into a fixed text field and terminate it.
void set_settings_text(char* field, std::size_t capacity, std::string_view value) noexcept;

// Read a fixed text field, stopping at the first NUL byte or at capacity.
std::string settings_text(const char* field, std::size_t capacity);

} // namespace dfre
