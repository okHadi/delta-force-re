#include "dfre/mission.hpp"

#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

namespace {
std::string json_string(std::string_view text) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setfill('0') << std::setw(4) << unsigned(c);
        else out << c;
    }
    out << '"';
    return out.str();
}

void append(dfre::AssetBytes& bytes, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

dfre::AssetBytes rebuild(const dfre::MissionData& mission) {
    dfre::AssetBytes bytes(mission.header.begin(), mission.header.end());
    for (const auto& group : mission.object_groups) for (const auto& record : group) {
        bytes.insert(bytes.end(), record.raw.begin(), record.raw.end());
    }
    for (const auto& list : mission.marker_lists) {
        append(bytes, list.flags); append(bytes, list.count);
        for (auto value : list.marker_indices) append(bytes, value);
    }
    for (const auto& group : mission.group_records) for (auto value : group) append(bytes, value);
    for (const auto& text : mission.text_records) bytes.insert(bytes.end(), text.begin(), text.end());
    append(bytes, static_cast<std::uint32_t>(mission.events.size()));
    append(bytes, static_cast<std::uint32_t>(mission.triggers.size()));
    append(bytes, static_cast<std::uint32_t>(mission.actions.size()));
    for (const auto& event : mission.events) for (auto value : event.words) append(bytes, value);
    for (const auto& trigger : mission.triggers) for (auto value : trigger) append(bytes, value);
    for (const auto& action : mission.actions) for (auto value : action) append(bytes, value);
    return bytes;
}

void report(const dfre::AssetStore& assets, const std::string& name, bool records) {
    const auto source = assets.read(name);
    const auto mission = dfre::decode_mission(source.bytes);
    if (rebuild(mission) != source.bytes) throw dfre::AssetError("BMS byte preservation failed");
    std::cout << "{\"member\":" << json_string(source.source.member) << ",\"terrain\":" << json_string(mission.terrain)
              << ",\"bytes\":" << source.bytes.size() << ",\"byte_preservation\":true,\"object_counts\":[";
    for (std::size_t group = 0; group < 4; ++group) {
        if (group) std::cout << ',';
        std::cout << mission.object_groups[group].size();
    }
    std::cout << "],\"type_counts\":[";
    for (std::size_t group = 0; group < 4; ++group) {
        if (group) std::cout << ',';
        std::map<std::uint32_t, std::size_t> counts;
        for (const auto& object : mission.object_groups[group]) ++counts[object.type_id];
        std::cout << '{';
        bool first = true;
        for (const auto& pair : counts) {
            if (!first) std::cout << ',';
            first = false;
            std::cout << json_string(std::to_string(pair.first)) << ':' << pair.second;
        }
        std::cout << '}';
    }
    std::size_t marker_lists = 0;
    for (const auto& list : mission.marker_lists) marker_lists += list.count != 0;
    std::cout << "],\"marker_lists_used\":" << marker_lists << ",\"events\":" << mission.events.size()
              << ",\"triggers\":" << mission.triggers.size() << ",\"actions\":" << mission.actions.size();
    if (records) {
        std::cout << ",\"objects\":[";
        bool first = true;
        for (std::size_t group = 0; group < 4; ++group) for (const auto& record : mission.object_groups[group]) {
            if (!first) std::cout << ',';
            first = false;
            std::cout << "{\"group\":" << group << ",\"type_id\":" << record.type_id << ",\"name_id\":" << record.name_id
                      << ",\"object_id\":" << record.object_id << ",\"flags\":" << record.flags << ",\"position_raw\":["
                      << record.position_raw[0] << ',' << record.position_raw[1] << ',' << record.position_raw[2]
                      << "],\"angle_degrees\":[" << record.angle_degrees[0] << ',' << record.angle_degrees[1]
                      << "],\"byte_67\":" << unsigned(record.byte_67) << ",\"byte_72\":" << unsigned(record.byte_72) << '}';
        }
        std::cout << ']';
    }
    std::cout << '}';
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "mission_probe GAME_FOLDER [BMS_MEMBER]\n";
        return 1;
    }
    try {
        const dfre::AssetStore assets(argv[1]);
        const auto names = argc == 3 ? std::vector<std::string>{argv[2]} : assets.missions();
        std::cout << "{\"missions\":[";
        bool first = true;
        for (const auto& name : names) {
            if (!first) std::cout << ',';
            first = false;
            report(assets, name, argc == 3);
        }
        std::cout << "],\"resolved\":" << names.size() << "}\n";
        return std::cout ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "mission: " << error.what() << '\n';
        return 1;
    }
}
