#include "dfre/resource_plan.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

dfre::MissionData mission_with(std::uint32_t theme, bool night) {
    dfre::MissionData mission;
    mission.header[0x74] = std::uint8_t(theme);
    mission.header[0x75] = std::uint8_t(theme >> 8);
    mission.header[0x76] = std::uint8_t(theme >> 16);
    mission.header[0x77] = std::uint8_t(theme >> 24);
    if (night) mission.header[0x7A] = 0x10;
    return mission;
}

void add_object(dfre::MissionData& mission, std::size_t group, std::uint32_t type, std::uint32_t flags) {
    dfre::MissionObject object;
    object.type_id = type;
    object.flags = flags;
    mission.object_groups[group].push_back(object);
}

const char* catalog_text() {
    return "begin \"null\"\nid 100000\ntype marker\nend\n"
        "begin \"M16\"\nid 104002\ntype object\ngraphic m16\nend\n"
        "begin \"PM16\"\nid 104003\ntype object\ngraphic pm16\nend\n"
        "begin \"M4\"\nid 104056\ntype object\ngraphic m4\nend\n"
        "begin \"Barrel\"\nid 101001\ntype decoration\ngraphic barrel\nhusk barrelx\nend\n"
        "begin \"player01\"\nid 105003\ntype person\ngraphic player01\nchr_file player01\nanim_def playanim\nend\n"
        "begin \"player02\"\nid 105004\ntype person\ngraphic fplayr01\nchr_file player01\nanim_def playanim\nend\n"
        "begin \"Squad\"\nid 105002\ntype person\ngraphic player01\nchr_file player01\nanim_def playanim\nend\n"
        "begin \"AMCrate\"\nid 104010\ntype object\ngraphic amcrate\nattrib: movecb powerup\nend\n"
        "begin \"Jeep\"\nid 100200\ntype vehicle\ngraphic jeep\nend\n"
        "begin \"exp_b\"\nid 107001\ntype effect\ngraphic exp_b\nend\n";
}

dfre::ResourcePlanInput make_input(const dfre::ItemCatalog& items, const dfre::MissionData* mission,
                                   const std::vector<dfre::WeaponItemIds>* weapons = nullptr) {
    dfre::ResourcePlanInput input;
    input.items = &items;
    input.mission = mission;
    input.weapons = weapons;
    input.options.squad_pass = true;
    return input;
}

const dfre::ResourceCall& call_at(const dfre::ResourcePlan& plan, std::size_t index) {
    check(index < plan.calls.size(), "Missing plan call");
    return plan.calls[index];
}

std::vector<std::size_t> step_indexes(const dfre::ResourcePlan& plan, dfre::ResourceStage stage,
                                      dfre::ResourceKind kind) {
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < plan.steps.size(); ++i)
        if (plan.steps[i].stage == stage && plan.steps[i].kind == kind) result.push_back(i);
    return result;
}

void check_identity() {
    check(dfre::original_item_type_number("person") == 3 &&
          dfre::original_item_type_number("vehicle") == 1 &&
          dfre::original_item_type_number("building") == 5 &&
          dfre::original_item_type_number("decoration") == 2 &&
          dfre::original_item_type_number("marker") == 4 &&
          dfre::original_item_type_number("object") == 6 &&
          dfre::original_item_type_number("effect") == 8 &&
          dfre::original_item_type_number("PERSON") == 3 &&
          dfre::original_item_type_number("weapon") == 0, "Item type numbers changed");
    check(dfre::original_item_bank(1, true) == 1 && dfre::original_item_bank(2, true) == 2 &&
          dfre::original_item_bank(3, true) == 3 && dfre::original_item_bank(4, true) == 0 &&
          dfre::original_item_bank(5, true) == 4 && dfre::original_item_bank(6, true) == 5 &&
          dfre::original_item_bank(8, true) == 0 && dfre::original_item_bank(0, true) == 0,
          "Loader banks changed");
    for (std::uint32_t type = 0; type < 10; ++type)
        check(dfre::original_item_bank(type, false) == 0, "Eight-bit bank changed");
    const auto& weapons = dfre::original_weapon_items();
    check(weapons.size() == 39 && weapons[0].player_item == 0 && weapons[0].world_item == 0 &&
          weapons[8].player_item == 4003 && weapons[8].world_item == 4002 &&
          weapons[27].player_item == 4073 && weapons[27].world_item == 4074 &&
          weapons[29].player_item == 4020 && weapons[29].world_item == 0 &&
          weapons[37].player_item == 4039 && weapons[37].world_item == 0,
          "Stock weapon table changed");
}

void check_plan() {
    const dfre::ItemCatalog items(catalog_text());
    auto mission = mission_with(0, false);
    add_object(mission, 0, 4002, 0);    // M16
    add_object(mission, 0, 4010, 0);    // ammo crate, powerup
    add_object(mission, 0, 200, 0x90);  // disabled jeep
    add_object(mission, 1, 1001, 0);    // barrel with husk
    add_object(mission, 2, 6001, 0);    // missing marker falls back to null
    add_object(mission, 3, 5003, 0);    // player01
    add_object(mission, 3, 5002, 0);    // squad placeholder
    const std::vector<dfre::WeaponItemIds> weapons = {{4003, 4002}, {0, 4002}};
    const auto plan = dfre::build_resource_plan(make_input(items, &mission, &weapons));

    check(call_at(plan, 0).stage == dfre::ResourceStage::soldier && call_at(plan, 0).item_id == 5003 &&
          call_at(plan, 0).item_index == 5 && call_at(plan, 0).found &&
          call_at(plan, 1).item_id == 5004 && call_at(plan, 1).item_index == 6,
          "Soldier stage order changed");
    check(call_at(plan, 2).stage == dfre::ResourceStage::weapons && call_at(plan, 2).item_id == 4003 &&
          call_at(plan, 3).item_id == 0 &&
          call_at(plan, 3).found && call_at(plan, 3).item_index == 0 &&
          call_at(plan, 4).item_id == 4002 && call_at(plan, 5).item_id == 4002 &&
          !call_at(plan, 6).found &&
          call_at(plan, 6).item_id == 4014 && call_at(plan, 7).item_id == 4040 &&
          call_at(plan, 8).item_id == 4020, "Weapon stage order changed");
    check(call_at(plan, 9).stage == dfre::ResourceStage::effects && call_at(plan, 9).item_index == 0 &&
          call_at(plan, 10).item_id == 7016 && call_at(plan, 14).item_id == 7001 &&
          call_at(plan, 14).item_index == 10 && call_at(plan, 40).item_id == 4075,
          "Effect stage order changed");
    const auto group0 = 41u;
    check(call_at(plan, group0).stage == dfre::ResourceStage::group0 &&
          call_at(plan, group0).item_id == 4002 && call_at(plan, group0).slot == 0 &&
          call_at(plan, group0 + 1).item_id == 4010 && call_at(plan, group0 + 1).slot == 1 &&
          plan.calls[group0 + 2].stage == dfre::ResourceStage::group1 &&
          plan.calls[group0 + 2].item_id == 1001 && plan.calls[group0 + 2].slot == 0,
          "Group 0/1 order changed");
    check(plan.calls[group0 + 3].stage == dfre::ResourceStage::group3 &&
          plan.calls[group0 + 3].item_id == 5003 && plan.calls[group0 + 3].chr_instance &&
          plan.calls[group0 + 4].item_id == 5002 && plan.calls[group0 + 4].chr_instance &&
          plan.calls.size() == group0 + 5, "Group 3 order changed");
    check(plan.steps.size() == 15, "Plan step count changed");
    const auto models = step_indexes(plan, dfre::ResourceStage::soldier, dfre::ResourceKind::model);
    check(models.size() == 2 && plan.steps[0].name == "player01" && plan.steps[0].bank == 3 &&
          plan.steps[0].first_load && plan.steps[1].name == "player01" &&
          plan.steps[1].kind == dfre::ResourceKind::character && plan.steps[1].first_load &&
          plan.steps[2].name == "playanim" && plan.steps[2].kind == dfre::ResourceKind::animation &&
          plan.steps[3].name == "fplayr01" && plan.steps[3].bank == 3 && plan.steps[3].first_load &&
          plan.steps[4].first_load == false && plan.steps[5].first_load == false,
          "Soldier step order changed");
    check(plan.steps[6].name == "pm16" && plan.steps[6].bank == 5 && plan.steps[7].name == "m16" &&
          plan.steps[7].bank == 5, "Weapon step banks changed");
    check(plan.steps[8].name == "exp_b" && plan.steps[8].bank == 0 &&
          plan.steps[8].kind == dfre::ResourceKind::model, "Effect step bank changed");
    check(plan.steps[9].name == "amcrate" && plan.steps[9].bank == 5 &&
          plan.steps[10].name == "barrel" && plan.steps[10].bank == 2 && plan.steps[10].first_load &&
          plan.steps[11].name == "barrelx" && plan.steps[11].kind == dfre::ResourceKind::husk &&
          plan.steps[11].first_load && plan.steps[12].name == "player01" &&
          plan.steps[12].first_load == false && plan.steps[14].name == "playanim",
          "Later stage steps changed");
    check(plan.first_loads().size() == 10, "First load count changed");

    check(plan.null_lookups == 33 && plan.skipped_nulls == 0, "Fallback counts changed");
    check(plan.removals.size() == 2 && plan.removals[0].stage == dfre::ResourceStage::group3 &&
          plan.removals[0].slot == 1 && plan.removals[0].item_id == 5002 &&
          std::string(plan.removals[0].reason) == "squad" && plan.removals[0].callee == 0x45DA30 &&
          plan.removals[1].stage == dfre::ResourceStage::group0 && plan.removals[1].slot == 1 &&
          plan.removals[1].item_id == 4010 && std::string(plan.removals[1].reason) == "powerup",
          "Slot removals changed");
    check(plan.notes.size() == 1 && plan.notes[0].stage == dfre::ResourceStage::group2,
          "Group 2 notes changed");
}

void check_theme_and_bank_modes() {
    const dfre::ItemCatalog items(catalog_text());
    const std::vector<dfre::WeaponItemIds> weapons = {{4003, 4002}};

    auto snow = mission_with(2, false);
    auto plan = dfre::build_resource_plan(make_input(items, &snow, &weapons));
    check(plan.calls[0].item_id == 5010 && plan.calls[1].item_id == 5011, "Snow pair changed");

    auto night = mission_with(0, true);
    plan = dfre::build_resource_plan(make_input(items, &night, &weapons));
    check(plan.calls[0].item_id == 5044 && plan.calls[1].item_id == 5045, "Night pair changed");

    auto snow_night = mission_with(2, true);
    plan = dfre::build_resource_plan(make_input(items, &snow_night, &weapons));
    check(plan.calls[0].item_id == 5046 && plan.calls[1].item_id == 5047, "Snow night pair changed");
    check(dfre::mission_theme(snow_night) == 2 && dfre::mission_night(snow_night),
          "Mission header fields changed");

    auto plain = mission_with(0, false);
    dfre::ResourcePlanInput input = make_input(items, &plain, &weapons);
    input.options.color32 = false;
    plan = dfre::build_resource_plan(input);
    for (const auto& step : plan.steps) check(step.bank == 0, "Eight-bit plan bank changed");
}

void check_explicit_inputs() {
    const dfre::ItemCatalog items(catalog_text());
    dfre::ResourcePlanInput input = make_input(items, nullptr);
    input.theme = 2;
    input.options.squad_pass = false;
    const auto plan = dfre::build_resource_plan(input);
    check(plan.calls.size() == 115 && plan.calls[0].item_id == 5010 && plan.calls[1].item_id == 5011,
          "Direct theme input changed");
    check(plan.removals.empty(), "Removals without a mission changed");
    check(plan.notes.empty(), "Notes without a mission changed");
    check(plan.steps.size() == 4 && plan.steps[0].name == "pm16" && plan.steps[1].name == "m4" &&
          plan.steps[2].name == "m16" && plan.steps[3].name == "exp_b", "Stock table steps changed");
}
} // namespace

int main() {
    try {
        check_identity();
        check_plan();
        check_theme_and_bank_modes();
        check_explicit_inputs();
        std::cout << "resource plan checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
