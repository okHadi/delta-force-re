#include "dfre/world_runtime.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool require(bool condition, const char* message) {
    if (!condition) std::cerr << message << '\n';
    return condition;
}

void put_word(std::array<std::uint8_t, 96>& raw, std::size_t at, std::uint32_t value) {
    raw[at] = static_cast<std::uint8_t>(value);
    raw[at + 1] = static_cast<std::uint8_t>(value >> 8);
    raw[at + 2] = static_cast<std::uint8_t>(value >> 16);
    raw[at + 3] = static_cast<std::uint8_t>(value >> 24);
}

dfre::MissionObject make_object(std::uint32_t type, std::uint32_t name_id, std::uint32_t id,
                                std::uint32_t flags, std::int32_t x, std::int32_t y, std::int32_t z,
                                std::int16_t angle0, std::int16_t angle1) {
    dfre::MissionObject object;
    put_word(object.raw, 0, type);
    put_word(object.raw, 4, name_id);
    put_word(object.raw, 8, id);
    put_word(object.raw, 12, flags);
    put_word(object.raw, 16, static_cast<std::uint32_t>(x));
    put_word(object.raw, 20, static_cast<std::uint32_t>(y));
    put_word(object.raw, 24, static_cast<std::uint32_t>(z));
    object.raw[52] = static_cast<std::uint8_t>(angle0);
    object.raw[53] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(angle0) >> 8);
    object.raw[54] = static_cast<std::uint8_t>(angle1);
    object.raw[55] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(angle1) >> 8);
    object.type_id = type;
    object.name_id = name_id;
    object.object_id = id;
    object.flags = flags;
    object.position_raw = {x, y, z};
    object.angle_degrees = {angle0, angle1};
    return object;
}

void set_header_word(dfre::MissionData& mission, std::size_t at, std::uint32_t value) {
    mission.header[at] = static_cast<std::uint8_t>(value);
    mission.header[at + 1] = static_cast<std::uint8_t>(value >> 8);
    mission.header[at + 2] = static_cast<std::uint8_t>(value >> 16);
    mission.header[at + 3] = static_cast<std::uint8_t>(value >> 24);
}

dfre::ItemCatalog test_catalog() {
    const char* text =
        "begin \"null\"\n"
        " id 100000\n"
        " type object\n"
        "end\n"
        "begin \"player01\"\n"
        " id 105003\n"
        " type person\n"
        " graphic player01\n"
        " chr_file player01\n"
        " anim_def playanim\n"
        " attrib: AIData\n"
        " hp 4\n"
        "end\n"
        "begin \"player02\"\n"
        " id 105004\n"
        " type person\n"
        " graphic player02\n"
        " chr_file player01\n"
        " anim_def playanim\n"
        " attrib: AIData\n"
        " hp 2\n"
        "end\n"
        "begin \"start\"\n"
        " id 106001\n"
        " type marker\n"
        " hp 1\n"
        "end\n"
        "begin \"powerup\"\n"
        " id 104001\n"
        " type object\n"
        " attrib: powerup\n"
        " hp 7\n"
        "end\n"
        "begin \"plain\"\n"
        " id 104002\n"
        " type building\n"
        " graphic b1\n"
        " husk h1\n"
        " ambient WIND 12\n"
        " hp 9\n"
        "end\n";
    return dfre::ItemCatalog(text);
}

dfre::MissionData test_mission() {
    dfre::MissionData mission;
    mission.header[0] = 'B';
    mission.header[1] = 'M';
    mission.header[2] = 'S';
    mission.header[3] = 13;
    set_header_word(mission, 0xEC, 1); // 6001 count for game type 0
    auto& group0 = mission.object_groups[0];
    group0.push_back(make_object(4001, 0, 100, 0x40, 0, 0, 1000, 0, 0));   // powerup, accepted
    group0.push_back(make_object(4001, 0, 101, 0x10, 0, 0, 0, 0, 0));      // multiplayer only
    auto& group1 = mission.object_groups[1];
    group1.push_back(make_object(4002, 0, 200, 0x40, 0, 0, 2048, 0, 0));   // building
    group1.push_back(make_object(4002, 0, 201, 0x10, 0, 0, 0, 0, 0));      // multiplayer only
    auto& group2 = mission.object_groups[2];
    group2.push_back(make_object(6001, 0, 300, 0x40, 0, 0, 4096, 0x1111, 0x2222)); // marker
    auto& group3 = mission.object_groups[3];
    group3.push_back(make_object(5003, 0, 400, 0x40, 0, 0, 8192, 0, 0));   // person
    group3.push_back(make_object(5003, 0, 401, 0x80, 0, 0, 0, 0, 0));      // single player rejects
    return mission;
}

dfre::TerrainGeometry test_terrain() {
    dfre::AssetBytes height(1024 * 1024, 4);
    dfre::AssetBytes character(1024 * 1024, 0);
    std::vector<std::uint32_t> detail(65536, 0);
    return dfre::TerrainGeometry(std::move(height), std::move(character), std::move(detail));
}

struct Fixture {
    dfre::MissionData mission = test_mission();
    dfre::ItemCatalog items = test_catalog();
    dfre::TerrainGeometry terrain = test_terrain();
    dfre::MissionRuntimeResources resources;
    dfre::MissionInitConfig config;

    Fixture() {
        dfre::ResourcePlanInput input;
        input.items = &items;
        input.mission = &mission;
        input.weapons = &weapons;
        input.theme = 0;
        input.night = false;
        input.options.color32 = true;
        input.options.powerup_pass = true;
        resources.mission = mission;
        resources.plan = dfre::build_resource_plan(input);
        resources.models = {{"player01.3di", 3}, {"h1.3di", 2}};
        resources.missing_models = {"b1.3di"};
        config.mode = 0;
        config.night = false;
        config.networked = true;
        config.rank = 1;
        config.player_object_id = 0;
        config.player_name = "Tester";
        config.player_face_id = 0x66;
        config.player_side = 0;
        config.weapon_slot_value = 0x55;
    }

private:
    std::vector<dfre::WeaponItemIds> weapons = {{4, 5}};
};

const std::uint32_t kHeight = 4 * 256 * 64; // constant terrain byte 4 -> 65536 fixed point

} // namespace

int main() {
    Fixture fixture;
    const dfre::MissionRuntimeState state =
        dfre::assemble_mission_runtime(fixture.resources, fixture.items, fixture.terrain, nullptr,
                                       fixture.config);

    // Definitions carry the 0x45D6F0 handles: loaded names get ids, missing
    // files stay null. The husk loads even when the graphic file is missing.
    const auto& class_def = state.definitions[1];
    if (!require(class_def.id == 5003 && class_def.model == dfre::kModelHandleBase,
                 "Class graphic handle failed.")) return 1;
    if (!require(class_def.character == dfre::kCharacterHandleBase &&
                 class_def.animation == dfre::kAnimationHandleBase,
                 "Class character/animation handles failed.")) return 1;
    if (!require(state.definitions[5].model == 0 && state.definitions[5].husk_model != 0,
                 "Missing graphic and loaded husk handles failed.")) return 1;

    // Conversion counts and the acceptance filter (group 0 slot 0 is removed
    // by the powerup pass below, so the filter is checked on group 1).
    if (!require(state.groups[1].size() == 2 && state.groups[1][0].active &&
                 !state.groups[1][1].active,
                 "Group 1 acceptance failed.")) return 1;
    if (!require(state.groups[3].size() == 2 && state.groups[3][0].active &&
                 !state.groups[3][1].active,
                 "Group 3 acceptance failed.")) return 1;

    // 0x402EC0 writes: +0x10 model, +0x14 husk (groups 0/1), +0x18 character
    // instance (group 3), +0x74 health low word.
    if (!require(state.groups[1][0].field_10 == 0 && state.groups[1][0].field_14 != 0,
                 "Group 1 resource fields failed.")) return 1;
    if (!require(state.groups[3][0].field_18 == dfre::kCharacterInstanceBase &&
                 state.character_instances.size() == 2,
                 "Group 3 character instance failed.")) return 1;
    if (!require(state.health[1][0] == 9 && state.health[3][0] == 4,
                 "Health words failed.")) return 1;
    if (!require(state.groups[0][0].person_slot.has_value() == false,
                 "Powerup slot needs no person record.")) return 1;

    // Height loops: group 3 adds 0x10000, markers keep the plain height, and
    // the person record copies the adjusted Z.
    if (!require(state.groups[2][0].position[2] == 4096 + kHeight,
                 "Marker height failed.")) return 1;
    if (!require(state.groups[3][0].position[2] == 8192 + kHeight + 0x10000,
                 "Group 3 height failed.")) return 1;
    if (!require(state.people[0].position[2] == 8192 + kHeight + 0x10000,
                 "Person Z copy failed.")) return 1;

    // Powerup removal clears the group 0 record (0x402E70 / 0x45DA30).
    if (!require(state.removals.size() == 1 && state.removals[0].group == 0 &&
                 state.removals[0].slot == 0 && state.removals[0].item_id == 4001,
                 "Powerup removal failed.")) return 1;
    if (!require(state.groups[0][0].definition_index == 0 && !state.groups[0][0].active &&
                 state.health[0][0] == 0,
                 "Removed slot must be clear.")) return 1;

    // Placement slot 0 (0x444C50) and the 0x440640 pass reuse the first free
    // group 3 slot, which is the rejected record at index 1.
    if (!require(state.placements.size() == dfre::kPlacementCount &&
                 state.placements[0].field_9c == 1 && state.placements[0].active,
                 "Placement slot 0 fields failed.")) return 1;
    if (!require(state.spawns.size() == 1 && state.spawns[0].placement == 0 &&
                 state.spawns[0].actor == 1,
                 "Placement actor slot failed.")) return 1;
    if (!require(state.group3_count == 2 && state.placements[0].actor.has_value() &&
                 *state.placements[0].actor == 1,
                 "Group 3 count failed.")) return 1;
    const dfre::RuntimeActor& placed = state.actors->actor(1);
    if (!require(placed.definition_index == 1 && placed.object_id == 0 && placed.flags == 3 &&
                 placed.field_70 == 1 && placed.field_72 == 1 && placed.health == 4,
                 "Placement actor fields failed.")) return 1;
    if (!require(placed.graphic == dfre::kModelHandleBase && placed.character != 0 &&
                 placed.argument == 0x66,
                 "Placement actor handles failed.")) return 1;
    // 0x4011C0 binds the AI/MOVE callback ids before the conversion; the
    // 0x449710 files without callbacks names get the table defaults.
    if (!require(placed.ai_function != 0 && placed.move_function != 0 &&
                 placed.ai_function == state.definitions[placed.definition_index].ai_data &&
                 placed.move_function == state.definitions[placed.definition_index].move_data,
                 "Placement actor callback ids failed.")) return 1;
    if (!require(placed.character == dfre::kCharacterInstanceBase + 1,
                 "Placement character instance failed.")) return 1;
    if (!require(placed.pose[0] == 0 && placed.pose[1] == 0 &&
                 placed.pose[2] == 4096 + kHeight,
                 "Placement pose bind failed.")) return 1;

    // The spawn person record gets the actor handle and the placement name.
    if (!require(state.spawns[0].person == dfre::person_handle(1),
                 "Spawn person handle failed.")) return 1;
    if (!require(state.people[1].owner == dfre::actor_handle(3, 1) &&
                 state.people[1].name == "Tester",
                 "Spawn person record failed.")) return 1;

    // 0x4829F0 writes the weapon slot value and face id on the local actor.
    if (!require(state.local_player.has_value() && state.local_player->actor == 1,
                 "Local player scan failed.")) return 1;
    if (!require(state.local_player->field_94 == 0x55 && state.local_player->field_98 == 0x66 &&
                 state.people[1].name == "Tester",
                 "Local player writes failed.")) return 1;
    const dfre::ActorImage placed_image = dfre::mission_actor_image(state, 3, 1);
    if (!require(placed_image[0x94] == 0x55 && placed_image[0x98] == 0x66,
                 "Local player image words failed.")) return 1;
    const dfre::ActorImage converted = dfre::mission_actor_image(state, 3, 0);
    if (!require(converted[0x74] == 4 && converted[0x18] == 0,
                 "Converted image words failed.")) return 1;
    (void)converted;

    // Live type counts run over the group 3 table after the placement actor.
    if (!require(state.live_types.count(1) == 1,
                 "Live type count failed.")) return 1;

    // Non-networked (0x67F528 == 0): no height loops, no placement actors.
    Fixture idle;
    idle.config.networked = false;
    const dfre::MissionRuntimeState raw = dfre::assemble_mission_runtime(
        idle.resources, idle.items, idle.terrain, nullptr, idle.config);
    if (!require(raw.groups[3][0].position[2] == 8192,
                 "Unnetworked run must keep raw Z.")) return 1;
    if (!require(raw.spawns.empty() && raw.placements[0].actor == std::nullopt &&
                 !raw.local_player.has_value(),
                 "Unnetworked run must skip the placement pass.")) return 1;

    unsigned sound_matches=0;
    const auto with_sound=dfre::assemble_mission_runtime(
        fixture.resources,fixture.items,fixture.terrain,nullptr,fixture.config,
        [&](std::string_view name){if(name=="WIND"){++sound_matches;return 17u;}return 0u;});
    const auto plain=dfre::runtime_item_index(with_sound.definitions,4002);
    if(!require(sound_matches==1&&with_sound.definitions[plain].ambient_handles[0]==17&&
                with_sound.definitions[plain].ambient_sound[0]==12,
                "Mission assembly did not bind its current sound definitions."))return 1;

    std::cout << "native_world_runtime: all checks passed\n";
    return 0;
}
