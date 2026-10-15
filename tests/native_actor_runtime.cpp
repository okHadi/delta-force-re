#include "dfre/actor_runtime.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool require(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

std::string to_hex(const std::array<std::uint8_t, dfre::kActorImageBytes>& image) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (const std::uint8_t byte : image) {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 15]);
    }
    return text;
}

std::uint32_t read32(const dfre::ActorImage& image, std::size_t at) {
    return static_cast<std::uint32_t>(image[at]) |
           (static_cast<std::uint32_t>(image[at + 1]) << 8) |
           (static_cast<std::uint32_t>(image[at + 2]) << 16) |
           (static_cast<std::uint32_t>(image[at + 3]) << 24);
}

std::uint16_t read16(const dfre::ActorImage& image, std::size_t at) {
    return static_cast<std::uint16_t>(image[at] | (static_cast<std::uint16_t>(image[at + 1]) << 8));
}

dfre::ActorDefinitionTable test_definitions(std::uint32_t hp = 4) {
    std::vector<dfre::ActorDefinition> defs;
    defs.push_back({0xFFFFu, 0, 0, 0, 0, 0});                          // index 0 filler
    defs.push_back({0x138Bu, 0x1000u, 0x2000u, 0x3000u, 0x4000u, hp}); // index 1
    defs.push_back({0x138Cu, 0x1100u, 0x2100u, 0x3100u, 0x4100u, 2});  // index 2
    defs.push_back({0x1771u, 0, 0, 0, 0, 0});                          // index 3 marker
    defs.push_back({0x1772u, 0, 0, 0, 0, 0});                          // index 4 marker
    return dfre::ActorDefinitionTable(std::move(defs));
}

const char* kCreateImage =
    "0100000001000000000000000000000200100000000000000000c0ab00000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000030000000000010004000000000000000000000000000000000000000000000000000000000000000000"
    "0000770000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000040000000300000";

const char* kSpawnImage =
    "010000000100000000010000000000020010000000000000000000110000000000000000aa000000bb000000cc000000dd00"
    "0000ee000000ff00000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000030000000500010004000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000040000000300000";

std::string image_hex(const dfre::ActorImage& image) {
    return to_hex(image);
}

} // namespace

int main() {
    // Original class item ids (0x45D520 selector): mode 0x8CF97C, night bit
    // 0x10 of 0x8CF982, second argument.
    const std::uint32_t expected_classes[2][2][2] = {
        {{0x138Bu, 0x138Cu}, {0x13B4u, 0x13B5u}},
        {{0x1392u, 0x1393u}, {0x13B6u, 0x13B7u}},
    };
    for (std::uint32_t mode : {0u, 1u, 3u, 7u}) {
        for (int night = 0; night < 2; ++night) {
            for (int side = 0; side < 2; ++side) {
                const std::uint32_t expected =
                    expected_classes[mode == 2 ? 1 : 0][night][side];
                if (!require(dfre::actor_class_item_id(mode, night != 0, side) == expected,
                             "Class id selection differs from the original.")) return 1;
            }
        }
    }

    // 0x4828C0: first match on +0x20; index 0 when the id is absent.
    {
        const auto defs = test_definitions();
        if (!require(defs.find_index(0x138Cu) == 2, "Definition lookup failed.")) return 1;
        if (!require(defs.find_index(0x138Bu) == 1, "Definition lookup failed.")) return 1;
        if (!require(defs.find_index(0x1234u) == 0, "Missing id must fall back to index 0.")) return 1;
    }

    // 0x45D520 create: full image, fields and health scaling.
    {
        dfre::RuntimeActorFactory factory(test_definitions(), {});
        const auto slot = factory.create(0x77, 0, {0x2000000u, 0xABC00000u});
        if (!require(slot.has_value() && *slot == 0, "First create must use slot 0.")) return 1;
        if (!require(factory.count() == 1, "Create must raise the high-water count.")) return 1;
        const dfre::ActorImage image = dfre::actor_image(factory.actor(0));
        if (!require(image_hex(image) == kCreateImage,
                     "Created actor image differs from the original.")) return 1;
        if (!require(read32(image, 0x00) == 1 && read32(image, 0x04) == 1,
                     "Definition index fields failed.")) return 1;
        if (!require(read32(image, 0x10) == 0x1000 && read32(image, 0x14) == 0,
                     "Graphic and husk fields failed.")) return 1;
        if (!require(read32(image, 0x18) == 0xABC00000u, "Character handle failed.")) return 1;
        if (!require(read32(image, 0x0C) == 0x2000000, "Person handle failed.")) return 1;
        if (!require(read32(image, 0x6C) == 3 && read16(image, 0x72) == 1,
                     "Actor flags failed.")) return 1;
        if (!require(read16(image, 0x74) == 4, "Health failed.")) return 1;
        if (!require(read32(image, 0x98) == 0x77, "Factory argument failed.")) return 1;
        if (!require(read32(image, 0x10C) == 0x4000 && read32(image, 0x110) == 0x3000,
                     "AI/move handles failed.")) return 1;

        factory = dfre::RuntimeActorFactory(test_definitions(0x4001u), {});
        if (!factory.create(0, 0, {}).has_value()) return 1;
        if (!require(read16(dfre::actor_image(factory.actor(0)), 0x74) == 0x4001,
                     "Health without scaling failed.")) return 1;

        dfre::ActorFactoryConfig scaled;
        scaled.scale_health = true;
        factory = dfre::RuntimeActorFactory(test_definitions(0x4001u), scaled);
        if (!factory.create(0, 0, {}).has_value()) return 1;
        if (!require(read16(dfre::actor_image(factory.actor(0)), 0x74) == 4,
                     "Health scaling must wrap to 16 bits.")) return 1;
    }

    // Sequential allocation and the 256-slot limit.
    {
        dfre::RuntimeActorFactory factory(test_definitions(), {});
        for (unsigned step = 0; step < 256; ++step) {
            const auto slot = factory.create(step, 0, {0x2000000u, 0xABC00000u});
            if (!require(slot.has_value() && *slot == step,
                         "Sequential allocation failed.")) return 1;
        }
        if (!require(factory.count() == 256, "High-water count failed.")) return 1;
        if (!require(!factory.create(0, 0, {}).has_value(),
                     "A full factory must return no slot.")) return 1;
    }
    {
        dfre::RuntimeActorFactory factory(dfre::ActorDefinitionTable{}, {});
        if (!require(!factory.create(0, 0, {}).has_value(),
                     "An empty definition table cannot create an actor.")) return 1;
    }

    // 0x43F610 bind: marker pose copy and the rotating counter. The spawn of
    // one placement already binds once (game type 1, two 6002 markers).
    {
        dfre::RuntimeActorFactory factory(test_definitions(), {});
        dfre::MarkerPoseTable markers;
        {
            std::vector<dfre::MarkerImage> records(2);
            const std::uint32_t poses[2][6] = {
                {0x11, 0x12, 0x13, 0x14, 0x15, 0x16},
                {0x21, 0x22, 0x23, 0x24, 0x25, 0x26},
            };
            for (std::size_t i = 0; i < 2; ++i) {
                std::array<std::uint8_t, dfre::kMarkerRecordBytes> record{};
                const std::uint32_t key = 4;  // definition index of 0x1772
                for (std::size_t b = 0; b < 4; ++b) record[b] = static_cast<std::uint8_t>(key >> (8 * b));
                for (std::size_t j = 0; j < 6; ++j) {
                    for (std::size_t b = 0; b < 4; ++b) {
                        record[0x24 + j * 4 + b] = static_cast<std::uint8_t>(poses[i][j] >> (8 * b));
                    }
                }
                records[i] = record;
            }
            markers = dfre::MarkerPoseTable(std::move(records));
        }
        std::vector<dfre::ActorPlacement> placements(dfre::kPlacementCount);
        placements[0].object_id = 0x42;
        placements[0].name = "A";
        placements[0].active = true;
        std::vector<dfre::ActorCreateHandles> handles(dfre::kPlacementCount);
        handles[0] = {0x2000000u, 0x11000000u};
        dfre::ActorBindConfig config;
        config.game_type = 1;
        config.count_6002 = 2;
        std::uint32_t counter = 0;
        const std::vector<dfre::ActorSpawn> spawns =
            dfre::spawn_placement_actors(factory, placements, handles, counter, config, markers, true);
        if (!require(spawns.size() == 1 && spawns[0].actor == 0, "Spawn failed.")) return 1;
        if (!require(placements[0].actor.has_value() && *placements[0].actor == 0,
                     "Placement actor index failed.")) return 1;
        if (!require(counter == 1, "Spawn must advance the bind counter.")) return 1;
        if (!require(read32(dfre::actor_image(factory.actor(0)), 0x24) == 0x11,
                     "Spawn must bind the first 6002 marker.")) return 1;
        const std::uint32_t expected_marker[5] = {0x21, 0x11, 0x21, 0x11, 0};
        const std::uint32_t expected_counter[5] = {2, 3, 4, 5, 5};
        for (int step = 0; step < 4; ++step) {
            dfre::bind_actor_pose(counter, config, markers, factory.definitions(), 0,
                                  factory.actor(0));
            const dfre::ActorImage image = dfre::actor_image(factory.actor(0));
            if (!require(read32(image, 0x24) == expected_marker[step],
                         "Bind rotation failed.")) return 1;
            if (!require(counter == expected_counter[step], "Bind counter failed.")) return 1;
        }
        // Original game types above 5 copy nothing and keep the counter.
        dfre::ActorBindConfig unknown;
        unknown.game_type = 7;
        dfre::bind_actor_pose(counter, unknown, markers, factory.definitions(), 0, factory.actor(0));
        if (!require(counter == 5, "Unknown game type must keep the counter.")) return 1;
    }

    // 0x440640 spawn over the 17 placement slots.
    {
        dfre::RuntimeActorFactory factory(test_definitions(), {});
        std::vector<dfre::MarkerImage> records(1);
        {
            std::array<std::uint8_t, dfre::kMarkerRecordBytes> record{};
            record[0] = 0x03;  // definition index of 0x1771
            const std::uint32_t pose[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
            for (std::size_t j = 0; j < 6; ++j) {
                for (std::size_t b = 0; b < 4; ++b) {
                    record[0x24 + j * 4 + b] = static_cast<std::uint8_t>(pose[j] >> (8 * b));
                }
            }
            records[0] = record;
        }
        const dfre::MarkerPoseTable markers(std::move(records));
        std::vector<dfre::ActorPlacement> placements(dfre::kPlacementCount);
        placements[0] = {0x100, "Alpha", 5, 0, 0, 1, true, {}};
        placements[5] = {0x200, "", 0, 0, 0, 1, true, {}};
        placements[0x10] = {0x300, "Bravo!", 7, 0, 0, 2, true, {}};
        placements[3] = {0x400, "Off", 1, 0, 0, 0, false, {}};
        std::vector<dfre::ActorCreateHandles> handles(dfre::kPlacementCount);
        handles[0] = {0x2000000u, 0x11000000u};
        handles[5] = {0x2000100u, 0x11000001u};
        handles[0x10] = {0x2000200u, 0x11000002u};
        dfre::ActorBindConfig config;
        config.game_type = 0;
        config.count_6001 = 1;
        std::uint32_t counter = 0;
        const std::vector<dfre::ActorSpawn> spawns =
            dfre::spawn_placement_actors(factory, placements, handles, counter, config, markers, true);
        if (!require(spawns.size() == 3, "Spawn must create the three active placements.")) return 1;
        if (!require(factory.count() == 3, "Spawn count failed.")) return 1;
        if (!require(spawns[0].placement == 0 && spawns[0].actor == 0 &&
                     spawns[0].person == 0x2000000 && spawns[0].name == "Alpha",
                     "First spawn result failed.")) return 1;
        if (!require(spawns[1].placement == 5 && spawns[1].name.empty() &&
                     spawns[2].placement == 0x10 && spawns[2].name == "Bravo!",
                     "Later spawn results failed.")) return 1;
        if (!require(!placements[3].actor.has_value(), "Inactive placements stay untouched.")) return 1;
        const dfre::ActorImage first = dfre::actor_image(factory.actor(0));
        if (!require(image_hex(first) == kSpawnImage,
                     "Spawned actor image differs from the original.")) return 1;
        const dfre::ActorImage second = dfre::actor_image(factory.actor(1));
        if (!require(read32(second, 0x08) == 0x200 && read32(second, 0x0C) == 0x2000100 &&
                     read16(second, 0x70) == 0,
                     "Second spawned actor fields failed.")) return 1;
        const dfre::ActorImage third = dfre::actor_image(factory.actor(2));
        if (!require(read32(third, 0x08) == 0x300 && read16(third, 0x70) == 7,
                     "Third spawned actor fields failed.")) return 1;
        if (!require(counter == 0, "Game type 0 must not advance the counter.")) return 1;

        // The pass is a no-op when 0x67F528 is zero.
        dfre::RuntimeActorFactory idle(test_definitions(), {});
        std::vector<dfre::ActorPlacement> more(dfre::kPlacementCount);
        more[0] = {0x100, "Alpha", 5, 0, 0, 1, true, {}};
        std::uint32_t idle_counter = 0;
        const auto none = dfre::spawn_placement_actors(idle, more, handles, idle_counter, config,
                                                       markers, false);
        if (!require(none.empty() && idle.count() == 0, "Disabled finalize must do nothing.")) return 1;
    }

    // 0x4829F0 actor side: local player scan and writes.
    {
        dfre::RuntimeActorFactory factory(test_definitions(), {});
        dfre::MarkerPoseTable markers;
        std::vector<dfre::ActorPlacement> placements(dfre::kPlacementCount);
        placements[0] = {0x100, "slot", 0, 0, 0, 0, true, {}};
        std::vector<dfre::ActorCreateHandles> handles(dfre::kPlacementCount);
        handles[0] = {0x2000000u, 0x11000000u};
        dfre::ActorBindConfig config;
        std::uint32_t counter = 0;
        dfre::spawn_placement_actors(factory, placements, handles, counter, config, markers, true);
        const auto bound = dfre::bind_local_player_actor(factory, 0x100, 0x55, 0x66, "Player");
        if (!require(bound.has_value() && bound->actor == 0, "Local player scan failed.")) return 1;
        if (!require(bound->field_94 == 0x55 && bound->field_98 == 0x66 &&
                     bound->person_name == "Player",
                     "Local player writes failed.")) return 1;
        const dfre::ActorImage image = dfre::actor_image(factory.actor(0));
        if (!require(read32(image, 0x94) == 0x55 && read32(image, 0x98) == 0x66,
                     "Local player image fields failed.")) return 1;
        if (!require(!dfre::bind_local_player_actor(factory, 0x999, 1, 2, "x").has_value(),
                     "Unknown player id must not bind.")) return 1;
    }

    std::cout << "native_actor_runtime: all checks passed\n";
    return 0;
}
