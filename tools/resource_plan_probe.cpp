#include "dfre/resource_plan.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
const char* stage_name(dfre::ResourceStage stage) {
    switch (stage) {
        case dfre::ResourceStage::soldier: return "soldier";
        case dfre::ResourceStage::weapons: return "weapons";
        case dfre::ResourceStage::effects: return "effects";
        case dfre::ResourceStage::group0: return "group0";
        case dfre::ResourceStage::group1: return "group1";
        case dfre::ResourceStage::group3: return "group3";
        case dfre::ResourceStage::group2: return "group2";
        case dfre::ResourceStage::projectiles: return "projectiles";
    }
    return "?";
}

const char* kind_name(dfre::ResourceKind kind) {
    switch (kind) {
        case dfre::ResourceKind::model: return "model";
        case dfre::ResourceKind::husk: return "husk";
        case dfre::ResourceKind::character: return "character";
        case dfre::ResourceKind::animation: return "animation";
    }
    return "?";
}

void print_plan(const std::string& name, const dfre::MissionData& mission,
                const dfre::ResourcePlan& plan) {
    std::cout << "{\"mission\":\"" << name << "\",\"theme\":" << dfre::mission_theme(mission)
              << ",\"night\":" << (dfre::mission_night(mission) ? "true" : "false") << ",\"calls\":[";
    bool first = true;
    for (const auto& call : plan.calls) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"stage\":\"" << stage_name(call.stage) << "\",\"id\":" << call.item_id
                  << ",\"index\":" << call.item_index << ",\"slot\":";
        if (call.slot == dfre::no_slot) std::cout << "null"; else std::cout << call.slot;
        std::cout << ",\"found\":" << (call.found ? "true" : "false")
                  << ",\"chr\":" << (call.chr_instance ? "true" : "false") << '}';
    }
    std::cout << "],\"steps\":[";
    first = true;
    for (const auto& step : plan.steps) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"stage\":\"" << stage_name(step.stage) << "\",\"kind\":\"" << kind_name(step.kind)
                  << "\",\"id\":" << step.item_id << ",\"index\":" << step.item_index << ",\"slot\":";
        if (step.slot == dfre::no_slot) std::cout << "null"; else std::cout << step.slot;
        std::cout << ",\"name\":\"" << step.name << "\",\"bank\":" << unsigned(step.bank)
                  << ",\"first\":" << (step.first_load ? "true" : "false") << '}';
    }
    std::cout << "],\"removals\":[";
    first = true;
    for (const auto& removal : plan.removals) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"stage\":\"" << stage_name(removal.stage) << "\",\"slot\":" << removal.slot
                  << ",\"id\":" << removal.item_id << ",\"callee\":" << removal.callee
                  << ",\"reason\":\"" << removal.reason << "\"}";
    }
    std::cout << "],\"notes\":[";
    first = true;
    for (const auto& note : plan.notes) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"stage\":\"" << stage_name(note.stage) << "\",\"text\":\"" << note.text << "\"}";
    }
    std::cout << "],\"null_lookups\":" << plan.null_lookups
              << ",\"skipped_nulls\":" << plan.skipped_nulls << '}';
}

void print_stock_weapons() {
    std::cout << '[';
    bool first = true;
    for (const auto& weapon : dfre::original_weapon_items()) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"player\":" << weapon.player_item << ",\"world\":" << weapon.world_item << '}';
    }
    std::cout << "]\n";
}

std::vector<dfre::WeaponItemIds> read_weapons(const std::string& path) {
    std::ifstream source(path);
    if (!source) throw dfre::AssetError("Cannot open weapon list: " + path);
    std::vector<dfre::WeaponItemIds> result;
    dfre::WeaponItemIds weapon;
    while (source >> weapon.player_item >> weapon.world_item) result.push_back(weapon);
    if (result.empty() || result.size() > 4096) throw dfre::AssetError("Invalid weapon list");
    return result;
}

dfre::ResourcePlanOptions parse_options(const std::string& text) {
    dfre::ResourcePlanOptions options;
    std::istringstream source(text);
    std::string item;
    while (std::getline(source, item, ',')) {
        if (item.empty()) continue;
        const auto at = item.find('=');
        if (at == std::string::npos) throw dfre::AssetError("Options use key=0 or key=1");
        const auto key = item.substr(0, at);
        const bool value = item.substr(at + 1) != "0";
        if (key == "color32") options.color32 = value;
        else if (key == "multiplayer") options.multiplayer = value;
        else if (key == "squad") options.squad_pass = value;
        else if (key == "powerup") options.powerup_pass = value;
        else if (key == "patch") options.patch_pass = value;
        else throw dfre::AssetError("Unknown option: " + key);
    }
    return options;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) throw dfre::AssetError(
            "resource_plan_probe GAME_FOLDER (MISSION.BMS|--all|--weapons-only) [--options k=v,..] [--weapons FILE]");
        for (int i = 1; i < argc; ++i)
            if (std::string(argv[i]) == "--weapons-only") { print_stock_weapons(); return 0; }
        if (argc < 3) throw dfre::AssetError("Missing mission or --all");
        const std::string target = argv[2];
        dfre::ResourcePlanInput input;
        std::vector<dfre::WeaponItemIds> weapons;
        const dfre::ItemCatalog items = dfre::load_item_catalog(dfre::AssetStore(argv[1]));
        input.items = &items;
        bool projectiles=false;
        for (int i = 3; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--options" && i + 1 < argc) input.options = parse_options(argv[++i]);
            else if(option=="--projectiles")projectiles=true;
            else if (option == "--weapons" && i + 1 < argc) weapons = read_weapons(argv[++i]);
            else throw dfre::AssetError("Unknown argument: " + option);
        }
        if (!weapons.empty()) input.weapons = &weapons;
        const dfre::AssetStore assets(argv[1]);
        const auto names = target == "--all" ? assets.missions() : std::vector<std::string>{target};
        std::cout << '[';
        bool first = true;
        for (const auto& name : names) {
            const auto mission = dfre::load_mission(assets, name);
            input.mission = &mission;
            auto plan = dfre::build_resource_plan(input);
            if(projectiles)dfre::append_projectile_resource_plan(plan,input);
            if (!first) std::cout << ',';
            first = false;
            print_plan(name, mission, plan);
        }
        std::cout << "]\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
