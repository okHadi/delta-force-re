#pragma once

#include "dfre/assets.hpp"

#include <array>

namespace dfre {

struct MissionObject {
    std::array<std::uint8_t, 96> raw{};
    std::uint32_t type_id = 0;
    std::uint32_t name_id = 0;
    std::uint32_t object_id = 0;
    std::uint32_t flags = 0;
    std::array<std::int32_t, 3> position_raw{};
    std::array<std::int16_t, 2> angle_degrees{};
    std::uint8_t byte_67 = 0;
    std::uint8_t byte_72 = 0;
};

struct MissionMarkerList {
    std::uint32_t flags = 0;
    std::uint32_t count = 0;
    std::array<std::uint32_t, 32> marker_indices{};
};

struct MissionEvent {
    std::array<std::uint32_t, 6> words{};
    std::uint32_t trigger_index() const noexcept { return words[1]; }
    std::uint32_t action_index() const noexcept { return words[2]; }
    std::uint8_t trigger_count() const noexcept { return static_cast<std::uint8_t>(words[5] >> 8); }
    std::uint8_t action_count() const noexcept { return static_cast<std::uint8_t>(words[5] >> 16); }
};

struct MissionData {
    std::array<std::uint8_t, 268> header{};
    std::string terrain;
    std::array<std::vector<MissionObject>, 4> object_groups;
    std::array<MissionMarkerList, 128> marker_lists{};
    std::array<std::array<std::uint32_t, 8>, 64> group_records{};
    std::array<std::array<std::uint8_t, 20>, 32> text_records{};
    std::vector<MissionEvent> events;
    std::vector<std::array<std::uint32_t, 8>> triggers;
    std::vector<std::array<std::uint32_t, 8>> actions;
};

MissionData decode_mission(const AssetBytes& data);
MissionData load_mission(const AssetStore& assets, std::string_view name);

} // namespace dfre
