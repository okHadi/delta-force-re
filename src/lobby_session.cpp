#include "dfre/lobby_session.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace dfre {
namespace {

std::string button_index(std::string_view prefix, int index) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.*s%d", static_cast<int>(prefix.size()), prefix.data(),
                  index);
    return buffer;
}

std::string button_gt(int index) { return button_index("but_ho_gt", index); }
std::string button_mp_s(int index) { return button_index("but_mp_s", index); }

const char* game_type_key(int game_type) {
    switch (game_type) {
        case 0: return "MPCOOPERATIVE";
        case 1: return "MPDEATHMATCH";
        case 2: return "MPTEAMDEATHMATCH";
        case 3: return "MPKINGOFTHEHILL";
        case 4: return "MPTEAMKINGOFTHEHILL";
        case 5: return "MPCAPTUREFLAG";
        default: return "MPUNKNOWNTYPE";
    }
}

std::string version_text(std::uint32_t version) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "V%i.%02i.%02i.%02i",
                  static_cast<int>(version >> 24), static_cast<int>((version >> 16) & 0xff),
                  static_cast<int>((version >> 8) & 0xff), static_cast<int>(version & 0xff));
    return buffer;
}

// The three standard button states shared by every pressed/hovered control.
IntentList press_release(LobbyMenuAction action, const char* button) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {set_button_state(button, DrawState::Pushed)};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state(button, DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
        case LobbyMenuAction::MouseUp:
            return {set_button_state(button, DrawState::Normal)};
        default:
            return {};
    }
}

// Edit text buttons only track hover; commits and changes are separate actions.
IntentList text_hover(LobbyMenuAction action, const char* button) {
    switch (action) {
        case LobbyMenuAction::HoverEnter:
            return {set_button_state(button, DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state(button, DrawState::Normal)};
        default:
            return {};
    }
}

void append(IntentList& out, IntentList extra) {
    out.insert(out.end(), extra.begin(), extra.end());
}

} // namespace

MenuIntent set_button_state(std::string button, DrawState state) {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::SetButtonState;
    intent.button = std::move(button);
    intent.value = static_cast<int>(state);
    return intent;
}

MenuIntent begin_text_edit(std::string button, int max_length) {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::BeginTextEdit;
    intent.button = std::move(button);
    intent.value = max_length;
    return intent;
}

MenuIntent switch_menu(std::string menu) {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::SwitchMenu;
    intent.menu = std::move(menu);
    return intent;
}

MenuIntent reset_menu_input() {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::ResetMenuInput;
    return intent;
}

MenuIntent stop_session_search() {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::StopSessionSearch;
    return intent;
}

MenuIntent start_session_search() {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::StartSessionSearch;
    return intent;
}

MenuIntent join_session(int index) {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::JoinSession;
    intent.value = index;
    return intent;
}

MenuIntent refresh_host_menu() {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::RefreshHostMenu;
    return intent;
}

MenuIntent refresh_host2_menu() {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::RefreshHost2Menu;
    return intent;
}

MenuIntent update_host_map_list() {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::UpdateHostMapList;
    return intent;
}

MenuIntent session_object_call30() {
    MenuIntent intent;
    intent.kind = MenuIntent::Kind::SessionObjectCall30;
    return intent;
}

std::optional<SessionRowView> session_row_view(const SessionBrowser& browser, int row) {
    if (row < 0 || static_cast<std::size_t>(row) >= browser.entries.size()) {
        return std::nullopt;
    }
    const SessionEntry& entry = browser.entries[static_cast<std::size_t>(row)];
    SessionRowView view;
    view.name = entry.name;
    const int adjust = entry.flag != 0 ? -1 : 0;
    view.players = entry.players + adjust;
    view.max_players = entry.max_players + adjust;
    view.game_type_key = game_type_key(entry.game_type);
    view.version_text = version_text(entry.version);
    view.self = entry.version == browser.local_server_id;
    view.dedicated = entry.flag != 0;
    // Original formats: 0x4ca03c, 0x4ca024, 0x4ca00c, 0x4c9ff0.
    if (view.self && !view.dedicated) {
        view.format = "%s, %i %s %i, %s";
    } else if (view.self) {
        view.format = "%s, %i %s %i, %s, DS";
    } else if (!view.dedicated) {
        view.format = "%s, %i %s %i, %s, %s";
    } else {
        view.format = "%s, %i %s %i, %s, DS, %s";
    }
    return view;
}

std::string session_row_text(const SessionRowView& row, const TextResolver& resolve) {
    const std::string of = resolve(row.of_key);
    const std::string type = resolve(row.game_type_key);
    char buffer[512];
    std::snprintf(buffer, sizeof buffer, row.format, row.name.c_str(), row.players, of.c_str(),
                  row.max_players, type.c_str(), row.version_text.c_str());
    return buffer;
}

std::optional<std::string> session_status_draw(const MultiplayerSettings& settings,
                                               const SessionBrowser& browser, LobbyMenuAction action,
                                               const TextResolver& resolve) {
    if (settings.mp_service == 4 || action != LobbyMenuAction::SliderMoved) {
        return std::nullopt;
    }
    if (browser.status == 0) {
        return resolve("MPIDLE");
    }
    if (browser.status == 1) {
        // Original appends the separator "." from 0x4c9f5c, then the dot counter.
        return resolve("MPSEARCHING") + "." +
               std::string(static_cast<std::size_t>(std::max(browser.dots, 0)), '.');
    }
    return std::nullopt;
}

IntentList session_row_select(SessionBrowser& browser, int row) {
    IntentList out;
    if (row >= 0 && static_cast<std::size_t>(row) < browser.entries.size()) {
        out.push_back(set_button_state(button_mp_s(browser.selected + 1), DrawState::Normal));
        browser.selected = row;
    }
    out.push_back(set_button_state(button_mp_s(row + 1), DrawState::Pushed));
    return out;
}

IntentList host_menu_action(LobbyMenuAction action) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {set_button_state("but_mp_hotxt", DrawState::Pushed)};
        case LobbyMenuAction::MouseUp:
            return {set_button_state("but_mp_hotxt", DrawState::Normal), stop_session_search(),
                    session_object_call30(), switch_menu("Host Menu")};
        case LobbyMenuAction::Hotkey:
            return {stop_session_search(), session_object_call30(), switch_menu("Host Menu")};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_mp_hotxt", DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_mp_hotxt", DrawState::Normal)};
        default:
            return {};
    }
}

IntentList join_menu_action(const MultiplayerSettings& settings, const SessionBrowser& browser,
                            LobbyMenuAction action, PreConnect& pre_connect) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {set_button_state("but_mp_jo", DrawState::Pushed)};
        case LobbyMenuAction::MouseUp:
            pre_connect.mode = 1;
            pre_connect.service = settings.mp_service;
            pre_connect.hosting = 0;
            pre_connect.flag = 1;
            return {set_button_state("but_mp_jo", DrawState::Normal),
                    join_session(browser.selected), stop_session_search(),
                    switch_menu("Pre Menu")};
        case LobbyMenuAction::Hotkey:
            pre_connect.mode = 1;
            pre_connect.service = settings.mp_service;
            pre_connect.hosting = 0;
            pre_connect.flag = 1;
            return {stop_session_search(), switch_menu("Pre Menu")};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_mp_jo", DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_mp_jo", DrawState::Normal)};
        default:
            return {};
    }
}

IntentList search_menu_action(const SessionBrowser& browser, LobbyMenuAction action) {
    switch (action) {
        case LobbyMenuAction::MouseDown: {
            IntentList out = {set_button_state("but_mp_se", DrawState::Pushed)};
            if (browser.status == 0) {
                out.push_back(start_session_search());
            } else if (browser.status == 1) {
                out.push_back(stop_session_search());
            }
            return out;
        }
        case LobbyMenuAction::MouseUp:
            return {set_button_state("but_mp_se", DrawState::Normal)};
        case LobbyMenuAction::Hotkey: {
            IntentList out = search_menu_action(browser, LobbyMenuAction::MouseDown);
            append(out, search_menu_action(browser, LobbyMenuAction::MouseUp));
            return out;
        }
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_mp_se", DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_mp_se", DrawState::Normal)};
        default:
            return {};
    }
}

IntentList host_game_name(LobbyMenuAction action) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {set_button_state("but_ho_gn", DrawState::Pushed)};
        case LobbyMenuAction::MouseUp:
            return {begin_text_edit("but_ho_gnw", 32)};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_ho_gn", DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_ho_gn", DrawState::Normal)};
        default:
            return {};
    }
}

IntentList host_game_name_text(MultiplayerSettings& settings, LobbyMenuAction action,
                               std::string_view text) {
    if (action == LobbyMenuAction::TextCommitted) {
        set_settings_text(settings.game_name.data(), settings.game_name.size(), text);
        return {set_button_state("but_ho_gn", DrawState::Normal)};
    }
    if (action == LobbyMenuAction::TextChanged) {
        return {set_button_state("but_ho_gn", DrawState::Normal)};
    }
    return text_hover(action, "but_ho_gnw");
}

IntentList host_session_password(LobbyMenuAction action) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {set_button_state("but_ho_sp", DrawState::Pushed)};
        case LobbyMenuAction::MouseUp:
            return {begin_text_edit("but_ho_spw", 17)};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_ho_sp", DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_ho_sp", DrawState::Normal)};
        default:
            return {};
    }
}

IntentList host_session_password_text(MultiplayerSettings& settings, LobbyMenuAction action,
                                      std::string_view text) {
    if (action == LobbyMenuAction::TextCommitted) {
        set_settings_text(settings.game_password.data(), settings.game_password.size(), text);
        return {set_button_state("but_ho_sp", DrawState::Normal)};
    }
    if (action == LobbyMenuAction::TextChanged) {
        return {set_button_state("but_ho_sp", DrawState::Normal)};
    }
    return text_hover(action, "but_ho_spw");
}

IntentList host_country(LobbyMenuAction action) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {set_button_state("but_ho_cty", DrawState::Pushed)};
        case LobbyMenuAction::MouseUp:
            return {begin_text_edit("but_ho_ctyw", 4)};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_ho_cty", DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_ho_cty", DrawState::Normal)};
        default:
            return {};
    }
}

IntentList host_country_text(MultiplayerSettings& settings, LobbyMenuAction action,
                             std::string_view text) {
    if (action == LobbyMenuAction::TextCommitted) {
        set_settings_text(settings.country.data(), settings.country.size(), text);
        return {set_button_state("but_ho_cty", DrawState::Normal)};
    }
    if (action == LobbyMenuAction::TextChanged) {
        return {set_button_state("but_ho_cty", DrawState::Normal)};
    }
    return text_hover(action, "but_ho_ctyw");
}

IntentList host_server_type(MultiplayerSettings& settings, LobbyMenuAction action,
                           std::string clicked_button) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {set_button_state(std::move(clicked_button), DrawState::Pushed)};
        case LobbyMenuAction::MouseUp:
            settings.dedicated ^= 1;
            return {set_button_state(std::move(clicked_button), DrawState::Normal),
                    refresh_host_menu()};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state(std::move(clicked_button), DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state(std::move(clicked_button), DrawState::Normal)};
        default:
            return {};
    }
}

IntentList host_game_type(HostState& host, MultiplayerSettings& settings, LobbyMenuAction action,
                          int user_id) {
    if (action != LobbyMenuAction::MouseDown) {
        return {};
    }
    IntentList out;
    out.push_back(set_button_state(button_gt(settings.mp_gametype + 1), DrawState::Normal));
    settings.mp_gametype = user_id;
    const bool enabled = user_id != 0;
    const DrawState list_state = enabled ? DrawState::Normal : DrawState::Null;
    for (const char* button : {"but_ho_seltxt", "but_ho_select", "but_ho_selup", "but_ho_seldn",
                               "but_ho_selslw"}) {
        out.push_back(set_button_state(button, list_state));
    }
    out.push_back(set_button_state("but_ho_selsl", list_state));
    host.map_offset = 0;
    host.slider_position = 0;
    if (settings.mp_gametype == 0) {
        if (settings.maxplayers > 8) {
            settings.maxplayers = 8;
        }
        host.slider_max = 0;
    } else if (settings.mp_gametype >= 1 && settings.mp_gametype <= 5) {
        host.slider_max = host.map_list_counts[static_cast<std::size_t>(settings.mp_gametype - 1)] - 1;
    }
    out.push_back(update_host_map_list());
    out.push_back(refresh_host_menu());
    out.push_back(set_button_state(button_gt(user_id + 1), DrawState::Pushed));
    return out;
}

IntentList host_max_players(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button) {
    if (action == LobbyMenuAction::MouseDown) {
        if (minus_button) {
            if (settings.maxplayers > 2) {
                return {set_button_state("but_ho_mpl", DrawState::Pushed)};
            }
        } else if (settings.maxplayers < 0x20) {
            return {set_button_state("but_ho_mpr", DrawState::Pushed)};
        }
        return {};
    }
    if (action == LobbyMenuAction::MouseUp) {
        settings.maxplayers += minus_button ? -1 : 1;
        if (settings.maxplayers < 2) {
            settings.maxplayers = 2;
        } else if (settings.maxplayers > 0x20) {
            settings.maxplayers = 0x20;
        }
        return {refresh_host_menu()};
    }
    return press_release(action, minus_button ? "but_ho_mpl" : "but_ho_mpr");
}

IntentList host_next(LobbyMenuAction action) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {reset_menu_input(), set_button_state("but_ho_next", DrawState::Pushed)};
        case LobbyMenuAction::MouseUp:
            return {set_button_state("but_ho_next", DrawState::Normal),
                    switch_menu("Host2 Menu")};
        case LobbyMenuAction::Hotkey:
            return {switch_menu("Host2 Menu")};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_ho_next", DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_ho_next", DrawState::Normal)};
        default:
            return {};
    }
}

IntentList host_accept(const HostState& host, const MultiplayerSettings& settings,
                       LobbyMenuAction action, PreConnect& pre_connect) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {reset_menu_input(), set_button_state("but_ho_actxt", DrawState::Pushed)};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_ho_actxt", DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_ho_actxt", DrawState::Normal)};
        case LobbyMenuAction::MouseUp:
            break;
        default:
            return {};
    }
    IntentList out = {set_button_state("but_ho_actxt", DrawState::Normal)};
    pre_connect.mode = 1;
    pre_connect.service = settings.mp_service;
    pre_connect.hosting = 1;
    pre_connect.flag = settings.dedicated == 0 ? 1 : 0;
    pre_connect.max_players = settings.maxplayers;
    pre_connect.game_type = settings.mp_gametype;
    if (settings.mp_gametype == 0) {
        out.push_back(switch_menu("Campaign Menu"));
        return out;
    }
    if (settings.mp_gametype >= 1 && settings.mp_gametype <= 5) {
        const std::size_t type = static_cast<std::size_t>(settings.mp_gametype - 1);
        const int index = host.map_list_bases[type] + host.map_offset;
        const auto& names = host.map_names[type];
        if (index >= 0 && static_cast<std::size_t>(index) < names.size()) {
            pre_connect.map_name = names[static_cast<std::size_t>(index)];
        } else {
            pre_connect.map_name.clear();
        }
        out.push_back(switch_menu("Pre Menu"));
        return out;
    }
    return out;
}

IntentList host2_accept(LobbyMenuAction action) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {reset_menu_input(), set_button_state("but_ho2_actxt", DrawState::Pushed)};
        case LobbyMenuAction::MouseUp:
            return {set_button_state("but_ho2_actxt", DrawState::Normal),
                    switch_menu("Host Menu")};
        case LobbyMenuAction::Hotkey:
            return {switch_menu("Host Menu")};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_ho2_actxt", DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_ho2_actxt", DrawState::Normal)};
        default:
            return {};
    }
}

IntentList host2_next(LobbyMenuAction action) {
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {set_button_state("but_ho2_wtxt", DrawState::Pushed), switch_menu("ho3 Menu")};
        case LobbyMenuAction::MouseUp:
        case LobbyMenuAction::HoverLeave:
            return {set_button_state("but_ho2_wtxt", DrawState::Normal)};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state("but_ho2_wtxt", DrawState::Lit)};
        default:
            return {};
    }
}

IntentList host2_infinite_replay(MultiplayerSettings& settings, LobbyMenuAction action,
                                 bool minus_button) {
    const char* button = minus_button ? "but_ho2_irl" : "but_ho2_irr";
    if (action == LobbyMenuAction::MouseDown) {
        return {set_button_state(button, DrawState::Pushed)};
    }
    if (action == LobbyMenuAction::MouseUp) {
        settings.replay += minus_button ? -1 : 1;
        if (settings.replay < 0) {
            settings.replay = 2;
        } else if (settings.replay > 2) {
            settings.replay = 0;
        }
        return {set_button_state(button, DrawState::Normal), refresh_host2_menu()};
    }
    return press_release(action, button);
}

IntentList host2_max_players(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button) {
    const char* button = minus_button ? "but_ho2_mpl" : "but_ho2_mpr";
    if (action == LobbyMenuAction::MouseDown) {
        return {set_button_state(button, DrawState::Pushed)};
    }
    if (action == LobbyMenuAction::MouseUp) {
        const int high = settings.mp_gametype == 0 ? 8 : 0x10;
        settings.maxplayers += minus_button ? -1 : 1;
        if (settings.maxplayers < 2) {
            settings.maxplayers = high;
        } else if (settings.maxplayers > high) {
            settings.maxplayers = 2;
        }
        return {set_button_state(button, DrawState::Normal), refresh_host2_menu()};
    }
    return press_release(action, button);
}

namespace {

IntentList slider_control(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button,
                          std::int32_t MultiplayerSettings::*field, int low, int high,
                          int* slider, const char* minus_name, const char* plus_name,
                          int clamp_position) {
    if (action == LobbyMenuAction::MouseDown) {
        return {set_button_state(minus_button ? minus_name : plus_name, DrawState::Pushed)};
    }
    if (action == LobbyMenuAction::MouseUp) {
        IntentList out = {set_button_state(minus_button ? minus_name : plus_name, DrawState::Normal)};
        settings.*field += minus_button ? -1 : 1;
        settings.*field = std::clamp(settings.*field, low, high);
        if (slider != nullptr) {
            *slider = settings.*field;
        }
        out.push_back(refresh_host2_menu());
        return out;
    }
    if (action == LobbyMenuAction::SliderMoved) {
        settings.*field = slider != nullptr ? *slider : settings.*field;
        if (settings.*field < low) {
            settings.*field = low;
            if (slider != nullptr) {
                *slider = clamp_position;
            }
        }
        return {refresh_host2_menu()};
    }
    return press_release(action, minus_button ? minus_name : plus_name);
}

} // namespace

IntentList host2_score_limit(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button,
                             int& slider) {
    return slider_control(settings, action, minus_button, &MultiplayerSettings::max_kills, 0, 100,
                          &slider, "but_ho2_sll", "but_ho2_slr", 1);
}

IntentList host2_time_limit(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button,
                            int& slider) {
    return slider_control(settings, action, minus_button, &MultiplayerSettings::time_limit, 0, 100,
                          &slider, "but_ho2_tll", "but_ho2_tlr", 1);
}

IntentList host2_koth_limit(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button,
                            int& slider) {
    return slider_control(settings, action, minus_button, &MultiplayerSettings::koth_limit, 1, 100,
                          &slider, "but_ho2_kohl", "but_ho2_kohr", 1);
}

IntentList host2_timeout(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button,
                         int& slider) {
    return slider_control(settings, action, minus_button, &MultiplayerSettings::timeout, 0, 100,
                          &slider, "but_ho2_tol", "but_ho2_tor", 0);
}

IntentList host2_ammo_respawn(MultiplayerSettings& settings, LobbyMenuAction action, bool minus_button) {
    const char* button = minus_button ? "but_ho2_arl" : "but_ho2_arr";
    if (action == LobbyMenuAction::MouseDown) {
        return {set_button_state(button, DrawState::Pushed)};
    }
    if (action == LobbyMenuAction::MouseUp) {
        settings.ammo_respawn += minus_button ? -1 : 1;
        if (settings.ammo_respawn < 0) {
            settings.ammo_respawn = 3;
        } else if (settings.ammo_respawn > 3) {
            settings.ammo_respawn = 0;
        }
        return {set_button_state(button, DrawState::Normal), refresh_host2_menu()};
    }
    return press_release(action, button);
}

IntentList host2_breakx(MultiplayerSettings& settings, LobbyMenuAction action) {
    if (action == LobbyMenuAction::MouseDown) {
        return {set_button_state("but_ho2_bxw", DrawState::Pushed)};
    }
    if (action == LobbyMenuAction::MouseUp) {
        settings.attributes ^= 1;
        return {set_button_state("but_ho2_bxw", DrawState::Normal), refresh_host2_menu()};
    }
    return press_release(action, "but_ho2_bxw");
}

IntentList host2_team_selection(MultiplayerSettings& settings, LobbyMenuAction action) {
    if (action == LobbyMenuAction::MouseDown) {
        return {set_button_state("but_ho2_tsw", DrawState::Pushed)};
    }
    if (action == LobbyMenuAction::MouseUp) {
        settings.attributes ^= 0x40;
        return {set_button_state("but_ho2_tsw", DrawState::Normal), refresh_host2_menu()};
    }
    return press_release(action, "but_ho2_tsw");
}

IntentList host2_da_tracers(MultiplayerSettings& settings, LobbyMenuAction action) {
    if (action == LobbyMenuAction::MouseDown) {
        return {set_button_state("but_ho2_trw", DrawState::Pushed)};
    }
    if (action == LobbyMenuAction::MouseUp) {
        settings.attributes ^= 2;
        return {set_button_state("but_ho2_trw", DrawState::Normal), refresh_host2_menu()};
    }
    return press_release(action, "but_ho2_trw");
}

IntentList host2_password(LobbyMenuAction action, int user_id) {
    const char* self = user_id == 0 ? "but_ho2_rtd" : "but_ho2_btd";
    const char* edit = user_id == 0 ? "but_ho2_rtw" : "but_ho2_btw";
    switch (action) {
        case LobbyMenuAction::MouseDown:
            return {set_button_state(self, DrawState::Pushed)};
        case LobbyMenuAction::MouseUp:
            return {begin_text_edit(edit, 10)};
        case LobbyMenuAction::HoverEnter:
            return {set_button_state(self, DrawState::Lit)};
        case LobbyMenuAction::HoverLeave:
            return {set_button_state(self, DrawState::Normal)};
        default:
            return {};
    }
}

IntentList host2_password_text(MultiplayerSettings& settings, LobbyMenuAction action, int user_id,
                               std::string_view text) {
    const char* edit = user_id == 0 ? "but_ho2_rtw" : "but_ho2_btw";
    IntentList out;
    if (action == LobbyMenuAction::MouseDown) {
        out.push_back(set_button_state(edit, DrawState::Pushed));
    } else if (action == LobbyMenuAction::MouseUp) {
        out.push_back(begin_text_edit(edit, 10));
    } else if (action == LobbyMenuAction::HoverEnter) {
        out.push_back(set_button_state(edit, DrawState::Lit));
    } else if (action == LobbyMenuAction::HoverLeave) {
        out.push_back(set_button_state(edit, DrawState::Normal));
    }
    if (action == LobbyMenuAction::TextCommitted) {
        if (user_id == 0) {
            set_settings_text(settings.red_password.data(), settings.red_password.size(), text);
        } else {
            set_settings_text(settings.blue_password.data(), settings.blue_password.size(), text);
        }
        out.push_back(set_button_state(user_id == 0 ? "but_ho2_rt" : "but_ho2_bt",
                                       DrawState::Normal));
    } else if (action == LobbyMenuAction::TextChanged) {
        // Original refreshes the blue row with "but_mp_bt" (0x41c050); kept as-is.
        out.push_back(set_button_state(user_id == 0 ? "but_ho2_rt" : "but_mp_bt",
                                       DrawState::Normal));
    }
    return out;
}

} // namespace dfre
