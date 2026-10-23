// World runtime probe. Modes:
//   audit GAME_FOLDER [--limit N]        one JSON row per mission summary
//   state GAME_FOLDER MISSION [options]  full initialized state as JSON
//   defs GAME_FOLDER MISSION [options]   runtime item definitions, one line each
// Options: --no-names (skip the campaign RTXT), --campaign-text FILE,
//          --weapon-slot N, --face N, --side N, --player-id N, --player-name TEXT,
//          --extra-placement index:object_id:name_hex:field9c:arg1:arg2:team:active

#include "dfre/world_runtime.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string fixed_string(const char* text, std::size_t size) {
    std::size_t length = 0;
    while (length < size && text[length] != '\0') ++length;
    return std::string(text, length);
}

std::string json_string(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out += '"';
    return out;
}

std::string to_hex(const std::string& text) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (const unsigned char c : text) {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 15]);
    }
    return out;
}

std::string campaign_file_for_mission(const std::string& mission) {
    if (mission.size() < 3 || mission[0] != 'C') return {};
    char text[32];
    std::snprintf(text, sizeof(text), "dfcamp%.2s.bin", mission.c_str() + 1);
    return text;
}

struct Options {
    bool names = true;
    std::string campaign_text;
    std::uint32_t weapon_slot = 0;
    std::uint32_t face = 0;
    std::uint32_t side = 0;
    std::uint32_t player_id = 0;
    std::string player_name = "ProbePlayer";
    std::vector<dfre::MissionPlacementInput> extra;
    long limit = -1;
};

bool parse_word(const std::string& text, std::uint32_t& value) {
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 0);
    if (end == text.c_str() || *end != '\0' || parsed > 0xFFFFFFFFul) return false;
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool parse_extra(const std::string& text, dfre::MissionPlacementInput& placement) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, ':')) parts.push_back(part);
    if (parts.size() != 8) return false;
    std::uint32_t index = 0, object = 0, field = 0, arg1 = 0, arg2 = 0, team = 0, active = 0;
    if (!parse_word(parts[0], index) || !parse_word(parts[1], object)) return false;
    if (!parse_word(parts[3], field) || !parse_word(parts[4], arg1) || !parse_word(parts[5], arg2) ||
        !parse_word(parts[6], team) || !parse_word(parts[7], active)) {
        return false;
    }
    placement.index = index;
    placement.object_id = object;
    placement.name.clear();
    for (std::size_t at = 0; at + 1 < parts[2].size(); at += 2) {
        placement.name.push_back(static_cast<char>(std::strtol(parts[2].substr(at, 2).c_str(), nullptr, 16)));
    }
    placement.field_9c = static_cast<std::uint16_t>(field);
    placement.argument1 = arg1;
    placement.argument2 = arg2;
    placement.team_code = team;
    placement.active = active != 0;
    return true;
}

dfre::MissionInitConfig make_config(const std::string& mission, const Options& options,
                                    const dfre::MissionData& data) {
    dfre::MissionInitConfig config;
    // 0x8CF97C is header +0x74 and 0x8CF982 bit 0x10 is header +0x7A bit 0x10
    // once 0x449C70 copied the 0x10C header to 0x8CF908.
    config.mode = dfre::mission_theme(data);
    config.night = dfre::mission_night(data);
    config.game_type = 0;
    config.campaign_text_file = options.campaign_text.empty()
                                    ? campaign_file_for_mission(mission)
                                    : options.campaign_text;
    if (!options.names) config.campaign_text_file.clear();
    config.player_object_id = options.player_id;
    config.player_name = options.player_name;
    config.player_face_id = options.face;
    config.player_side = options.side;
    config.player_team = 0;
    config.weapon_slot_value = options.weapon_slot;
    config.extra_placements = options.extra;
    return config;
}

void print_state(const std::string& mission, const dfre::MissionRuntimeState& state) {
    std::cout << "{\"mission\":" << json_string(mission);
    std::cout << ",\"counts\":[";
    for (std::size_t group = 0; group < 4; ++group) {
        if (group) std::cout << ',';
        std::cout << state.mission.object_groups[group].size();
    }
    std::cout << "],\"converted\":[";
    for (std::size_t group = 0; group < 4; ++group) {
        if (group) std::cout << ',';
        std::cout << state.groups[group].size();
    }
    std::cout << "],\"defs\":[";
    for (std::size_t i = 0; i < state.definitions.size(); ++i) {
        const auto& def = state.definitions[i];
        if (i) std::cout << ',';
        std::cout << "{\"i\":" << i << ",\"id\":" << def.id << ",\"f\":" << unsigned(def.flags)
                  << ",\"t\":" << def.type << ",\"hp\":" << def.hp << ",\"m\":" << def.model
                  << ",\"h\":" << def.husk_model << ",\"c\":" << def.character
                  << ",\"a\":" << def.animation << ",\"ai\":" << def.ai_data
                  << ",\"mo\":" << def.move_data << ",\"gn\":" << json_string(fixed_string(def.graphic, 16))
                  << ",\"hn\":" << json_string(fixed_string(def.husk, 16))
                  << ",\"cn\":" << json_string(fixed_string(def.chr_file, 16))
                  << ",\"an\":" << json_string(fixed_string(def.anim_def, 16)) << '}';
    }
    std::cout << "],\"groups\":[";
    for (std::size_t group = 0; group < 4; ++group) {
        if (group) std::cout << ',';
        std::cout << '[';
        const auto& records = state.groups[group];
        for (std::size_t slot = 0; slot < records.size(); ++slot) {
            const auto& actor = records[slot];
            if (slot) std::cout << ',';
            std::cout << "{\"d\":" << actor.definition_index << ",\"o\":" << actor.object_id
                      << ",\"p\":" << (actor.person_slot ? std::to_string(*actor.person_slot) : "-1")
                      << ",\"f10\":" << actor.field_10 << ",\"f14\":" << actor.field_14
                      << ",\"f18\":" << actor.field_18 << ",\"hp\":"
                      << (slot < state.health[group].size() ? state.health[group][slot] : 0)
                      << ",\"x\":" << actor.position[0] << ",\"y\":" << actor.position[1]
                      << ",\"z\":" << actor.position[2] << ",\"yaw\":" << actor.angle_yaw
                      << ",\"pitch\":" << actor.angle_pitch << ",\"f6c\":" << unsigned(actor.field_6c)
                      << ",\"f70\":" << actor.field_70 << ",\"f72\":" << actor.field_72
                      << ",\"f76\":" << actor.field_76
                      << ",\"f10c\":" << actor.field_10c << ",\"f110\":" << actor.field_110
                      << ",\"a\":" << (actor.active ? 1 : 0) << '}';
        }
        std::cout << ']';
    }
    std::cout << "],\"created\":[";
    if (state.actors) {
        bool first_created = true;
        for (std::size_t slot = 0; slot < state.group3_count; ++slot) {
            if (!dfre::mission_actor_created(state, slot)) continue;
            if (slot >= state.actors->actors().size()) break;
            const auto& actor = state.actors->actor(slot);
            if (!first_created) std::cout << ',';
            first_created = false;
            std::cout << "{\"d\":" << actor.definition_index << ",\"o\":" << actor.object_id
                      << ",\"person\":" << actor.person << ",\"f10\":" << actor.graphic
                      << ",\"f14\":" << actor.husk << ",\"f18\":" << actor.character
                      << ",\"hp\":" << actor.health << ",\"x\":" << actor.pose[0]
                      << ",\"y\":" << actor.pose[1] << ",\"z\":" << actor.pose[2]
                      << ",\"yaw\":" << actor.pose[3] << ",\"pitch\":" << actor.pose[4]
                      << ",\"f6c\":" << actor.flags << ",\"f70\":" << actor.field_70
                      << ",\"f72\":" << actor.field_72 << ",\"f94\":" << actor.weapon_slot
                      << ",\"f98\":" << actor.argument << ",\"f10c\":" << actor.move_function
                      << ",\"f110\":" << actor.ai_function << '}';
        }
    }
    std::cout << "],\"placements\":[";
    for (std::size_t index = 0; index < state.placements.size(); ++index) {
        const auto& placement = state.placements[index];
        if (index) std::cout << ',';
        std::cout << "{\"i\":" << index << ",\"o\":" << placement.object_id
                  << ",\"n\":\"" << to_hex(placement.name) << "\",\"f9c\":" << placement.field_9c
                  << ",\"a1\":" << placement.argument1 << ",\"a2\":" << placement.argument2
                  << ",\"team\":" << placement.team_code << ",\"on\":"
                  << (placement.active ? 1 : 0) << ",\"actor\":"
                  << (placement.actor ? std::to_string(*placement.actor) : "-1") << '}';
    }
    std::cout << "],\"spawns\":[";
    for (std::size_t i = 0; i < state.spawns.size(); ++i) {
        const auto& spawn = state.spawns[i];
        if (i) std::cout << ',';
        std::cout << "{\"p\":" << spawn.placement << ",\"a\":" << spawn.actor
                  << ",\"person\":" << spawn.person << ",\"n\":\"" << to_hex(spawn.name) << "\"}";
    }
    std::cout << "],\"removals\":[";
    for (std::size_t i = 0; i < state.removals.size(); ++i) {
        const auto& removal = state.removals[i];
        if (i) std::cout << ',';
        std::cout << "{\"g\":" << removal.group << ",\"s\":" << removal.slot
                  << ",\"id\":" << removal.item_id << ",\"r\":" << json_string(removal.reason) << '}';
    }
    std::cout << "],\"local\":";
    if (state.local_player) {
        std::cout << "{\"found\":1,\"actor\":" << state.local_player->actor
                  << ",\"f94\":" << state.local_player->field_94
                  << ",\"f98\":" << state.local_player->field_98
                  << ",\"name\":\"" << to_hex(state.local_player->person_name) << "\"}";
    } else {
        std::cout << "{\"found\":0}";
    }
    std::cout << ",\"models\":[";
    for (std::size_t i = 0; i < state.models.size(); ++i) {
        const auto& binding = state.models[i];
        if (i) std::cout << ',';
        std::cout << "{\"n\":" << json_string(binding.name) << ",\"b\":" << unsigned(binding.bank)
                  << ",\"h\":" << binding.handle << '}';
    }
    std::cout << "],\"missing\":[";
    for (std::size_t i = 0; i < state.missing_models.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << json_string(state.missing_models[i]);
    }
    std::cout << "],\"chars\":{";
    bool first = true;
    for (const auto& [name, handle] : state.characters) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << json_string(name) << ':' << handle;
    }
    std::cout << "},\"anims\":{";
    first = true;
    for (const auto& [name, handle] : state.animations) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << json_string(name) << ':' << handle;
    }
    std::cout << "},\"instances\":[";
    for (std::size_t i = 0; i < state.character_instances.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << state.character_instances[i];
    }
    std::cout << "],\"live\":[";
    first = true;
    for (std::size_t slot = 1; slot < dfre::kAiTypeSlotCount; ++slot) {
        const std::uint32_t count = state.live_types.count(slot);
        if (count == 0) continue;
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"slot\":" << slot << ",\"count\":" << count
                  << ",\"prev\":" << state.live_types.previous_count(slot) << '}';
    }
    std::cout << "],\"persons\":[";
    for (std::size_t slot = 0; slot < state.people.size(); ++slot) {
        const auto& person = state.people[slot];
        if (person.owner == 0) continue;
        if (slot) std::cout << ',';
        std::cout << "{\"s\":" << slot << ",\"owner\":" << person.owner
                  << ",\"name\":\"" << to_hex(person.name) << "\",\"z\":" << person.position[2]
                  << ",\"f64\":" << person.field_64 << ",\"f68\":" << person.field_68 << '}';
    }
    std::cout << "],\"counters\":{\"calls\":" << state.resource_calls << ",\"steps\":"
              << state.resource_steps << ",\"nulls\":" << state.null_lookups << ",\"skipped\":"
              << state.skipped_null_slots << ",\"group3\":" << state.group3_count
              << ",\"bind\":" << state.bind_counter << "}}\n";
}

void print_audit_row(const std::string& mission, const dfre::MissionRuntimeState& state) {
    std::size_t persons = 0;
    for (const auto& person : state.people) {
        if (person.owner != 0) ++persons;
    }
    std::size_t active = 0;
    for (std::size_t group = 0; group < 4; ++group) {
        for (const auto& actor : state.groups[group]) {
            if (actor.active) ++active;
        }
    }
    std::cout << "{\"mission\":" << json_string(mission) << ",\"theme\":"
              << dfre::mission_theme(state.mission) << ",\"night\":"
              << (dfre::mission_night(state.mission) ? "true" : "false") << ",\"counts\":[";
    for (std::size_t group = 0; group < 4; ++group) {
        if (group) std::cout << ',';
        std::cout << state.mission.object_groups[group].size();
    }
    std::cout << "],\"active\":" << active << ",\"persons\":" << persons
              << ",\"defs\":" << state.definitions.size() << ",\"models\":" << state.models.size()
              << ",\"missing\":" << state.missing_models.size()
              << ",\"removals\":" << state.removals.size()
              << ",\"placements\":" << [&] {
                  std::size_t count = 0;
                  for (const auto& placement : state.placements) if (placement.active) ++count;
                  return count;
              }()
              << ",\"spawns\":" << state.spawns.size()
              << ",\"local\":" << (state.local_player ? 1 : 0)
              << ",\"group3\":" << state.group3_count << "}\n";
}

int run_audit(const std::string& folder, const Options& options) {
    dfre::AssetStore assets(folder);
    const auto missions = assets.missions();
    std::size_t done = 0;
    for (const auto& mission : missions) {
        if (options.limit >= 0 && static_cast<long>(done) >= options.limit) break;
        dfre::MissionAssetCache cache(assets);
        const auto resources = cache.load_runtime(mission);
        const auto config = make_config(mission, options, resources.mission);
        const auto terrain = dfre::load_terrain_runtime(assets, mission, 32);
        std::optional<dfre::RtxtTable> people;
        if (!config.campaign_text_file.empty() &&
            dfre::mission_asset_present(assets, config.campaign_text_file)) {
            people.emplace(assets.read(config.campaign_text_file).bytes);
        }
        const auto state = dfre::assemble_mission_runtime(resources, cache.items(), terrain.geometry,
                                                          people ? &*people : nullptr, config,{},[&](std::string_view name)->const dfre::Model&{return cache.model(name);});
        print_audit_row(mission, state);
        ++done;
    }
    return 0;
}

int run_state(const std::string& folder, const std::string& mission, const Options& options) {
    dfre::AssetStore assets(folder);
    dfre::MissionAssetCache cache(assets);
    const auto resources = cache.load_runtime(mission);
    const auto config = make_config(mission, options, resources.mission);
    const auto terrain = dfre::load_terrain_runtime(assets, mission, 32);
    std::optional<dfre::RtxtTable> people;
    if (!config.campaign_text_file.empty() &&
        dfre::mission_asset_present(assets, config.campaign_text_file)) {
        people.emplace(assets.read(config.campaign_text_file).bytes);
    }
    const auto state = dfre::assemble_mission_runtime(resources, cache.items(), terrain.geometry,
                                                      people ? &*people : nullptr, config,{},[&](std::string_view name)->const dfre::Model&{return cache.model(name);});
    print_state(mission, state);
    return 0;
}

int run_defs(const std::string& folder, const std::string& mission, const Options& options) {
    dfre::AssetStore assets(folder);
    dfre::MissionAssetCache cache(assets);
    const auto resources = cache.load_runtime(mission);
    const auto config = make_config(mission, options, resources.mission);
    const auto terrain = dfre::load_terrain_runtime(assets, mission, 32);
    const auto state = dfre::assemble_mission_runtime(resources, cache.items(), terrain.geometry,
                                                      nullptr, config,{},[&](std::string_view name)->const dfre::Model&{return cache.model(name);});
    for (std::size_t i = 0; i < state.definitions.size(); ++i) {
        const auto& def = state.definitions[i];
        std::cout << i << ' ' << def.id << ' ' << unsigned(def.flags) << ' ' << def.type << ' '
                  << def.hp << ' ' << def.model << ' ' << def.husk_model << ' ' << def.character
                  << ' ' << def.animation << ' ' << def.ai_data << ' ' << def.move_data << ' '
                  << fixed_string(def.graphic, 16) << ' ' << fixed_string(def.husk, 16) << ' '
                  << fixed_string(def.chr_file, 16) << ' ' << fixed_string(def.anim_def, 16) << '\n';
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            std::cerr << "world_runtime_probe audit GAME_FOLDER [options] | state GAME_FOLDER MISSION "
                         "[options] | defs GAME_FOLDER MISSION [options]\n";
            return 2;
        }
        const std::string mode = argv[1];
        const std::string folder = argv[2];
        std::string mission;
        Options options;
        int index = 3;
        if ((mode == "state" || mode == "defs") && index < argc && argv[index][0] != '-') {
            mission = argv[index++];
        }
        for (; index < argc; ++index) {
            const std::string option = argv[index];
            if (option == "--no-names") options.names = false;
            else if (option == "--campaign-text" && index + 1 < argc) options.campaign_text = argv[++index];
            else if (option == "--player-name" && index + 1 < argc) options.player_name = argv[++index];
            else if (option == "--limit" && index + 1 < argc) options.limit = std::strtol(argv[++index], nullptr, 0);
            else if (option == "--weapon-slot" && index + 1 < argc) {
                if (!parse_word(argv[++index], options.weapon_slot)) throw std::runtime_error("bad weapon slot");
            } else if (option == "--face" && index + 1 < argc) {
                if (!parse_word(argv[++index], options.face)) throw std::runtime_error("bad face");
            } else if (option == "--side" && index + 1 < argc) {
                if (!parse_word(argv[++index], options.side)) throw std::runtime_error("bad side");
            } else if (option == "--player-id" && index + 1 < argc) {
                if (!parse_word(argv[++index], options.player_id)) throw std::runtime_error("bad player id");
            } else if (option == "--extra-placement" && index + 1 < argc) {
                dfre::MissionPlacementInput placement;
                if (!parse_extra(argv[++index], placement)) throw std::runtime_error("bad placement");
                options.extra.push_back(placement);
            } else {
                throw std::runtime_error("Unknown option: " + option);
            }
        }
        if (mode == "audit") return run_audit(folder, options);
        if (mode == "state" && !mission.empty()) return run_state(folder, mission, options);
        if (mode == "defs" && !mission.empty()) return run_defs(folder, mission, options);
        throw std::runtime_error("Unknown mode or missing mission");
    } catch (const std::exception& error) {
        std::cerr << "world_runtime_probe: " << error.what() << '\n';
        return 1;
    }
}
