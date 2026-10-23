#include "dfre/mission.hpp"

#include <algorithm>
#include <cstring>
#include <set>

namespace dfre {
std::string mission_dialogue_archive(const MissionData& mission) {
    const auto first=mission.header.begin()+0x64,last=first+16;
    const auto end=std::find(first,last,0);
    if(end==last)throw AssetError("Unterminated mission dialogue archive");
    return std::string(first,end);
}
namespace {
std::uint32_t word(const AssetBytes& data, std::size_t at) {
    return static_cast<std::uint32_t>(data[at]) |
           (static_cast<std::uint32_t>(data[at + 1]) << 8) |
           (static_cast<std::uint32_t>(data[at + 2]) << 16) |
           (static_cast<std::uint32_t>(data[at + 3]) << 24);
}

std::int32_t signed_word(const AssetBytes& data, std::size_t at) {
    const auto bits = word(data, at);
    std::int32_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::int16_t signed_short(const AssetBytes& data, std::size_t at) {
    const std::uint16_t bits = data[at] | (static_cast<unsigned>(data[at + 1]) << 8);
    std::int16_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

template<std::size_t N> std::array<std::uint32_t, N> words(const AssetBytes& data, std::size_t at) {
    std::array<std::uint32_t, N> result{};
    for (std::size_t i = 0; i < N; ++i) result[i] = word(data, at + i * 4);
    return result;
}
} // namespace

MissionData decode_mission(const AssetBytes& data) {
    if (data.size() < 268 || !std::equal(data.begin(), data.begin() + 4, "BMS\r")) {
        throw AssetError("Expected DF1 BMS v13 header");
    }
    const auto counts = words<4>(data, 152);
    std::uint64_t object_count = 0;
    for (auto count : counts) object_count += count;
    const std::uint64_t events_offset = 268 + object_count * 96 + 17408 + 2048 + 640;
    if (events_offset + 12 > data.size()) throw AssetError("Truncated BMS objects or fixed blocks");
    const auto event_counts = words<3>(data, static_cast<std::size_t>(events_offset));
    if (word(data, 168) != event_counts[0]) throw AssetError("BMS event counts differ");
    const std::uint64_t expected_size = events_offset + 12 + static_cast<std::uint64_t>(event_counts[0]) * 24 +
        (static_cast<std::uint64_t>(event_counts[1]) + event_counts[2]) * 32;
    if (expected_size != data.size()) throw AssetError("BMS event tables do not match the file length");
    MissionData mission;
    std::copy_n(data.begin(), mission.header.size(), mission.header.begin());
    mission.terrain = mission_terrain_name(data);
    std::set<std::uint32_t> ids;
    std::size_t at = 268;
    for (std::size_t group = 0; group < counts.size(); ++group) {
        auto& records = mission.object_groups[group];
        records.reserve(counts[group]);
        for (std::uint32_t i = 0; i < counts[group]; ++i) {
            MissionObject record;
            std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(at), record.raw.size(), record.raw.begin());
            record.type_id = word(data, at);
            record.name_id = word(data, at + 4);
            record.object_id = word(data, at + 8);
            record.flags = word(data, at + 12);
            if (!ids.insert(record.object_id).second) throw AssetError("Duplicate BMS object ID");
            for (std::size_t axis = 0; axis < 3; ++axis) record.position_raw[axis] = signed_word(data, at + 16 + axis * 4);
            record.angle_degrees = {signed_short(data, at + 52), signed_short(data, at + 54)};
            record.byte_67 = data[at + 67];
            record.byte_72 = data[at + 72];
            records.push_back(std::move(record));
            at += 96;
        }
    }
    for (auto& list : mission.marker_lists) {
        list.flags = word(data, at);
        list.count = word(data, at + 4);
        list.marker_indices = words<32>(data, at + 8);
        if (list.count > 32) throw AssetError("BMS marker-list count exceeds 32");
        for (std::size_t i = 0; i < list.count; ++i) {
            if (list.marker_indices[i] >= counts[2]) throw AssetError("BMS marker-list index is outside the marker table");
        }
        at += 136;
    }
    for (auto& group : mission.group_records) { group = words<8>(data, at); at += 32; }
    for (auto& text : mission.text_records) {
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(at), text.size(), text.begin());
        if (std::find(text.begin() + 4, text.end(), 0) == text.end()) throw AssetError("Unterminated BMS text slot");
        at += 20;
    }
    at += 12;
    mission.events.reserve(event_counts[0]);
    for (std::uint32_t i = 0; i < event_counts[0]; ++i) {
        MissionEvent event{words<6>(data, at)};
        if (event.trigger_index() > event_counts[1] || event.action_index() > event_counts[2] ||
            event.trigger_count() > event_counts[1] - event.trigger_index() ||
            event.action_count() > event_counts[2] - event.action_index()) {
            throw AssetError("BMS event table index is outside its table");
        }
        mission.events.push_back(event);
        at += 24;
    }
    mission.triggers.reserve(event_counts[1]);
    for (std::uint32_t i = 0; i < event_counts[1]; ++i) { mission.triggers.push_back(words<8>(data, at)); at += 32; }
    mission.actions.reserve(event_counts[2]);
    for (std::uint32_t i = 0; i < event_counts[2]; ++i) { mission.actions.push_back(words<8>(data, at)); at += 32; }
    return mission;
}

MissionData load_mission(const AssetStore& assets, std::string_view name) {
    auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c + 'a' - 'A') : c; };
    if (name.size() <= 4 || name[name.size() - 4] != '.' || lower(name[name.size() - 3]) != 'b' ||
        lower(name[name.size() - 2]) != 'm' || lower(name[name.size() - 1]) != 's') {
        throw AssetError("Expected a BMS member name");
    }
    return decode_mission(assets.read(name).bytes);
}

} // namespace dfre
