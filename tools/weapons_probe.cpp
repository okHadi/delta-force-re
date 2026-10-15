// weapons_probe: scripted weapon-state probe over the native WeaponState.
// Reads a line script from stdin (or argv[1]) and prints one JSON line per
// step. scripts/check_weapons_original.py runs the same script against the
// original Df.exe code and compares the output.
//
// Script commands:
//   slot_map A B C D E F G H I J
//   unlimited 0|1
//   resupply 0|1
//   select SLOT
//   cycle -1|1
//   fire
//   reload
//   aim
//   tick COUNT TRIGGER
//   tables
#include "dfre/weapons.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
namespace {
void arrays(const dfre::WeaponState& state, std::ostream& out) {
    out << "\"magazine\":[";
    for (std::size_t i = 0; i < dfre::kWeaponClassCount; ++i) out << (i ? "," : "") << state.magazine(int(i));
    out << "],\"reserve\":[";
    for (std::size_t i = 0; i < dfre::kWeaponClassCount; ++i) out << (i ? "," : "") << state.reserve(int(i));
    out << "],\"pending\":[";
    for (std::size_t i = 0; i < dfre::kWeaponClassCount; ++i) out << (i ? "," : "") << state.pending(int(i));
    out << "]";
}
void events(dfre::WeaponState& state, std::ostream& out) {
    out << "\"events\":[";
    bool first = true;
    for (const auto& event : state.take_events()) {
        if (!first) out << ",";
        first = false;
        const char* kind = "?";
        switch (event.kind) {
            case dfre::WeaponEventKind::shot: kind = "shot"; break;
            case dfre::WeaponEventKind::sound: kind = "sound"; break;
            case dfre::WeaponEventKind::sound2: kind = "sound2"; break;
            case dfre::WeaponEventKind::place: kind = "place"; break;
            case dfre::WeaponEventKind::queue: kind = "queue"; break;
            case dfre::WeaponEventKind::message: kind = "message"; break;
        }
        out << "[\"" << kind << "\"," << event.value << "]";
    }
    out << "]";
}
void state_line(dfre::WeaponState& state, const char* op, std::ostream& out) {
    out << "{\"op\":\"" << op << "\",\"slot\":" << state.slot()
        << ",\"weapon\":" << int(state.current_weapon())
        << ",\"fire_timer\":" << state.fire_timer()
        << ",\"reload_timer\":" << state.reload_timer()
        << ",\"aim\":" << (state.aim_blocked() ? 1 : 0)
        << ",\"burst\":" << state.burst_counter()
        << ",\"raise\":" << state.raise_counter() << ",";
    arrays(state, out);
    out << ",";
    events(state, out);
    out << "}\n";
}
void state_line_ret(dfre::WeaponState& state, const char* op, bool ret, std::ostream& out) {
    out << "{\"op\":\"" << op << "\",\"ret\":" << (ret ? "true" : "false")
        << ",\"slot\":" << state.slot()
        << ",\"weapon\":" << int(state.current_weapon())
        << ",\"fire_timer\":" << state.fire_timer()
        << ",\"reload_timer\":" << state.reload_timer()
        << ",\"aim\":" << (state.aim_blocked() ? 1 : 0)
        << ",\"burst\":" << state.burst_counter()
        << ",\"raise\":" << state.raise_counter() << ",";
    arrays(state, out);
    out << ",";
    events(state, out);
    out << "}\n";
}
void dump_tables(std::ostream& out) {
    out << "{\"op\":\"tables\",\"weapons\":[";
    const auto& weapons = dfre::weapon_definitions();
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        const auto& w = weapons[i];
        if (i) out << ",";
        out << "[" << w.primary_item << "," << w.secondary_item << "," << int(w.use_class) << ","
            << int(w.ui_index) << "," << int(w.weapon_class) << "," << w.muzzle_x << "," << w.muzzle_y << ","
            << w.muzzle_z << "," << w.aim_x << "," << w.aim_y << "," << w.aim_z << "," << w.aim_field << ","
            << w.fire_delay << "," << w.burst_field << "," << w.flags << "]";
    }
    out << "],\"classes\":[";
    const auto& classes = dfre::weapon_classes();
    for (std::size_t i = 0; i < classes.size(); ++i) {
        const auto& c = classes[i];
        if (i) out << ",";
        out << "[" << c.id << "," << c.magazine_count << "," << c.magazine_size << "," << c.field_0c << ","
            << c.field_10 << "," << c.field_14 << "," << c.field_18 << "," << c.field_1c << "," << c.field_20 << ","
            << c.action_sound << "," << c.reload_sound << "," << c.empty_sound << "," << c.field_30 << "]";
    }
    out << "]}\n";
}
}
int main(int argc, char** argv) {
    std::istream* input = &std::cin;
    std::ifstream file;
    if (argc > 1) {
        file.open(argv[1]);
        if (!file) { std::cerr << "cannot open script\n"; return 1; }
        input = &file;
    }
    dfre::WeaponState state;
    std::string line;
    while (std::getline(*input, line)) {
        std::istringstream words(line);
        std::string command;
        if (!(words >> command) || command.empty() || command[0] == '#') continue;
        if (command == "slot_map") {
            std::array<std::uint8_t, dfre::kWeaponSlotCount> map{};
            int value = 0;
            std::size_t at = 0;
            while (words >> value) { if (at < map.size()) map[at++] = std::uint8_t(value); }
            state.set_slot_map(map);
        } else if (command == "unlimited") {
            int value = 0; words >> value; state.set_unlimited_ammo(value != 0);
        } else if (command == "resupply") {
            int value = 0; words >> value;
            state.resupply(value != 0);
            state_line(state, "resupply", std::cout);
        } else if (command == "select") {
            int value = 0; words >> value;
            bool ret = state.select_slot(value);
            state_line_ret(state, "select", ret, std::cout);
        } else if (command == "cycle") {
            int value = 0; words >> value;
            bool ret = state.cycle_slot(value);
            state_line_ret(state, "cycle", ret, std::cout);
        } else if (command == "fire") {
            bool ret = state.fire();
            state_line_ret(state, "fire", ret, std::cout);
        } else if (command == "reload") {
            bool ret = state.reload_press();
            state_line_ret(state, "reload", ret, std::cout);
        } else if (command == "aim") {
            state.toggle_aim_block();
            state_line(state, "aim", std::cout);
        } else if (command == "tick") {
            int count = 0; int trigger = 0; words >> count >> trigger;
            for (int i = 0; i < count; ++i) {
                state.tick(trigger != 0);
                std::ostringstream name;
                name << "tick" << i;
                state_line(state, name.str().c_str(), std::cout);
            }
        } else if (command == "tables") {
            dump_tables(std::cout);
        } else {
            std::cerr << "unknown command: " << command << "\n";
            return 1;
        }
    }
    return 0;
}
