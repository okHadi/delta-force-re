// Stateful probe for the actor runtime stage. Commands (stdin, one per line):
//   reset
//   config <mode> <night> <scale> <game_type> <c6001> <c6002> <c6003> <c6004> <counter>
//   def <index> <item_id> <graphic> <character> <ai> <move> <hp>
//   marker <index> <definition_index> <p0> <p1> <p2> <p3> <p4> <p5>
//   place <index> <object_id> <active> <field_9c> <arg1> <arg2> <team_code> <name_hex>
//   class <mode> <night> <side>
//   create <arg1> <arg2> <person> <character>
//   bind <placement_index> <actor_index>
//   spawn <person0> <character0> ... <person16> <character16>
//   init <player_object_id> <weapon_slot> <field_98> <name_hex>
//   dump <actor_index>
// Numbers are hexadecimal. Setup commands print nothing; actions print one
// line. Empty hex payloads use "-".

#include "dfre/actor_runtime.hpp"

#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool parse_hex(const std::string& text, std::vector<std::uint8_t>& out) {
    out.clear();
    if (text == "-") return true;
    if (text.size() % 2) return false;
    for (std::size_t at = 0; at < text.size(); at += 2) {
        const int high = hex_digit(text[at]);
        const int low = hex_digit(text[at + 1]);
        if (high < 0 || low < 0) return false;
        out.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return true;
}

std::string to_hex(const std::uint8_t* bytes, std::size_t size) {
    if (size == 0) return "-";
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        text.push_back(digits[bytes[i] >> 4]);
        text.push_back(digits[bytes[i] & 15]);
    }
    return text;
}

std::string parse_text_hex(const std::string& text) {
    std::vector<std::uint8_t> bytes;
    if (!parse_hex(text, bytes)) return {};
    return std::string(bytes.begin(), bytes.end());
}

struct State {
    dfre::ActorFactoryConfig factory_config;
    dfre::ActorBindConfig bind_config;
    std::uint32_t bind_counter = 0;
    std::map<std::size_t, dfre::ActorDefinition> definitions;
    std::map<std::size_t, std::array<std::uint32_t, 7>> markers;
    std::map<std::size_t, dfre::ActorPlacement> placements;
    std::optional<dfre::RuntimeActorFactory> factory;

    dfre::ActorDefinitionTable definition_table() const {
        std::vector<dfre::ActorDefinition> entries;
        for (const auto& [index, definition] : definitions) {
            (void)index;
            entries.push_back(definition);
        }
        return dfre::ActorDefinitionTable(std::move(entries));
    }

    dfre::MarkerPoseTable marker_table() const {
        std::vector<dfre::MarkerImage> records;
        for (const auto& [index, fields] : markers) {
            (void)index;
            dfre::MarkerImage image{};
            for (std::size_t i = 0; i < 7; ++i) {
                const std::uint32_t word = fields[i];
                const std::size_t at = i == 0 ? 0 : 0x24 + (i - 1) * 4;
                image[at + 0] = static_cast<std::uint8_t>(word);
                image[at + 1] = static_cast<std::uint8_t>(word >> 8);
                image[at + 2] = static_cast<std::uint8_t>(word >> 16);
                image[at + 3] = static_cast<std::uint8_t>(word >> 24);
            }
            records.push_back(image);
        }
        return dfre::MarkerPoseTable(std::move(records));
    }

    std::vector<dfre::ActorPlacement> placement_list() const {
        std::vector<dfre::ActorPlacement> list(dfre::kPlacementCount);
        for (const auto& [index, placement] : placements) {
            if (index < list.size()) list[index] = placement;
        }
        return list;
    }

    void store_list(const std::vector<dfre::ActorPlacement>& list) {
        for (std::size_t i = 0; i < list.size(); ++i) {
            if (placements.count(i)) placements[i] = list[i];
        }
    }
};

int run_create(State& state, std::istringstream& fields) {
    std::uint32_t argument1 = 0, argument2 = 0, person = 0, character = 0;
    fields >> std::hex >> argument1 >> argument2 >> person >> character;
    if (!fields) return 1;
    if (!state.factory) {
        state.factory = dfre::RuntimeActorFactory(state.definition_table(), state.factory_config);
    }
    const dfre::ActorCreateHandles handles{person, character};
    const auto slot = state.factory->create(argument1, argument2, handles);
    if (!slot) {
        std::cout << "- " << state.factory->count() << " -\n";
        return 0;
    }
    const dfre::ActorImage image = dfre::actor_image(state.factory->actor(*slot));
    std::cout << *slot << ' ' << state.factory->count() << ' '
              << to_hex(image.data(), image.size()) << '\n';
    return 0;
}

int run_bind(State& state, std::istringstream& fields) {
    std::uint32_t placement = 0, actor = 0;
    fields >> std::hex >> placement >> actor;
    if (!fields || !state.factory) return 1;
    if (placement >= dfre::kPlacementCount) return 1;
    dfre::RuntimeActor& target = state.factory->actor(actor);
    dfre::bind_actor_pose(state.bind_counter, state.bind_config, state.marker_table(),
                          state.factory->definitions(), placement, target);
    const dfre::ActorImage image = dfre::actor_image(target);
    std::cout << to_hex(image.data(), image.size()) << ' ' << std::hex << state.bind_counter
              << '\n';
    return 0;
}

int run_spawn(State& state, std::istringstream& fields) {
    std::vector<dfre::ActorCreateHandles> handles(dfre::kPlacementCount);
    for (auto& handle : handles) {
        fields >> std::hex >> handle.person >> handle.character;
    }
    if (!fields) return 1;
    state.factory = dfre::RuntimeActorFactory(state.definition_table(), state.factory_config);
    auto placements = state.placement_list();
    const std::vector<dfre::ActorSpawn> spawns = dfre::spawn_placement_actors(
        *state.factory, placements, handles, state.bind_counter, state.bind_config,
        state.marker_table(), true);
    state.store_list(placements);
    for (const dfre::ActorSpawn& spawn : spawns) {
        const dfre::ActorImage image = dfre::actor_image(state.factory->actor(spawn.actor));
        std::cout << spawn.placement << ' ' << spawn.actor << ' ' << std::hex << spawn.person
                  << ' ' << to_hex(reinterpret_cast<const std::uint8_t*>(spawn.name.data()),
                                               spawn.name.size())
                  << ' ' << to_hex(image.data(), image.size()) << ' ' << std::hex
                  << state.bind_counter << '\n';
    }
    std::cout << "count " << std::hex << state.factory->count() << '\n';
    return 0;
}

int run_init(State& state, std::istringstream& fields) {
    std::uint32_t object_id = 0, weapon_slot = 0, field_98 = 0;
    std::string name_text;
    fields >> std::hex >> object_id >> weapon_slot >> field_98 >> name_text;
    if (!fields || !state.factory) return 1;
    const std::string name = parse_text_hex(name_text);
    const auto result = dfre::bind_local_player_actor(*state.factory, object_id, weapon_slot,
                                                     field_98, name);
    if (!result) {
        std::cout << "0 - - - -\n";
        return 0;
    }
    std::cout << "1 " << result->actor << ' ' << std::hex << result->field_94 << ' '
              << result->field_98 << ' '
              << to_hex(reinterpret_cast<const std::uint8_t*>(result->person_name.data()),
                        result->person_name.size())
              << '\n';
    return 0;
}

int run(State& state, std::istringstream& fields, const std::string& command) {
    if (command == "reset") {
        state = State{};
        return 0;
    }
    if (command == "config") {
        std::uint32_t mode = 0, night = 0, scale = 0, game_type = 0;
        std::uint32_t c1 = 0, c2 = 0, c3 = 0, c4 = 0, counter = 0;
        fields >> std::hex >> mode >> night >> scale >> game_type >> c1 >> c2 >> c3 >> c4
               >> counter;
        if (!fields) return 1;
        state.factory_config.mode = mode;
        state.factory_config.night = night != 0;
        state.factory_config.scale_health = scale != 0;
        state.bind_config.game_type = game_type;
        state.bind_config.count_6001 = c1;
        state.bind_config.count_6002 = c2;
        state.bind_config.count_6003 = c3;
        state.bind_config.count_6004 = c4;
        state.bind_counter = counter;
        state.factory.reset();
        return 0;
    }
    if (command == "def") {
        std::uint32_t index = 0, item_id = 0, graphic = 0, character = 0, ai = 0, move = 0, hp = 0;
        fields >> std::hex >> index >> item_id >> graphic >> character >> ai >> move >> hp;
        if (!fields) return 1;
        state.definitions[index] = {item_id, graphic, character, ai, move, hp};
        state.factory.reset();
        return 0;
    }
    if (command == "marker") {
        std::uint32_t index = 0;
        std::array<std::uint32_t, 7> values{};
        fields >> std::hex >> index;
        for (auto& value : values) fields >> std::hex >> value;
        if (!fields) return 1;
        state.markers[index] = values;
        return 0;
    }
    if (command == "place") {
        std::uint32_t index = 0, object_id = 0, active = 0, field_9c = 0, arg1 = 0, arg2 = 0,
                      team = 0;
        std::string name;
        fields >> std::hex >> index >> object_id >> active >> field_9c >> arg1 >> arg2 >> team
               >> name;
        if (!fields || index >= dfre::kPlacementCount) return 1;
        dfre::ActorPlacement placement;
        placement.object_id = object_id;
        placement.active = active != 0;
        placement.field_9c = static_cast<std::uint16_t>(field_9c);
        placement.argument1 = arg1;
        placement.argument2 = arg2;
        placement.team_code = team;
        placement.name = parse_text_hex(name);
        state.placements[index] = placement;
        return 0;
    }
    if (command == "class") {
        std::uint32_t mode = 0, night = 0, side = 0;
        fields >> std::hex >> mode >> night >> side;
        if (!fields) return 1;
        std::cout << std::hex << dfre::actor_class_item_id(mode, night != 0, side) << '\n';
        return 0;
    }
    if (command == "create") return run_create(state, fields);
    if (command == "bind") return run_bind(state, fields);
    if (command == "spawn") return run_spawn(state, fields);
    if (command == "init") return run_init(state, fields);
    if (command == "dump") {
        std::uint32_t actor = 0;
        fields >> std::hex >> actor;
        if (!fields || !state.factory) return 1;
        const dfre::ActorImage image = dfre::actor_image(state.factory->actor(actor));
        std::cout << to_hex(image.data(), image.size()) << '\n';
        return 0;
    }
    std::cerr << "Unknown command: " << command << '\n';
    return 1;
}

} // namespace

int main() {
    std::cout << std::hex;
    State state;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream fields(line);
        std::string command;
        if (!(fields >> command)) continue;
        if (run(state, fields, command) != 0) {
            std::cerr << "Bad arguments for: " << command << '\n';
            return 1;
        }
    }
    return std::cin.bad() || !std::cout ? 1 : 0;
}
