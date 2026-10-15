#include "dfre/mission.hpp"

#include <algorithm>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void put(dfre::AssetBytes& data, std::size_t at, std::uint32_t value) {
    for (unsigned byte = 0; byte < 4; ++byte) data.at(at + byte) = static_cast<std::uint8_t>(value >> (byte * 8));
}

template<class Function> void rejects(Function function) {
    try { function(); }
    catch (const dfre::AssetError&) { return; }
    throw std::runtime_error("Malformed BMS accepted");
}

dfre::AssetBytes fixture() {
    dfre::AssetBytes data(268 + 192 + 20096 + 12 + 24 + 32 + 32, 0);
    std::copy_n("BMS\r", 4, data.begin());
    std::copy_n("TEST.TRN", 8, data.begin() + 84);
    put(data, 152, 1); put(data, 160, 1); put(data, 168, 1);
    put(data, 268, 2013); put(data, 268 + 4, 7); put(data, 268 + 8, 10); put(data, 268 + 12, 0x1040);
    put(data, 268 + 16, 0xffffff00u); put(data, 268 + 20, 512); put(data, 268 + 24, 768);
    put(data, 268 + 52, 0xfff1005a); data[268 + 67] = 2; data[268 + 72] = 3;
    put(data, 268 + 80, 0xdeadbeefu);
    put(data, 364, 6005); put(data, 364 + 8, 11);
    const std::size_t fixed = 460;
    put(data, fixed, 3); put(data, fixed + 4, 1); put(data, fixed + 8, 0);
    put(data, fixed + 17408, 42);
    put(data, fixed + 17408 + 2048, 7);
    std::copy_n("ROUTE", 5, data.begin() + fixed + 17408 + 2048 + 4);
    const auto events = fixed + 20096;
    put(data, events, 1); put(data, events + 4, 1); put(data, events + 8, 1);
    put(data, events + 12, 1); put(data, events + 24, 123); put(data, events + 28, 456);
    put(data, events + 32, 0x00010100u);
    put(data, events + 12 + 24 + 4, 1);
    put(data, events + 12 + 24 + 32 + 4, 12);
    return data;
}
} // namespace

int main() {
    try {
        const auto data = fixture();
        const auto mission = dfre::decode_mission(data);
        require(mission.terrain == "TEST.TRN", "Terrain filename failed");
        require(mission.object_groups[0].size() == 1 && mission.object_groups[2].size() == 1, "Object groups failed");
        const auto& object = mission.object_groups[0][0];
        require(object.type_id == 2013 && object.object_id == 10 && object.name_id == 7 && object.flags == 0x1040,
                "Object identity failed");
        require(object.position_raw == std::array<std::int32_t, 3>({-256, 512, 768}), "Signed position failed");
        require(object.angle_degrees == std::array<std::int16_t, 2>({90, -15}), "Signed angle failed");
        require(object.byte_67 == 2 && object.byte_72 == 3, "Raw byte fields failed");
        require(object.raw[80] == 0xef && object.raw[83] == 0xde, "Unknown object bytes lost");
        require(mission.marker_lists[0].flags == 3 && mission.marker_lists[0].count == 1 &&
                mission.marker_lists[0].marker_indices[0] == 0, "Marker-list data failed");
        require(mission.group_records[0][0] == 42 && mission.text_records[0][4] == 'R', "Fixed records lost");
        require(mission.events.size() == 1 && mission.events[0].trigger_count() == 1 &&
                mission.events[0].action_count() == 1 && mission.actions[0][1] == 12, "Event tables failed");
        for (const auto& mutation : std::vector<std::pair<std::size_t, std::uint32_t>>{
                 {0, 0}, {152, 0xffffffffu}, {168, 2}, {364 + 8, 10}, {460 + 4, 33}, {460 + 8, 1},
                 {20556, 0xffffffffu}, {20556 + 12 + 4, 2}, {20556 + 12 + 8, 2},
                 {20556 + 12 + 20, 0x00010200u}, {20556 + 12 + 20, 0x00020100u}}) {
            auto bad = data; put(bad, mutation.first, mutation.second);
            rejects([&] { dfre::decode_mission(bad); });
        }
        for (auto size : {std::size_t(0), std::size_t(267), std::size_t(459), data.size() - 1}) {
            auto bad = data; bad.resize(size);
            rejects([&] { dfre::decode_mission(bad); });
        }
        auto bad = data; bad.push_back(0);
        rejects([&] { dfre::decode_mission(bad); });
        bad = data; std::fill_n(bad.begin() + 460 + 17408 + 2048 + 4, 16, 'X');
        rejects([&] { dfre::decode_mission(bad); });
        bad = data; std::fill_n(bad.begin() + 84, 16, 'X');
        rejects([&] { dfre::decode_mission(bad); });
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
