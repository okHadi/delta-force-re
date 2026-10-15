#include "dfre/lobby_session.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using dfre::IntentList;
using dfre::MenuIntent;

std::string hex_text(std::string_view text) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (const char c : text) {
        out += digits[static_cast<unsigned char>(c) >> 4];
        out += digits[static_cast<unsigned char>(c) & 15];
    }
    if (out.empty()) {
        out = "-";
    }
    return out;
}

const char* intent_kind(const MenuIntent& intent) {
    switch (intent.kind) {
        case MenuIntent::Kind::SetButtonState: return "SetButtonState";
        case MenuIntent::Kind::BeginTextEdit: return "BeginTextEdit";
        case MenuIntent::Kind::SwitchMenu: return "SwitchMenu";
        case MenuIntent::Kind::ResetMenuInput: return "ResetMenuInput";
        case MenuIntent::Kind::StopSessionSearch: return "StopSessionSearch";
        case MenuIntent::Kind::StartSessionSearch: return "StartSessionSearch";
        case MenuIntent::Kind::JoinSession: return "JoinSession";
        case MenuIntent::Kind::RefreshHostMenu: return "RefreshHostMenu";
        case MenuIntent::Kind::RefreshHost2Menu: return "RefreshHost2Menu";
        case MenuIntent::Kind::UpdateHostMapList: return "UpdateHostMapList";
        case MenuIntent::Kind::SessionObjectCall30: return "SessionObjectCall30";
    }
    return "?";
}

void print_intents(const IntentList& intents) {
    for (const MenuIntent& intent : intents) {
        std::cout << "INT " << intent_kind(intent);
        if (!intent.button.empty()) {
            std::cout << ' ' << intent.button;
        }
        if (intent.kind == MenuIntent::Kind::SetButtonState ||
            intent.kind == MenuIntent::Kind::BeginTextEdit ||
            intent.kind == MenuIntent::Kind::JoinSession) {
            std::cout << ' ' << intent.value;
        }
        if (!intent.menu.empty()) {
            std::cout << ' ' << intent.menu;
        }
        std::cout << '\n';
    }
    std::cout << "END\n";
}

dfre::LobbyMenuAction to_action(int value) {
    return static_cast<dfre::LobbyMenuAction>(value);
}

std::vector<std::string> test_map_names() {
    std::vector<std::string> names;
    for (int index = 0; index < 64; ++index) {
        char buffer[16];
        std::snprintf(buffer, sizeof buffer, "Map%02d", index);
        names.push_back(buffer);
    }
    return names;
}

dfre::HostState make_host(std::istringstream& in) {
    dfre::HostState host;
    for (int& count : host.map_list_counts) {
        in >> count;
    }
    for (int& base : host.map_list_bases) {
        in >> base;
    }
    in >> host.map_offset;
    for (auto& names : host.map_names) {
        names = test_map_names();
    }
    return host;
}

dfre::MultiplayerSettings make_settings(std::istringstream& in) {
    dfre::MultiplayerSettings settings = dfre::multiplayer_defaults();
    int mp_service = 0, mp_gametype = 0, maxplayers = 0, dedicated = 0, replay = 0,
        max_kills = 0, time_limit = 0, koth_limit = 0, ammo_respawn = 0, timeout = 0,
        attributes = 0;
    in >> mp_service >> mp_gametype >> maxplayers >> dedicated >> replay >> max_kills >> time_limit
        >> koth_limit >> ammo_respawn >> timeout >> attributes;
    settings.mp_service = mp_service;
    settings.mp_gametype = mp_gametype;
    settings.maxplayers = maxplayers;
    settings.dedicated = dedicated;
    settings.replay = replay;
    settings.max_kills = max_kills;
    settings.time_limit = time_limit;
    settings.koth_limit = koth_limit;
    settings.ammo_respawn = ammo_respawn;
    settings.timeout = timeout;
    settings.attributes = attributes;
    return settings;
}

std::string read_text(std::istringstream& in) {
    std::string value;
    in >> value;
    return value;
}
} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string command;
        in >> command;
        if (command == "mp-host") {
            int action = 0;
            in >> action;
            print_intents(dfre::host_menu_action(to_action(action)));
        } else if (command == "mp-join") {
            int action = 0, selected = 0;
            dfre::MultiplayerSettings settings = make_settings(in);
            in >> action >> selected;
            dfre::SessionBrowser browser;
            browser.selected = selected;
            dfre::PreConnect pre;
            print_intents(dfre::join_menu_action(settings, browser, to_action(action), pre));
            std::cout << "PRE " << pre.mode << ' ' << pre.service << ' ' << pre.hosting << ' '
                      << pre.flag << ' ' << pre.max_players << ' ' << pre.game_type << '\n';
        } else if (command == "mp-search") {
            int status = 0, action = 0;
            in >> status >> action;
            dfre::SessionBrowser browser;
            browser.status = status;
            print_intents(dfre::search_menu_action(browser, to_action(action)));
        } else if (command == "select") {
            int selected = 0, count = 0, row = 0;
            in >> selected >> count >> row;
            dfre::SessionBrowser browser;
            for (int at = 0; at < count; ++at) {
                browser.entries.push_back({});
            }
            browser.selected = selected;
            print_intents(dfre::session_row_select(browser, row));
            std::cout << "SET selected=" << browser.selected << '\n';
        } else if (command == "row") {
            dfre::SessionBrowser browser;
            dfre::SessionEntry entry;
            in >> entry.name >> entry.players >> entry.max_players >> entry.game_type >> entry.flag >>
                entry.version >> browser.local_server_id;
            browser.entries.push_back(entry);
            const auto view = dfre::session_row_view(browser, 0);
            if (!view) {
                std::cout << "TEXT -\n";
            } else {
                const auto resolve = [](std::string_view key) { return "<" + std::string(key) + ">"; };
                std::cout << "ROW " << view->name << ' ' << view->players << ' ' << view->max_players
                          << ' ' << view->game_type_key << ' ' << view->version_text << ' '
                          << (view->self ? 1 : 0) << ' ' << (view->dedicated ? 1 : 0) << '\n';
                std::cout << "TEXT " << hex_text(dfre::session_row_text(*view, resolve)) << '\n';
            }
        } else if (command == "status") {
            int service = 0, state = 0, dots = 0, action = 0;
            in >> service >> state >> dots >> action;
            dfre::MultiplayerSettings settings = dfre::multiplayer_defaults();
            settings.mp_service = service;
            dfre::SessionBrowser browser;
            browser.status = state;
            browser.dots = dots;
            const auto resolve = [](std::string_view key) { return "<" + std::string(key) + ">"; };
            const auto text = dfre::session_status_draw(settings, browser, to_action(action), resolve);
            std::cout << "TEXT " << (text ? hex_text(*text) : "-") << '\n';
        } else if (command == "gamename") {
            int action = 0;
            in >> action;
            print_intents(dfre::host_game_name(to_action(action)));
        } else if (command == "gamename-text") {
            int action = 0;
            in >> action;
            dfre::MultiplayerSettings settings = dfre::multiplayer_defaults();
            print_intents(dfre::host_game_name_text(settings, to_action(action), read_text(in)));
            std::cout << "SET game_name=" << hex_text(dfre::settings_text(
                             settings.game_name.data(), settings.game_name.size())) << '\n';
        } else if (command == "sessionpassword") {
            int action = 0;
            in >> action;
            print_intents(dfre::host_session_password(to_action(action)));
        } else if (command == "sessionpassword-text") {
            int action = 0;
            in >> action;
            dfre::MultiplayerSettings settings = dfre::multiplayer_defaults();
            print_intents(dfre::host_session_password_text(settings, to_action(action), read_text(in)));
            std::cout << "SET game_password=" << hex_text(dfre::settings_text(
                             settings.game_password.data(), settings.game_password.size())) << '\n';
        } else if (command == "country") {
            int action = 0;
            in >> action;
            print_intents(dfre::host_country(to_action(action)));
        } else if (command == "country-text") {
            int action = 0;
            in >> action;
            dfre::MultiplayerSettings settings = dfre::multiplayer_defaults();
            print_intents(dfre::host_country_text(settings, to_action(action), read_text(in)));
            std::cout << "SET country=" << hex_text(
                             dfre::settings_text(settings.country.data(), settings.country.size()))
                      << '\n';
        } else if (command == "servertype") {
            int action = 0;
            std::string button;
            in >> action >> button;
            dfre::MultiplayerSettings settings = dfre::multiplayer_defaults();
            print_intents(dfre::host_server_type(settings, to_action(action), button));
            std::cout << "SET dedicated=" << settings.dedicated << '\n';
        } else if (command == "gametype") {
            int action = 0, user_id = 0;
            dfre::HostState host = make_host(in);
            dfre::MultiplayerSettings settings = make_settings(in);
            in >> action >> user_id;
            print_intents(dfre::host_game_type(host, settings, to_action(action), user_id));
            std::cout << "SET gametype=" << settings.mp_gametype << " maxplayers="
                      << settings.maxplayers << " slider_max=" << host.slider_max << " slider_pos="
                      << host.slider_position << " map_offset=" << host.map_offset << '\n';
        } else if (command == "maxplayers") {
            int action = 0, minus = 0;
            dfre::MultiplayerSettings settings = make_settings(in);
            in >> action >> minus;
            print_intents(dfre::host_max_players(settings, to_action(action), minus != 0));
            std::cout << "SET maxplayers=" << settings.maxplayers << '\n';
        } else if (command == "hostnext") {
            int action = 0;
            in >> action;
            print_intents(dfre::host_next(to_action(action)));
        } else if (command == "hostaccept") {
            int action = 0;
            dfre::HostState host = make_host(in);
            dfre::MultiplayerSettings settings = make_settings(in);
            in >> action;
            dfre::PreConnect pre;
            print_intents(dfre::host_accept(host, settings, to_action(action), pre));
            std::cout << "PRE " << pre.mode << ' ' << pre.service << ' ' << pre.hosting << ' '
                      << pre.flag << ' ' << pre.max_players << ' ' << pre.game_type << '\n';
            std::cout << "MAP " << (pre.map_name.empty() ? std::string("-") : pre.map_name) << '\n';
        } else if (command == "host2-accept") {
            int action = 0;
            in >> action;
            print_intents(dfre::host2_accept(to_action(action)));
        } else if (command == "host2-next") {
            int action = 0;
            in >> action;
            print_intents(dfre::host2_next(to_action(action)));
        } else if (command == "host2-replay") {
            int action = 0, minus = 0;
            dfre::MultiplayerSettings settings = make_settings(in);
            in >> action >> minus;
            print_intents(dfre::host2_infinite_replay(settings, to_action(action), minus != 0));
            std::cout << "SET replay=" << settings.replay << '\n';
        } else if (command == "host2-maxplayers") {
            int action = 0, minus = 0;
            dfre::MultiplayerSettings settings = make_settings(in);
            in >> action >> minus;
            print_intents(dfre::host2_max_players(settings, to_action(action), minus != 0));
            std::cout << "SET maxplayers=" << settings.maxplayers << '\n';
        } else if (command == "host2-score" || command == "host2-time" ||
                   command == "host2-koth" || command == "host2-timeout") {
            int action = 0, minus = 0, slider = 0;
            dfre::MultiplayerSettings settings = make_settings(in);
            in >> action >> minus >> slider;
            IntentList intents;
            if (command == "host2-score") {
                intents = dfre::host2_score_limit(settings, to_action(action), minus != 0, slider);
            } else if (command == "host2-time") {
                intents = dfre::host2_time_limit(settings, to_action(action), minus != 0, slider);
            } else if (command == "host2-koth") {
                intents = dfre::host2_koth_limit(settings, to_action(action), minus != 0, slider);
            } else {
                intents = dfre::host2_timeout(settings, to_action(action), minus != 0, slider);
            }
            print_intents(intents);
            std::cout << "SET max_kills=" << settings.max_kills << " time_limit="
                      << settings.time_limit << " koth_limit=" << settings.koth_limit
                      << " timeout=" << settings.timeout << " slider=" << slider << '\n';
        } else if (command == "host2-ammo") {
            int action = 0, minus = 0;
            dfre::MultiplayerSettings settings = make_settings(in);
            in >> action >> minus;
            print_intents(dfre::host2_ammo_respawn(settings, to_action(action), minus != 0));
            std::cout << "SET ammo_respawn=" << settings.ammo_respawn << '\n';
        } else if (command == "host2-breakx" || command == "host2-teamsel" ||
                   command == "host2-tracers") {
            int action = 0;
            dfre::MultiplayerSettings settings = make_settings(in);
            in >> action;
            if (command == "host2-breakx") {
                print_intents(dfre::host2_breakx(settings, to_action(action)));
            } else if (command == "host2-teamsel") {
                print_intents(dfre::host2_team_selection(settings, to_action(action)));
            } else {
                print_intents(dfre::host2_da_tracers(settings, to_action(action)));
            }
            std::cout << "SET attributes=" << settings.attributes << '\n';
        } else if (command == "host2-password") {
            int action = 0, user_id = 0;
            in >> action >> user_id;
            print_intents(dfre::host2_password(to_action(action), user_id));
        } else if (command == "host2-password-text") {
            int action = 0, user_id = 0;
            in >> action >> user_id;
            dfre::MultiplayerSettings settings = dfre::multiplayer_defaults();
            print_intents(
                dfre::host2_password_text(settings, to_action(action), user_id, read_text(in)));
            std::cout << "SET red=" << hex_text(dfre::settings_text(
                             settings.red_password.data(), settings.red_password.size()))
                      << " blue=" << hex_text(dfre::settings_text(
                             settings.blue_password.data(), settings.blue_password.size()))
                      << '\n';
        } else {
            std::cerr << "Unknown command.\n";
            return 1;
        }
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}
