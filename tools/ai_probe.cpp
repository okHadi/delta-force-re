#include "dfre/ai.hpp"
#include "dfre/assets.hpp"
#include "dfre/items.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string hex(const std::uint8_t* data, std::size_t size) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0xF]);
    }
    return out;
}

bool from_hex(std::string_view text, std::vector<std::uint8_t>& out) {
    if (text.size() % 2 != 0) return false;
    out.clear();
    out.reserve(text.size() / 2);
    const auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int high = nibble(text[i]), low = nibble(text[i + 1]);
        if (high < 0 || low < 0) return false;
        out.push_back(static_cast<std::uint8_t>((high << 4) | low));
    }
    return true;
}

std::uint32_t number(std::string_view text) {
    std::uint32_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') throw std::runtime_error("Invalid decimal token");
        value = value * 10 + static_cast<std::uint32_t>(c - '0');
    }
    return value;
}

std::uint32_t hex_word(std::string_view text) {
    std::uint32_t value = 0;
    for (char c : text) {
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else throw std::runtime_error("Invalid hex token");
        value = value * 16 + static_cast<std::uint32_t>(digit);
    }
    return value;
}

dfre::AiItemDefinitionTable definitions_from_token(const std::string& token) {
    std::vector<dfre::AiItemDefinition> entries;
    std::stringstream groups(token);
    std::string group;
    while (std::getline(groups, group, ';')) {
        if (group.empty()) continue;
        std::stringstream fields(group);
        std::string field;
        std::vector<std::uint32_t> values;
        while (std::getline(fields, field, ',')) values.push_back(number(field));
        if (values.size() != 6) throw std::runtime_error("Definition needs 6 fields");
        dfre::AiItemDefinition entry;
        entry.id = values[0];
        entry.flags24 = static_cast<std::uint8_t>(values[1]);
        entry.field_6c = values[2];
        entry.field_84 = values[3];
        entry.field_90 = values[4];
        entry.field_94 = static_cast<std::uint16_t>(values[5]);
        entries.push_back(entry);
    }
    if (entries.empty()) entries.push_back({});
    return dfre::AiItemDefinitionTable(std::move(entries));
}

// Row: record_hex defs owner multiplayer rank counter name_hex
void case_convert(const std::vector<std::string>& fields) {
    if (fields.size() != 7) throw std::runtime_error("convert needs 7 fields");
    std::vector<std::uint8_t> raw;
    if (!from_hex(fields[0], raw) || raw.size() != 96) throw std::runtime_error("Bad record hex");
    dfre::MissionObject object;
    std::copy(raw.begin(), raw.end(), object.raw.begin());
    dfre::AiMissionConfig config;
    config.multiplayer = fields[3] == "1";
    config.rank = number(fields[4]);
    config.animation_counter = number(fields[5]);
    dfre::AiActorFactory factory(definitions_from_token(fields[1]), config);
    std::string name;
    if (fields[6] != "-") {
        std::vector<std::uint8_t> bytes;
        if (!from_hex(fields[6], bytes)) throw std::runtime_error("Bad name hex");
        name.assign(bytes.begin(), bytes.end());
    }
    const auto actor = factory.convert(object, number(fields[2]), name);
    std::string result = hex(dfre::ai_actor_image(actor).data(), dfre::kAiActorRecordBytes);
    result += ' ';
    if (actor.person_slot) {
        result += hex(dfre::ai_person_image(factory.people().slots()[*actor.person_slot]).data(),
                      dfre::kAiPersonRecordBytes);
    } else {
        result += '-';
    }
    result += ' ';
    result += std::to_string(factory.animation_counter());
    std::cout << result << '\n';
}

// Row: n0 <dword@0x70>... n1 ... n3 ... (decimal groups, group 0, 1, 3 order)
void case_count(const std::vector<std::string>& fields) {
    std::vector<dfre::AiActorImage> groups[3];
    std::size_t at = 0;
    for (auto& group : groups) {
        const auto count = number(fields.at(at++));
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto value = number(fields.at(at++));
            dfre::AiActorImage image{};
            std::memcpy(image.data() + 0x70, &value, 4);
            group.push_back(image);
        }
    }
    dfre::AiTypeSlotTable slots;
    dfre::refresh_live_type_counts(groups[0], groups[1], groups[2], slots);
    std::string out;
    for (std::size_t slot = 0; slot < dfre::kAiTypeSlotCount; ++slot) {
        if (!out.empty()) out += ' ';
        out += std::to_string(slots.count(slot));
        out += ',';
        out += std::to_string(slots.previous_count(slot));
        out += ',';
        out += std::to_string(slots.bytes[slot * dfre::kAiTypeSlotBytes + 0x38]);
        out += ',';
        out += std::to_string(slots.bytes[slot * dfre::kAiTypeSlotBytes + 0x39]);
        out += ',';
        out += std::to_string(slots.bytes[slot * dfre::kAiTypeSlotBytes + 0x3A]);
        out += ',';
        out += std::to_string(slots.bytes[slot * dfre::kAiTypeSlotBytes + 0x3B]);
    }
    std::cout << out << '\n';
}

// Row: n3 <first:byte6C>... n0 <first:byte6C>...
void case_reset(const std::vector<std::string>& fields) {
    std::size_t at = 0;
    std::vector<dfre::AiActorState> groups[2];
    for (auto& group : groups) {
        const auto count = number(fields.at(at++));
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& token = fields.at(at++);
            const auto colon = token.find(':');
            if (colon == std::string::npos) throw std::runtime_error("Bad reset token");
            dfre::AiActorState state;
            state.active = true;
            state.definition_index = hex_word(token.substr(0, colon));
            state.field_6c = static_cast<std::uint8_t>(hex_word(token.substr(colon + 1)));
            group.push_back(state);
        }
    }
    const auto selection = dfre::select_actor_reset_targets(groups[0], groups[1]);
    std::cout << selection.group3.size();
    for (auto index : selection.group3) std::cout << ' ' << index;
    std::cout << ' ' << selection.group0.size();
    for (auto index : selection.group0) std::cout << ' ' << index;
    std::cout << '\n';
}

// Row: 192 hex values (word0, word2, word3 per group record).
void case_slots(const std::vector<std::string>& fields) {
    if (fields.size() != 192) throw std::runtime_error("slots needs 192 values");
    std::array<std::array<std::uint32_t, 8>, 64> records{};
    for (std::size_t k = 0; k < 64; ++k) {
        records[k][0] = hex_word(fields[k * 3]);
        records[k][2] = hex_word(fields[k * 3 + 1]);
        records[k][3] = hex_word(fields[k * 3 + 2]);
    }
    dfre::AiTypeSlotTable table;
    dfre::load_type_slots(records, table);
    std::string out;
    for (std::size_t k = 0; k < 64; ++k) {
        if (!out.empty()) out += ' ';
        out += std::to_string(table.group_word(k, 0x00));
        out += ',';
        out += std::to_string(table.group_word(k, 0x30));
        out += ',';
        out += std::to_string(table.group_word(k, 0x34));
    }
    std::cout << out << '\n';
}

// Row: one decimal int32 value. Uses the staged conversion so the checker can
// compare the exact person +0x70 output against original x86.
void case_percent(const std::vector<std::string>& fields) {
    if (fields.size() != 1) throw std::runtime_error("percent needs one value");
    std::int64_t signed_value = 0;
    std::size_t at = 0;
    bool negative = false;
    if (!fields[0].empty() && fields[0][0] == '-') {
        negative = true;
        at = 1;
    }
    if (at == fields[0].size()) throw std::runtime_error("percent value is empty");
    for (; at < fields[0].size(); ++at) {
        if (fields[0][at] < '0' || fields[0][at] > '9') throw std::runtime_error("percent value is not decimal");
        signed_value = signed_value * 10 + (fields[0][at] - '0');
    }
    if (negative) signed_value = -signed_value;
    const auto value = static_cast<std::int32_t>(signed_value);
    dfre::AiActorFactory factory(dfre::AiItemDefinitionTable({
        dfre::AiItemDefinition{},
        dfre::AiItemDefinition{3012, 4, 0, 0, 0, 0},
    }), {});
    dfre::MissionObject object;
    object.raw.fill(0);
    const std::uint32_t type_id = 3012;
    std::memcpy(object.raw.data() + 0x00, &type_id, 4);
    std::memcpy(object.raw.data() + 0x20, &value, 4);
    const auto actor = factory.convert(object, 1);
    if (!actor.person_slot) throw std::runtime_error("percent case did not create a person");
    std::cout << factory.people().slots()[*actor.person_slot].field_70 << '\n';
}

int run_audit(const std::string& folder) {
    const dfre::AssetStore assets(folder);
    const auto catalog = dfre::load_item_catalog(assets);
    std::vector<dfre::AiItemDefinition> definitions;
    for (const auto& item : catalog.entries()) definitions.push_back({item.id, 0, 0, 0, 0, 0});
    std::cout << '[';
    bool first = true;
    for (const auto& name : assets.missions()) {
        const auto mission = dfre::load_mission(assets, name);
        dfre::AiActorFactory factory(dfre::AiItemDefinitionTable(definitions), {});
        std::array<std::vector<dfre::AiActorState>, 4> groups;
        std::uint32_t specials[3] = {0, 0, 0};
        std::map<std::uint32_t, std::size_t> types;
        for (std::size_t group = 0; group < 4; ++group) {
            for (const auto& object : mission.object_groups[group]) {
                const auto actor = factory.convert(object, 0);
                if (actor.active) {
                    if (object.type_id == 0x1775) ++specials[0];
                    if (object.type_id == 0x177D) ++specials[1];
                    if (object.type_id == 0x1776) ++specials[2];
                    ++types[actor.field_72];
                }
                groups[group].push_back(actor);
            }
        }
        dfre::AiTypeSlotTable slots;
        dfre::load_type_slots(mission.group_records, slots);
        dfre::refresh_live_type_counts(groups[0], groups[1], groups[3], slots);
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"mission\":\"" << name << "\",\"records\":[";
        for (std::size_t group = 0; group < 4; ++group) {
            if (group) std::cout << ',';
            std::cout << groups[group].size();
        }
        std::cout << "],\"active\":[";
        for (std::size_t group = 0; group < 4; ++group) {
            if (group) std::cout << ',';
            std::cout << std::count_if(groups[group].begin(), groups[group].end(),
                                       [](const dfre::AiActorState& a) { return a.active; });
        }
        std::cout << "],\"specials\":[" << specials[0] << ',' << specials[1] << ',' << specials[2]
                  << "],\"live_types\":{";
        bool first_type = true;
        for (std::size_t slot = 1; slot < dfre::kAiTypeSlotCount; ++slot) {
            if (!slots.count(slot)) continue;
            if (!first_type) std::cout << ',';
            first_type = false;
            std::cout << '"' << slot << "\":" << slots.count(slot);
        }
        std::cout << "},\"field72\":{";
        bool first_field = true;
        for (const auto& [type, count] : types) {
            if (!first_field) std::cout << ',';
            first_field = false;
            std::cout << '"' << type << "\":" << count;
        }
        std::cout << "}}";
    }
    std::cout << "]\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "audit") return run_audit(argv[2]);
        if (argc == 2) {
            const std::string mode = argv[1];
            if (mode != "convert" && mode != "count" && mode != "reset" && mode != "slots" &&
                mode != "percent")
                throw std::runtime_error("ai_probe audit GAME_FOLDER | ai_probe MODE < cases.txt");
            std::string line;
            while (std::getline(std::cin, line)) {
                if (line.empty()) continue;
                std::vector<std::string> fields;
                std::stringstream stream(line);
                std::string field;
                while (stream >> field) fields.push_back(field);
                if (fields.empty()) continue;
                if (mode == "convert") case_convert(fields);
                else if (mode == "count") case_count(fields);
                else if (mode == "reset") case_reset(fields);
                else if (mode == "percent") case_percent(fields);
                else case_slots(fields);
            }
            return 0;
        }
        throw std::runtime_error("ai_probe audit GAME_FOLDER | ai_probe MODE < cases.txt");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
