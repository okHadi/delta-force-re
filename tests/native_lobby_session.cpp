#include "dfre/lobby_session.hpp"

#include <iostream>
#include <string>

namespace {
bool require(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

using dfre::DrawState;
using dfre::IntentList;
using dfre::LobbyMenuAction;
using dfre::MenuIntent;
using dfre::MultiplayerSettings;
using dfre::SessionBrowser;
using dfre::SessionEntry;
using dfre::TextResolver;

std::string plain(std::string_view key) { return "<" + std::string(key) + ">"; }

const MenuIntent* find(const IntentList& intents, MenuIntent::Kind kind) {
    for (const auto& intent : intents) {
        if (intent.kind == kind) return &intent;
    }
    return nullptr;
}

int count(const IntentList& intents, MenuIntent::Kind kind) {
    int total = 0;
    for (const auto& intent : intents) {
        if (intent.kind == kind) ++total;
    }
    return total;
}
} // namespace

int main() {
    // Intent constructors.
    if (!require(dfre::set_button_state("but_x", DrawState::Pushed).value == 3,
                 "Pushed state must be 3.")) return 1;
    if (!require(dfre::begin_text_edit("but_x", 17).value == 17, "Edit length must carry.")) return 1;
    if (!require(dfre::switch_menu("Host Menu").menu == "Host Menu", "Menu name must carry.")) return 1;

    // Session rows: version text, adjustments, key, and the four formats.
    SessionBrowser browser;
    browser.local_server_id = 0x01050302;
    SessionEntry entry;
    entry.name = "RowA";
    entry.players = 5;
    entry.max_players = 10;
    entry.game_type = 1;
    entry.flag = 1;
    entry.version = 0x01050302;
    browser.entries.push_back(entry);
    auto view = dfre::session_row_view(browser, 0);
    if (!require(view && view->players == 4 && view->max_players == 9 && view->self &&
                 view->dedicated, "Flag adjustment and self must apply.")) return 1;
    if (!require(view->version_text == "V1.05.03.02", "Version text must zero-pad.")) return 1;
    if (!require(view->game_type_key == "MPDEATHMATCH", "Game type key must map.")) return 1;
    if (!require(std::string(view->format) == "%s, %i %s %i, %s, DS",
                 "Self dedicated format must match 0x4ca024.")) return 1;
    if (!require(dfre::session_row_text(*view, plain) == "RowA, 4 <OF> 9, <MPDEATHMATCH>, DS",
                 "Self dedicated row text must render.")) return 1;

    browser.entries[0].flag = 0;
    view = dfre::session_row_view(browser, 0);
    if (!require(view && view->players == 5 && !view->dedicated &&
                 std::string(view->format) == "%s, %i %s %i, %s",
                 "Self open format must match 0x4ca03c.")) return 1;
    browser.entries[0].version = 0x02010000;
    browser.entries[0].flag = 1;
    view = dfre::session_row_view(browser, 0);
    if (!require(view && !view->self && view->version_text == "V2.01.00.00" &&
                 std::string(view->format) == "%s, %i %s %i, %s, DS, %s",
                 "Remote dedicated format must match 0x4c9ff0.")) return 1;
    if (!require(dfre::session_row_text(*view, plain) ==
                 "RowA, 4 <OF> 9, <MPDEATHMATCH>, DS, V2.01.00.00",
                 "Remote dedicated row text must render.")) return 1;
    browser.entries[0].flag = 0;
    view = dfre::session_row_view(browser, 0);
    if (!require(view && std::string(view->format) == "%s, %i %s %i, %s, %s",
                 "Remote open format must match 0x4ca00c.")) return 1;
    browser.entries[0].game_type = 9;
    view = dfre::session_row_view(browser, 0);
    if (!require(view && view->game_type_key == "MPUNKNOWNTYPE", "Unknown type key.")) return 1;
    if (!require(!dfre::session_row_view(browser, 1), "Out-of-range row must be empty.")) return 1;

    // Status draw rules.
    MultiplayerSettings settings = dfre::multiplayer_defaults();
    settings.mp_service = 4;
    browser.status = 0;
    if (!require(!dfre::session_status_draw(settings, browser, LobbyMenuAction::SliderMoved, plain),
                 "mp_service 4 must not draw status.")) return 1;
    settings.mp_service = 2;
    auto text = dfre::session_status_draw(settings, browser, LobbyMenuAction::SliderMoved, plain);
    if (!require(text && *text == "<MPIDLE>", "Idle status must resolve MPIDLE.")) return 1;
    browser.status = 1;
    browser.dots = 0;
    text = dfre::session_status_draw(settings, browser, LobbyMenuAction::SliderMoved, plain);
    if (!require(text && *text == "<MPSEARCHING>.", "Searching needs the separator dot.")) return 1;
    browser.dots = 3;
    text = dfre::session_status_draw(settings, browser, LobbyMenuAction::SliderMoved, plain);
    if (!require(text && *text == "<MPSEARCHING>....", "Searching dots must append.")) return 1;
    if (!require(!dfre::session_status_draw(settings, browser, LobbyMenuAction::MouseUp, plain),
                 "Only the draw action shows status.")) return 1;

    // Row selection.
    browser.entries.assign(3, SessionEntry{});
    browser.selected = 0;
    IntentList intents = dfre::session_row_select(browser, 2);
    if (!require(browser.selected == 2, "Selection must update.")) return 1;
    if (!require(intents.size() == 2 &&
                 intents[0].button == "but_mp_s1" && intents[0].value == 1 &&
                 intents[1].button == "but_mp_s3" && intents[1].value == 3,
                 "Selection must reset the old row and push the new one.")) return 1;
    intents = dfre::session_row_select(browser, 5);
    if (!require(browser.selected == 2 && intents.size() == 1 && intents[0].value == 3,
                 "Invalid row must only push itself.")) return 1;

    // Multiplayer panel callbacks.
    intents = dfre::host_menu_action(LobbyMenuAction::MouseUp);
    if (!require(count(intents, MenuIntent::Kind::StopSessionSearch) == 1 &&
                 count(intents, MenuIntent::Kind::SessionObjectCall30) == 1 &&
                 find(intents, MenuIntent::Kind::SwitchMenu)->menu == "Host Menu" &&
                 count(intents, MenuIntent::Kind::SetButtonState) == 1,
                 "Host menu click must stop search, close session, open host menu.")) return 1;
    intents = dfre::host_menu_action(LobbyMenuAction::Hotkey);
    if (!require(count(intents, MenuIntent::Kind::SetButtonState) == 0 &&
                 count(intents, MenuIntent::Kind::SessionObjectCall30) == 1,
                 "Host hotkey must not press the button.")) return 1;

    dfre::PreConnect pre;
    browser.selected = 3;
    intents = dfre::join_menu_action(settings, browser, LobbyMenuAction::MouseUp, pre);
    if (!require(pre.mode == 1 && pre.service == 2 && pre.hosting == 0 && pre.flag == 1,
                 "Join pre-connect values must match 0x416f80.")) return 1;
    const MenuIntent* join = find(intents, MenuIntent::Kind::JoinSession);
    if (!require(join && join->value == 3 && count(intents, MenuIntent::Kind::StopSessionSearch) == 1,
                 "Join must store the selected index and stop search.")) return 1;
    pre = dfre::PreConnect{};
    intents = dfre::join_menu_action(settings, browser, LobbyMenuAction::Hotkey, pre);
    if (!require(pre.mode == 1 && count(intents, MenuIntent::Kind::JoinSession) == 0,
                 "Join hotkey sets pre-connect without the join call.")) return 1;
    if (!require(find(intents, MenuIntent::Kind::SwitchMenu)->menu == "Pre Menu",
                 "Join must open Pre Menu.")) return 1;

    browser.status = 0;
    intents = dfre::search_menu_action(browser, LobbyMenuAction::MouseDown);
    if (!require(count(intents, MenuIntent::Kind::StartSessionSearch) == 1 &&
                 intents[0].value == 3, "Idle search click must start search.")) return 1;
    browser.status = 1;
    intents = dfre::search_menu_action(browser, LobbyMenuAction::MouseDown);
    if (!require(count(intents, MenuIntent::Kind::StopSessionSearch) == 1,
                 "Searching click must stop search.")) return 1;
    intents = dfre::search_menu_action(browser, LobbyMenuAction::Hotkey);
    if (!require(intents.size() == 3 && intents[0].value == 3 && intents[2].value == 1,
                 "Search hotkey must press and release.")) return 1;

    // Host page field rules.
    settings = dfre::multiplayer_defaults();
    settings.dedicated = 1;
    intents = dfre::host_server_type(settings, LobbyMenuAction::MouseUp, "but_ho_sew");
    if (!require(settings.dedicated == 0 &&
                 count(intents, MenuIntent::Kind::RefreshHostMenu) == 1,
                 "Server type must toggle dedicated and refresh.")) return 1;

    settings.game_name[0] = '\0';
    intents = dfre::host_game_name(LobbyMenuAction::MouseUp);
    if (!require(intents.size() == 1 && intents[0].kind == MenuIntent::Kind::BeginTextEdit &&
                 intents[0].button == "but_ho_gnw" && intents[0].value == 32,
                 "Game name must open a 32 byte edit.")) return 1;
    intents = dfre::host_game_name_text(settings, LobbyMenuAction::TextCommitted, "Alpha");
    if (!require(dfre::settings_text(settings.game_name.data(), settings.game_name.size()) == "Alpha",
                 "Game name commit must copy.")) return 1;
    if (!require(intents.size() == 1 && intents[0].button == "but_ho_gn" && intents[0].value == 1,
                 "Game name commit must refresh the dot button.")) return 1;
    intents = dfre::host_session_password(LobbyMenuAction::MouseUp);
    if (!require(intents[0].button == "but_ho_spw" && intents[0].value == 17,
                 "Session password edit limit must be 17.")) return 1;
    dfre::host_session_password_text(settings, LobbyMenuAction::TextCommitted, "pass");
    dfre::host_country_text(settings, LobbyMenuAction::TextCommitted, "US");
    if (!require(dfre::settings_text(settings.game_password.data(), settings.game_password.size()) ==
                     "pass" &&
                 dfre::settings_text(settings.country.data(), settings.country.size()) == "US",
                 "Password and country commits must copy.")) return 1;

    dfre::HostState host;
    for (auto& names : host.map_names) {
        for (int index = 0; index < 64; ++index) {
            char name[8];
            std::snprintf(name, sizeof name, "Map%02d", index);
            names.push_back(name);
        }
    }
    host.map_list_counts = {6, 7, 3, 4, 5};
    host.map_list_bases = {10, 20, 30, 40, 50};
    settings = dfre::multiplayer_defaults();
    settings.mp_gametype = 1;
    settings.maxplayers = 16;
    intents = dfre::host_game_type(host, settings, LobbyMenuAction::MouseDown, 3);
    if (!require(settings.mp_gametype == 3, "Game type must take the button user id.")) return 1;
    if (!require(host.slider_max == 2 && host.slider_position == 0 && host.map_offset == 0,
                 "Game type 3 slider max must use its map count.")) return 1;
    if (!require(intents.front().button == "but_ho_gt2" && intents.front().value == 1,
                 "Previous game type button must reset.")) return 1;
    if (!require(intents.back().button == "but_ho_gt4" && intents.back().value == 3,
                 "Clicked game type button must push.")) return 1;
    settings.mp_gametype = 2;
    settings.maxplayers = 20;
    dfre::host_game_type(host, settings, LobbyMenuAction::MouseDown, 0);
    if (!require(settings.maxplayers == 8 && host.slider_max == 0,
                 "Coop selection must clamp maxplayers to 8.")) return 1;

    settings = dfre::multiplayer_defaults();
    settings.maxplayers = 33;
    intents = dfre::host_max_players(settings, LobbyMenuAction::MouseUp, 0);
    if (!require(settings.maxplayers == 32 && count(intents, MenuIntent::Kind::RefreshHostMenu) == 1,
                 "Max players plus must clamp to 32.")) return 1;
    settings.maxplayers = 2;
    dfre::host_max_players(settings, LobbyMenuAction::MouseUp, 1);
    if (!require(settings.maxplayers == 2, "Max players minus must clamp to 2.")) return 1;
    settings.maxplayers = 2;
    if (!require(dfre::host_max_players(settings, LobbyMenuAction::MouseDown, 1).empty(),
                 "Minus must not press at the low bound.")) return 1;
    if (!require(dfre::host_max_players(settings, LobbyMenuAction::MouseUp, 1).size() == 1,
                 "Max players update must not set a button state.")) return 1;

    settings = dfre::multiplayer_defaults();
    settings.mp_service = 2;
    settings.mp_gametype = 5;
    settings.maxplayers = 8;
    settings.dedicated = 1;
    intents = dfre::host_accept(host, settings, LobbyMenuAction::MouseUp, pre);
    if (!require(pre.mode == 1 && pre.service == 2 && pre.hosting == 1 && pre.flag == 0 &&
                 pre.max_players == 8 && pre.game_type == 5,
                 "Host accept must fill the pre-connect block.")) return 1;
    if (!require(pre.map_name == "Map50", "Host accept must copy the selected map.")) return 1;
    if (!require(find(intents, MenuIntent::Kind::SwitchMenu)->menu == "Pre Menu",
                 "Host accept must open Pre Menu.")) return 1;
    settings.mp_gametype = 0;
    dfre::PreConnect coop;
    dfre::host_accept(host, settings, LobbyMenuAction::MouseUp, coop);
    if (!require(coop.map_name.empty(), "Coop accept must not copy a map.")) return 1;

    intents = dfre::host_next(LobbyMenuAction::MouseDown);
    if (!require(intents.size() == 2 && intents[0].kind == MenuIntent::Kind::ResetMenuInput,
                 "Host next down must reset input.")) return 1;
    if (!require(dfre::host_next(LobbyMenuAction::MouseUp).size() == 2,
                 "Host next up must release and switch.")) return 1;

    // Host options page rules.
    settings = dfre::multiplayer_defaults();
    settings.replay = 2;
    dfre::host2_infinite_replay(settings, LobbyMenuAction::MouseUp, 0);
    if (!require(settings.replay == 0, "Replay must wrap up.")) return 1;
    settings.replay = 0;
    dfre::host2_infinite_replay(settings, LobbyMenuAction::MouseUp, 1);
    if (!require(settings.replay == 2, "Replay must wrap down.")) return 1;

    settings = dfre::multiplayer_defaults();
    settings.mp_gametype = 0;
    settings.maxplayers = 8;
    dfre::host2_max_players(settings, LobbyMenuAction::MouseUp, 0);
    if (!require(settings.maxplayers == 2, "Coop max players must wrap 8 to 2.")) return 1;
    settings.mp_gametype = 1;
    settings.maxplayers = 16;
    dfre::host2_max_players(settings, LobbyMenuAction::MouseUp, 0);
    if (!require(settings.maxplayers == 2, "Open max players must wrap 16 to 2.")) return 1;

    int slider = 0;
    settings = dfre::multiplayer_defaults();
    settings.max_kills = 100;
    dfre::host2_score_limit(settings, LobbyMenuAction::MouseUp, 0, slider);
    if (!require(settings.max_kills == 100 && slider == 100,
                 "Score limit must clamp at 100 and write the slider.")) return 1;
    slider = 500;
    dfre::host2_score_limit(settings, LobbyMenuAction::MouseUp, 0, slider);
    if (!require(settings.max_kills == 100, "Score limit must clamp from the slider.")) return 1;
    slider = -5;
    dfre::host2_score_limit(settings, LobbyMenuAction::SliderMoved, 0, slider);
    if (!require(settings.max_kills == 0 && slider == 1,
                 "Score drag below zero must set the slider to 1.")) return 1;
    slider = 500;
    dfre::host2_score_limit(settings, LobbyMenuAction::SliderMoved, 0, slider);
    if (!require(settings.max_kills == 500, "Score drag must not clamp above.")) return 1;
    slider = 0;
    settings.koth_limit = 1;
    dfre::host2_koth_limit(settings, LobbyMenuAction::MouseUp, 1, slider);
    if (!require(settings.koth_limit == 1, "KOH limit must not fall below 1.")) return 1;
    slider = 0;
    settings.timeout = 0;
    dfre::host2_timeout(settings, LobbyMenuAction::SliderMoved, 0, slider);
    if (!require(settings.timeout == 0 && slider == 0, "Timeout drag at zero must keep 0.")) return 1;

    settings = dfre::multiplayer_defaults();
    settings.ammo_respawn = 3;
    dfre::host2_ammo_respawn(settings, LobbyMenuAction::MouseUp, 0);
    if (!require(settings.ammo_respawn == 0, "Ammo respawn must wrap.")) return 1;
    settings.attributes = 0;
    dfre::host2_breakx(settings, LobbyMenuAction::MouseUp);
    dfre::host2_da_tracers(settings, LobbyMenuAction::MouseUp);
    dfre::host2_team_selection(settings, LobbyMenuAction::MouseUp);
    if (!require(settings.attributes == 0x43, "Attribute toggles must set bits 0, 1, and 6.")) return 1;

    settings = dfre::multiplayer_defaults();
    intents = dfre::host2_password(LobbyMenuAction::MouseUp, 1);
    if (!require(intents.size() == 1 && intents[0].button == "but_ho2_btw" && intents[0].value == 10,
                 "Blue password must open a 10 byte edit.")) return 1;
    intents = dfre::host2_password_text(settings, LobbyMenuAction::MouseDown, 0, {});
    if (!require(intents.size() == 1 && intents[0].button == "but_ho2_rtw" && intents[0].value == 3,
                 "Password text down must press the edit button.")) return 1;
    intents = dfre::host2_password_text(settings, LobbyMenuAction::TextCommitted, 1, "SECRET");
    if (!require(dfre::settings_text(settings.blue_password.data(), settings.blue_password.size()) ==
                     "SECRET",
                 "Blue password commit must copy.")) return 1;
    if (!require(intents.size() == 1 && intents[0].button == "but_ho2_bt",
                 "Blue commit must refresh with but_ho2_bt.")) return 1;
    intents = dfre::host2_password_text(settings, LobbyMenuAction::TextChanged, 1, {});
    if (!require(intents.size() == 1 && intents[0].button == "but_mp_bt",
                 "Blue change must refresh with but_mp_bt.")) return 1;
    return 0;
}
