// Item runtime probe: maps ITEMS.DEF text through the native item_runtime port
// and prints records as hex for tools/check_item_runtime_original.py.
//
// Commands:
//   item_runtime_probe items      stdin = decoded ITEMS.DEF text, JSON records
//   item_runtime_probe constants  JSON layout, flag and type constants
#include "dfre/item_runtime.hpp"

#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

namespace {

std::string hex_record(const dfre::RuntimeItemDefinition& item) {
    const auto* bytes = dfre::runtime_item_bytes(item);
    static const char digits[] = "0123456789abcdef";
    std::string out(dfre::runtime_item_size * 2, '0');
    for (std::size_t i = 0; i < dfre::runtime_item_size; ++i) {
        out[i * 2] = digits[bytes[i] >> 4];
        out[i * 2 + 1] = digits[bytes[i] & 15];
    }
    return out;
}

std::string read_stdin() {
    return std::string(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
}

int command_items() {
    const auto text = read_stdin();
    const dfre::ItemCatalog catalog(text);
    const auto items = dfre::build_runtime_items(catalog);
    std::ostringstream json;
    json << "{\"count\":" << items.size() << ",\"record_size\":" << dfre::runtime_item_size << ",\"records\":[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) json << ",";
        json << "\"" << hex_record(items[i]) << "\"";
    }
    json << "]}";
    std::cout << json.str() << "\n";
    return 0;
}

int command_constants() {
    std::ostringstream json;
    json << "{"
         << "\"record_size\":" << dfre::runtime_item_size
         << ",\"name_max\":" << dfre::runtime_item_name_max
         << ",\"id_bias\":" << dfre::runtime_item_id_bias
         << ",\"ambient_slots\":" << dfre::runtime_item_ambient_slots
         << ",\"ambient_size\":" << dfre::runtime_item_ambient_string
         << ",\"offsets\":{"
         << "\"name\":" << offsetof(dfre::RuntimeItemDefinition, name)
         << ",\"id\":" << offsetof(dfre::RuntimeItemDefinition, id)
         << ",\"flags\":" << offsetof(dfre::RuntimeItemDefinition, flags)
         << ",\"type\":" << offsetof(dfre::RuntimeItemDefinition, type)
         << ",\"graphic\":" << offsetof(dfre::RuntimeItemDefinition, graphic)
         << ",\"husk\":" << offsetof(dfre::RuntimeItemDefinition, husk)
         << ",\"chr_file\":" << offsetof(dfre::RuntimeItemDefinition, chr_file)
         << ",\"anim_def\":" << offsetof(dfre::RuntimeItemDefinition, anim_def)
         << ",\"model\":" << offsetof(dfre::RuntimeItemDefinition, model)
         << ",\"husk_model\":" << offsetof(dfre::RuntimeItemDefinition, husk_model)
         << ",\"character\":" << offsetof(dfre::RuntimeItemDefinition, character)
         << ",\"animation\":" << offsetof(dfre::RuntimeItemDefinition, animation)
         << ",\"ai_function\":" << offsetof(dfre::RuntimeItemDefinition, ai_function)
         << ",\"ai_data\":" << offsetof(dfre::RuntimeItemDefinition, ai_data)
         << ",\"move_function\":" << offsetof(dfre::RuntimeItemDefinition, move_function)
         << ",\"move_data\":" << offsetof(dfre::RuntimeItemDefinition, move_data)
         << ",\"hp\":" << offsetof(dfre::RuntimeItemDefinition, hp)
         << ",\"hp_secondary\":" << offsetof(dfre::RuntimeItemDefinition, hp_secondary)
         << ",\"effect_count\":" << offsetof(dfre::RuntimeItemDefinition, effect_count)
         << ",\"phrase_set\":" << offsetof(dfre::RuntimeItemDefinition, phrase_set)
         << ",\"face_set\":" << offsetof(dfre::RuntimeItemDefinition, face_set)
         << ",\"acceleration\":" << offsetof(dfre::RuntimeItemDefinition, acceleration)
         << ",\"turn_rate\":" << offsetof(dfre::RuntimeItemDefinition, turn_rate)
         << ",\"axle_dist\":" << offsetof(dfre::RuntimeItemDefinition, axle_dist)
         << ",\"ambient\":" << offsetof(dfre::RuntimeItemDefinition, ambient)
         << ",\"ambient_sound\":" << offsetof(dfre::RuntimeItemDefinition, ambient_sound)
         << ",\"interrupt_sound_id\":" << offsetof(dfre::RuntimeItemDefinition, interrupt_sound_id)
         << "},\"flags\":{"
         << "\"good\":" << unsigned(dfre::item_flag_good)
         << ",\"evil\":" << unsigned(dfre::item_flag_evil)
         << ",\"aidata\":" << unsigned(dfre::item_flag_aidata)
         << ",\"fo\":" << unsigned(dfre::item_flag_fo)
         << ",\"movecb\":" << unsigned(dfre::item_flag_movecb)
         << ",\"powerup\":" << unsigned(dfre::item_flag_powerup)
         << ",\"flesh\":" << unsigned(dfre::item_flag_flesh)
         << ",\"noak\":" << unsigned(dfre::item_flag_noak)
         << "},\"types\":{"
         << "\"vehicle\":" << std::uint32_t(dfre::RuntimeItemType::vehicle)
         << ",\"decoration\":" << std::uint32_t(dfre::RuntimeItemType::decoration)
         << ",\"person\":" << std::uint32_t(dfre::RuntimeItemType::person)
         << ",\"marker\":" << std::uint32_t(dfre::RuntimeItemType::marker)
         << ",\"building\":" << std::uint32_t(dfre::RuntimeItemType::building)
         << ",\"object\":" << std::uint32_t(dfre::RuntimeItemType::object)
         << ",\"effect\":" << std::uint32_t(dfre::RuntimeItemType::effect)
         << "}}";
    std::cout << json.str() << "\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: item_runtime_probe items|constants\n"
                         "  items      read decoded ITEMS.DEF text on stdin, print JSON records\n"
                         "  constants  print record layout, flag and type constants\n";
            return 2;
        }
        const std::string command = argv[1];
        if (command == "items") return command_items();
        if (command == "constants") return command_constants();
        std::cerr << "unknown command " << command << "\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
