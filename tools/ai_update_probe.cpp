#include "dfre/ai_update.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> fields;
    std::stringstream stream(text);
    std::string field;
    while (std::getline(stream, field, separator)) fields.push_back(field);
    return fields;
}

std::int64_t signed_number(const std::string& text) {
    std::size_t at = 0;
    bool negative = false;
    if (!text.empty() && text[0] == '-') {
        negative = true;
        at = 1;
    }
    if (at == text.size()) throw std::runtime_error("empty number");
    std::int64_t value = 0;
    for (; at < text.size(); ++at) {
        if (text[at] < '0' || text[at] > '9') throw std::runtime_error("bad number");
        value = value * 10 + (text[at] - '0');
    }
    return negative ? -value : value;
}

std::uint32_t number(const std::string& text) {
    const auto value = signed_number(text);
    if (value < 0) throw std::runtime_error("expected unsigned number");
    return static_cast<std::uint32_t>(value);
}

dfre::AiItemDefinitionTable parse_defs(const std::string& token) {
    std::vector<dfre::AiItemDefinition> entries;
    for (const auto& group : split(token, ';')) {
        if (group.empty()) continue;
        const auto fields = split(group, ':');
        if (fields.size() != 3) throw std::runtime_error("definition needs id:field28:field9c");
        dfre::AiItemDefinition entry;
        entry.id = number(fields[0]);
        entry.field_28 = number(fields[1]);
        entry.field_9c = number(fields[2]);
        entries.push_back(entry);
    }
    if (entries.empty()) entries.push_back({});
    return dfre::AiItemDefinitionTable(std::move(entries));
}

std::map<std::uint32_t, dfre::AiActorBounds> parse_bounds(const std::string& token) {
    std::map<std::uint32_t, dfre::AiActorBounds> table;
    if (token == "-") return table;
    for (const auto& group : split(token, ';')) {
        if (group.empty()) continue;
        const auto fields = split(group, ':');
        if (fields.size() != 3) throw std::runtime_error("bounds need handle:radius:height");
        dfre::AiActorBounds bounds;
        bounds.radius_64 = static_cast<std::int32_t>(signed_number(fields[1]));
        bounds.height_60 = static_cast<std::int32_t>(signed_number(fields[2]));
        table[number(fields[0])] = bounds;
    }
    return table;
}

dfre::AiBoundsSource make_bounds(const std::map<std::uint32_t, dfre::AiActorBounds>& table) {
    return [&table](std::uint32_t handle) {
        const auto found = table.find(handle);
        if (found == table.end()) throw dfre::AiError("no bounds for object handle");
        return found->second;
    };
}

// rebuild record: def:byte6c:handle:x:y:z
dfre::AiActorState parse_rebuild_record(const std::string& token) {
    const auto fields = split(token, ':');
    if (fields.size() != 6) throw std::runtime_error("rebuild record needs 6 fields");
    dfre::AiActorState actor;
    actor.active = true;
    actor.definition_index = number(fields[0]);
    actor.field_6c = static_cast<std::uint8_t>(number(fields[1]));
    actor.field_10 = number(fields[2]);
    for (std::size_t axis = 0; axis < 3; ++axis)
        actor.position[axis] = static_cast<std::uint32_t>(signed_number(fields[3 + axis]));
    return actor;
}

// tick record: def:fc:field110:field10c:field18:field78
dfre::AiActorState parse_tick_record(const std::string& token) {
    const auto fields = split(token, ':');
    if (fields.size() != 6) throw std::runtime_error("tick record needs 6 fields");
    dfre::AiActorState actor;
    actor.active = true;
    actor.definition_index = number(fields[0]);
    actor.field_fc = number(fields[1]);
    actor.field_110 = number(fields[2]);
    actor.field_10c = number(fields[3]);
    actor.field_18 = number(fields[4]);
    actor.field_78 = number(fields[5]);
    return actor;
}

template<typename Reader>
std::vector<dfre::AiActorState> parse_group(const std::vector<std::string>& fields, std::size_t& at, Reader reader) {
    const auto count = number(fields.at(at++));
    std::vector<dfre::AiActorState> group;
    for (std::uint32_t i = 0; i < count; ++i) group.push_back(reader(fields.at(at++)));
    return group;
}

std::string join_list(const std::vector<dfre::AiActorRef>& list) {
    if (list.empty()) return "-";
    std::string out;
    for (const auto& ref : list) {
        if (!out.empty()) out += ';';
        out += std::to_string(ref.group);
        out += ',';
        out += std::to_string(ref.index);
    }
    return out;
}

std::string join_indices(const std::vector<std::size_t>& indices) {
    if (indices.empty()) return "-";
    std::string out;
    for (auto index : indices) {
        if (!out.empty()) out += ',';
        out += std::to_string(index);
    }
    return out;
}

void case_rebuild(const std::vector<std::string>& fields) {
    if (fields.size() < 4) throw std::runtime_error("rebuild needs defs bounds counts ...");
    const auto definitions = parse_defs(fields[0]);
    const auto bounds_table = parse_bounds(fields[1]);
    dfre::AiActorTables tables;
    std::size_t at = 2;
    tables.groups[1] = parse_group(fields, at, parse_rebuild_record);
    tables.groups[0] = parse_group(fields, at, parse_rebuild_record);
    tables.groups[3] = parse_group(fields, at, parse_rebuild_record);
    if (at != fields.size()) throw std::runtime_error("rebuild row has trailing fields");
    dfre::AiUpdateState state;
    dfre::rebuild_target_lists(tables, state, definitions, make_bounds(bounds_table));
    std::vector<std::size_t> selected3, selected0, starts, counts;
    const auto collect = [&](std::uint8_t group, std::vector<std::size_t>& selected) {
        for (std::size_t index = 0; index < tables.groups[group].size(); ++index) {
            const auto& actor = tables.groups[group][index];
            if (actor.definition_index != 0 && (actor.field_6c & 2) == 0) {
                selected.push_back(index);
                starts.push_back(actor.field_104);
                counts.push_back(actor.field_108);
            }
        }
    };
    collect(3, selected3);
    collect(0, selected0);
    std::cout << "list=" << join_list(state.target_list)
              << " sel3=" << join_indices(selected3)
              << " sel0=" << join_indices(selected0)
              << " f104=" << join_indices(starts)
              << " f108=" << join_indices(counts) << '\n';
}

void case_tick(const std::vector<std::string>& fields) {
    if (fields.size() < 11) throw std::runtime_error("tick needs defs, five groups, and state");
    const auto definitions = parse_defs(fields[0]);
    dfre::AiActorTables tables;
    std::size_t at = 1;
    tables.groups[0] = parse_group(fields, at, parse_tick_record);
    tables.groups[1] = parse_group(fields, at, parse_tick_record);
    tables.groups[2] = parse_group(fields, at, parse_tick_record);
    tables.groups[3] = parse_group(fields, at, parse_tick_record);
    tables.extra = parse_group(fields, at, parse_tick_record);
    dfre::AiUpdateState state;
    const auto counter_in = number(fields.at(at++));
    state.call_counter = counter_in;
    state.paused = number(fields.at(at++)) != 0;
    state.demo = number(fields.at(at++)) != 0;
    const auto local = static_cast<std::int64_t>(signed_number(fields.at(at++)));
    if (local >= 0) state.local_actor = static_cast<std::size_t>(local);
    const auto tick = number(fields.at(at++));
    if (at != fields.size()) throw std::runtime_error("tick row has trailing fields");
    const bool rebuild_expected = counter_in >= 0x40;
    const auto bounds = [](std::uint32_t) -> dfre::AiActorBounds {
        throw dfre::AiError("bounds not expected in tick cases");
    };
    const auto report = dfre::tick_ai(tables, state, definitions, bounds, tick);
    std::vector<std::size_t> starts, counts;
    const auto collect = [&](std::uint8_t group) {
        for (std::size_t index = 0; index < tables.groups[group].size(); ++index) {
            const auto& actor = tables.groups[group][index];
            if (actor.definition_index != 0 && (actor.field_6c & 2) == 0) {
                starts.push_back(actor.field_104);
                counts.push_back(actor.field_108);
            }
        }
    };
    collect(3);
    collect(0);
    const auto call_name = [](dfre::AiTickCall::Kind kind) -> const char* {
        switch (kind) {
            case dfre::AiTickCall::Kind::per_actor_update: return "u";
            case dfre::AiTickCall::Kind::local_player_control: return "l";
            case dfre::AiTickCall::Kind::fixed_0045e050: return "a";
            case dfre::AiTickCall::Kind::fixed_00464eb0: return "b";
            case dfre::AiTickCall::Kind::fixed_00465bd0: return "c";
        }
        return "?";
    };
    std::string calls;
    for (const auto& call : report.calls) {
        if (!calls.empty()) calls += ';';
        calls += call_name(call.kind);
        if (call.kind == dfre::AiTickCall::Kind::per_actor_update) {
            calls += ',';
            calls += std::to_string(call.actor.group);
            calls += ',';
            calls += std::to_string(call.actor.index);
        }
    }
    std::string actions;
    for (const auto& action : report.actions) {
        if (!actions.empty()) actions += ';';
        actions += action.kind == dfre::AiActorActionKind::update ? "u" : "s";
        actions += ',';
        actions += std::to_string(action.actor.group);
        actions += ',';
        actions += std::to_string(action.actor.index);
        actions += ',';
        actions += std::to_string(action.target);
    }
    std::string marks;
    for (auto mark : report.profiler_marks) {
        if (!marks.empty()) marks += ';';
        marks += std::to_string(mark);
    }
    std::cout << "calls=" << (calls.empty() ? "-" : calls)
              << " actions=" << (actions.empty() ? "-" : actions)
              << " marks=" << (marks.empty() ? "-" : marks)
              << " counter=" << state.call_counter
              << " rebuilt=" << (rebuild_expected ? 1 : 0)
              << " list=" << join_list(state.target_list)
              << " f104=" << join_indices(starts)
              << " f108=" << join_indices(counts)
              << " t3=";
    for (std::size_t index = 0; index < tables.groups[3].size(); ++index) {
        if (index) std::cout << ';';
        std::cout << tables.groups[3][index].field_fc << ',' << tables.groups[3][index].field_78;
    }
    if (tables.groups[3].empty()) std::cout << '-';
    std::cout << " t0=";
    for (std::size_t index = 0; index < tables.groups[0].size(); ++index) {
        if (index) std::cout << ';';
        std::cout << tables.groups[0][index].field_fc;
    }
    if (tables.groups[0].empty()) std::cout << '-';
    std::cout << " t1=";
    for (std::size_t index = 0; index < tables.groups[1].size(); ++index) {
        if (index) std::cout << ';';
        std::cout << tables.groups[1][index].field_fc;
    }
    if (tables.groups[1].empty()) std::cout << '-';
    std::cout << " t2=";
    for (std::size_t index = 0; index < tables.groups[2].size(); ++index) {
        if (index) std::cout << ';';
        std::cout << tables.groups[2][index].field_fc;
    }
    if (tables.groups[2].empty()) std::cout << '-';
    std::cout << " xt=";
    for (std::size_t index = 0; index < tables.extra.size(); ++index) {
        if (index) std::cout << ';';
        std::cout << tables.extra[index].field_fc << ','
                  << (tables.extra[index].definition_index == 0 ? 1 : 0);
    }
    if (tables.extra.empty()) std::cout << '-';
    std::cout << '\n';
}

// Row: defs bounds prefill actor g1_count <recs...> g0_count <recs...>
// Calls rebuild_actor_targets for one group 3 actor with the shared list
// pre-seeded with `prefill` entries.
void case_acquire(const std::vector<std::string>& fields) {
    if (fields.size() < 6) throw std::runtime_error("acquire needs defs bounds prefill actor groups");
    const auto definitions = parse_defs(fields[0]);
    const auto bounds_table = parse_bounds(fields[1]);
    const auto prefill = number(fields[2]);
    dfre::AiActorTables tables;
    std::size_t at = 4;
    tables.groups[1] = parse_group(fields, at, parse_rebuild_record);
    tables.groups[0] = parse_group(fields, at, parse_rebuild_record);
    tables.groups[3].push_back(parse_rebuild_record(fields[3]));
    if (at != fields.size()) throw std::runtime_error("acquire row has trailing fields");
    dfre::AiUpdateState state;
    for (std::uint32_t i = 0; i < prefill; ++i) state.target_list.push_back({255, i});
    const auto before = state.target_list.size();
    dfre::rebuild_actor_targets(tables, {3, 0}, state, definitions, make_bounds(bounds_table));
    std::string appended;
    for (std::size_t i = before; i < state.target_list.size(); ++i) {
        if (!appended.empty()) appended += ';';
        appended += std::to_string(state.target_list[i].group);
        appended += ',';
        appended += std::to_string(state.target_list[i].index);
    }
    std::cout << "count=" << state.target_list.size()
              << " f108=" << tables.groups[3][0].field_108
              << " f104=" << tables.groups[3][0].field_104
              << " appended=" << (appended.empty() ? "-" : appended) << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("ai_update_probe rebuild|acquire|tick < cases.txt");
        const std::string mode = argv[1];
        if (mode != "rebuild" && mode != "acquire" && mode != "tick")
            throw std::runtime_error("ai_update_probe rebuild|acquire|tick < cases.txt");
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            std::vector<std::string> fields;
            std::stringstream stream(line);
            std::string field;
            while (stream >> field) fields.push_back(field);
            if (fields.empty()) continue;
            if (mode == "rebuild") case_rebuild(fields);
            else if (mode == "acquire") case_acquire(fields);
            else case_tick(fields);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
