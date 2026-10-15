#pragma once

#include "dfre/lobby.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dfre {

// Lobby session and host-setting callback state.
//
// Original callbacks (Df.exe image base 0x400000):
//   mnu_mp_host 0x416f10, mnu_mp_join 0x416f80, mnu_mp_search 0x4173b0,
//   mnu_mp_status 0x416500, mnu_mp_sessions 0x416670,
//   mnu_host_* 0x418110..0x418ca0, mnu_host2_* 0x41b490..0x41c050.
// This module returns record data and menu intents only; it performs no UI or
// network work. External calls are explicit intents for the caller to wire.

// Button action ids dispatched by the original button input at 0x40def0.
enum class LobbyMenuAction : int {
    MouseDown = 1,
    MouseUp = 2,
    SliderMoved = 3,
    TextCommitted = 4,
    TextChanged = 5,
    HoverEnter = 6,
    HoverLeave = 7,
    Hotkey = 8,
};

// Draw states written by the original setter 0x40e2a0(button, state).
enum class DrawState : int { Null = 0, Normal = 1, Lit = 2, Pushed = 3, Disabled = 4 };

// Text keys are resolved by the game text system (0x49a092), which is outside
// this module; the caller supplies the resolver.
using TextResolver = std::function<std::string(std::string_view key)>;

struct MenuIntent {
    enum class Kind : int {
        SetButtonState,      // 0x40e2a0(button, state)
        BeginTextEdit,       // 0x40e340(button, max_length)
        SwitchMenu,          // 0x461340(menu name)
        ResetMenuInput,      // 0x40b9a0()
        StopSessionSearch,   // 0x417370() resets status to idle and stops polling
        StartSessionSearch,  // 0x4172f0() starts the service-specific search
        JoinSession,         // 0x41e2d0(selected) stores the join target index
        RefreshHostMenu,     // 0x417b60() rewrites the host page fields
        RefreshHost2Menu,    // 0x41ac00() rewrites the host options page fields
        UpdateHostMapList,   // 0x417530() rewrites the map list controls
        SessionObjectCall30, // 0x497580() session object vtable +0x30
    };
    Kind kind = Kind::SetButtonState;
    std::string button;  // for SetButtonState and BeginTextEdit
    int value = 0;       // draw state, edit length, or session index
    std::string menu;    // for SwitchMenu
};

using IntentList = std::vector<MenuIntent>;

MenuIntent set_button_state(std::string button, DrawState state);
MenuIntent begin_text_edit(std::string button, int max_length);
MenuIntent switch_menu(std::string menu);
MenuIntent reset_menu_input();
MenuIntent stop_session_search();
MenuIntent start_session_search();
MenuIntent join_session(int index);
MenuIntent refresh_host_menu();
MenuIntent refresh_host2_menu();
MenuIntent update_host_map_list();
MenuIntent session_object_call30();

// Session browser model.
// Original: linked list head 0x501328, count 0x501330, selected 0x4d6b68,
// status 0x4d6b58, search dots 0x4d6b64, local server id 0x50362c.
struct SessionEntry {
    std::string name;            // node +0x0c
    int players = 0;             // node +0x34
    int max_players = 0;         // node +0x38
    int game_type = 0;           // node +0x3c, 0..5
    int flag = 0;                // node +0x40
    std::uint32_t version = 0;   // node +0x44 packed as %i.%02i.%02i.%02i
};

struct SessionBrowser {
    std::vector<SessionEntry> entries;
    int selected = 0;                   // 0x4d6b68
    int status = 0;                     // 0x4d6b58; 0 idle, 1 searching
    int dots = 0;                       // 0x4d6b64
    std::uint32_t local_server_id = 0;  // 0x50362c
};

// Session row data for mnu_mp_sessions action 3 (draw).
struct SessionRowView {
    std::string name;
    int players = 0;               // node players with the flag adjustment applied
    int max_players = 0;           // node max with the flag adjustment applied
    std::string game_type_key;     // MPCOOPERATIVE..MPCAPTUREFLAG or MPUNKNOWNTYPE
    std::string version_text;      // rendered "V%i.%02i.%02i.%02i"
    bool self = false;             // node version == local server id
    bool dedicated = false;        // node flag != 0
    std::string of_key = "OF";     // original text key between the counts
    const char* format = nullptr;  // exact original row format string
};

std::optional<SessionRowView> session_row_view(const SessionBrowser& browser, int row);
std::string session_row_text(const SessionRowView& row, const TextResolver& resolve);

// mnu_mp_status action 3 (draw): only drawn when mp_service != 4. State 0 shows
// MPIDLE; state 1 shows MPSEARCHING followed by the dot counter.
std::optional<std::string> session_status_draw(const MultiplayerSettings& settings,
                                               const SessionBrowser& browser, LobbyMenuAction action,
                                               const TextResolver& resolve);

// mnu_mp_sessions action 1 (mouse down): resets the previous row and selects.
IntentList session_row_select(SessionBrowser& browser, int row);

// mnu_mp_host. Returns the intents for the action.
IntentList host_menu_action(LobbyMenuAction action);

// mnu_mp_join. Sets the pre-connect block from the selected row.
struct PreConnect {
    int mode = 0;         // 0x67f520
    int service = 0;      // 0x67f524 (mp_service)
    int hosting = 0;      // 0x67f528
    int flag = 0;         // 0x67f52c
    int max_players = 0;  // 0x67f530
    int game_type = 0;    // 0x67f53c
    std::string map_name;  // 0x67f438 copied from the selected host map list entry
};

IntentList join_menu_action(const MultiplayerSettings& settings, const SessionBrowser& browser,
                            LobbyMenuAction action, PreConnect& pre_connect);

// mnu_mp_search.
IntentList search_menu_action(const SessionBrowser& browser, LobbyMenuAction action);

// Host page map list state (original 0x4e65ac..0x4e65d4 and the map-name list).
struct HostState {
    std::array<int, 5> map_list_counts{};  // 0x4e65ac, b0, b4, b8, bc (game types 1..5)
    std::array<int, 5> map_list_bases{};   // 0x4e65c0, c4, c8, cc, d0
    int map_offset = 0;                    // 0x4e65d4
    int slider_max = 0;                    // selected map slider +0x64
    int slider_position = 0;               // selected map slider +0x68
    std::array<int, 5> selected_map{};     // 0x4e65d8, dc, e0, e4, e8
    // One map-name table per game type 1..5 at 0x4d6ba8, 0x4d9da8, 0x4dcfa8,
    // 0x4e01a8, 0x4e33a8; each entry is one 0x80-byte name.
    std::array<std::vector<std::string>, 5> map_names;
};

// Host page callbacks. "text" carries the current text of the paired edit
// button for TextCommitted and TextChanged actions.
IntentList host_game_name(LobbyMenuAction action);
IntentList host_game_name_text(MultiplayerSettings& settings, LobbyMenuAction action,
                               std::string_view text);
IntentList host_session_password(LobbyMenuAction action);
IntentList host_session_password_text(MultiplayerSettings& settings, LobbyMenuAction action,
                                      std::string_view text);
IntentList host_country(LobbyMenuAction action);
IntentList host_country_text(MultiplayerSettings& settings, LobbyMenuAction action,
                             std::string_view text);
IntentList host_server_type(MultiplayerSettings& settings, LobbyMenuAction action,
                           std::string clicked_button);
IntentList host_game_type(HostState& host, MultiplayerSettings& settings, LobbyMenuAction action,
                          int user_id);
IntentList host_max_players(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button);
IntentList host_next(LobbyMenuAction action);
IntentList host_accept(const HostState& host, const MultiplayerSettings& settings,
                       LobbyMenuAction action, PreConnect& pre_connect);

// Host options (second) page callbacks.
IntentList host2_accept(LobbyMenuAction action);
IntentList host2_next(LobbyMenuAction action);
IntentList host2_infinite_replay(MultiplayerSettings& settings, LobbyMenuAction action,
                                 bool minus_button);
IntentList host2_max_players(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button);
IntentList host2_score_limit(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button,
                             int& slider);
IntentList host2_time_limit(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button,
                            int& slider);
IntentList host2_koth_limit(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button,
                            int& slider);
IntentList host2_ammo_respawn(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button);
IntentList host2_timeout(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button,
                         int& slider);
IntentList host2_breakx(MultiplayerSettings& settings, LobbyMenuAction action);
IntentList host2_team_selection(MultiplayerSettings& settings, LobbyMenuAction action);
IntentList host2_da_tracers(MultiplayerSettings& settings, LobbyMenuAction action);
IntentList host2_password(LobbyMenuAction action, int user_id);
IntentList host2_password_text(MultiplayerSettings& settings, LobbyMenuAction action, int user_id,
                               std::string_view text);

} // namespace dfre
