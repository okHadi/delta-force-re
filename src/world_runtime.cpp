#include "dfre/world_runtime.hpp"
#include "dfre/item_callbacks.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace dfre {
namespace {

std::string lower_name(std::string_view value) {
    std::string out(value);
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z') c = char(c + 32);
    }
    return out;
}

std::string filename(std::string_view value, std::string_view suffix) {
    if (value.empty()) return {};
    auto out = lower_name(value);
    if (out.size() < suffix.size() ||
        out.compare(out.size() - suffix.size(), suffix.size(), suffix) != 0) {
        out += suffix;
    }
    return out;
}

std::uint32_t header_word(const MissionData& mission, std::size_t at) {
    const auto& h = mission.header;
    return std::uint32_t(h[at]) | (std::uint32_t(h[at + 1]) << 8) |
           (std::uint32_t(h[at + 2]) << 16) | (std::uint32_t(h[at + 3]) << 24);
}

// 0x8CF9F4/F8/FC and 0x8CFA00 live inside the 0x10C header read by 0x449C70.
std::uint32_t bind_count_6001(const MissionData& mission) { return header_word(mission, 0xEC); }
std::uint32_t bind_count_6003(const MissionData& mission) { return header_word(mission, 0xF0); }
std::uint32_t bind_count_6004(const MissionData& mission) { return header_word(mission, 0xF4); }
std::uint32_t bind_count_6002(const MissionData& mission) { return header_word(mission, 0xF8); }

void check_group_limits(const MissionData& mission) {
    constexpr std::size_t limits[4] = {2048, 1024, 512, 256};
    for (std::size_t group = 0; group < 4; ++group) {
        if (mission.object_groups[group].size() > limits[group]) {
            throw AssetError("BMS group exceeds the original runtime table: group " +
                             std::to_string(group));
        }
    }
}

// 0x45D6F0 handles: the first load per item and kind wins, models by filename.
void fill_definition_handles(MissionRuntimeState& state, const MissionRuntimeResources& resources,
                             const ItemModelResolver& model) {
    std::map<std::string, std::uint32_t> model_by_name;
    for (std::size_t i = 0; i < resources.models.size(); ++i) {
        ModelBinding binding;
        binding.name = resources.models[i].name;
        binding.bank = resources.models[i].bank;
        binding.handle = kModelHandleBase + static_cast<std::uint32_t>(i);
        model_by_name.emplace(lower_name(binding.name), binding.handle);
        state.models.push_back(std::move(binding));
    }
    state.missing_models = resources.missing_models;

    std::uint32_t character_index = 0, animation_index = 0;
    std::vector<std::uint8_t> graphic_done(state.definitions.size(), 0);
    std::vector<std::uint8_t> husk_done(state.definitions.size(), 0);
    for (const auto& step : resources.plan.steps) {
        if (step.item_index >= state.definitions.size()) continue;
        auto& definition = state.definitions[step.item_index];
        switch (step.kind) {
        case ResourceKind::model: {
            // The registry returns the same handle for repeated file loads; a
            // missing file stays null even when another item first loaded it.
            if (graphic_done[step.item_index]) break;
            graphic_done[step.item_index] = 1;
            const auto found = model_by_name.find(lower_name(filename(step.name, ".3di")));
            definition.model = found == model_by_name.end() ? 0 : found->second;
            break;
        }
        case ResourceKind::husk: {
            if (husk_done[step.item_index]) break;
            husk_done[step.item_index] = 1;
            const auto found = model_by_name.find(lower_name(filename(step.name, ".3di")));
            definition.husk_model = found == model_by_name.end() ? 0 : found->second;
            break;
        }
        case ResourceKind::character: {
            const auto key = lower_name(filename(step.name, ".chr"));
            const auto found = state.characters.find(key);
            if (found == state.characters.end()) {
                state.characters.emplace(key, kCharacterHandleBase + character_index++);
            }
            definition.character = state.characters.at(key);
            break;
        }
        case ResourceKind::animation: {
            const auto key = lower_name(filename(step.name, ".anm"));
            const auto found = state.animations.find(key);
            if (found == state.animations.end()) {
                state.animations.emplace(key, kAnimationHandleBase + animation_index++);
            }
            definition.animation = state.animations.at(key);
            break;
        }
        }
    }
    if(model)for(std::size_t item=0;item<state.definitions.size();++item){
        auto& definition=state.definitions[item];
        if(definition.type!=8||!definition.model||!graphic_done[item])continue;
        const auto index=definition.model-kModelHandleBase;
        if(index>=state.models.size())throw AssetError("Effect model handle exceeds loaded resources");
        const auto& loaded=model(state.models[index].name);
        if(loaded.lods.empty())continue;
        const auto& header=loaded.lods.front().raw_header;
        if(header.size()<0x5c)throw AssetError("Effect first LOD header is incomplete");
        const auto upper=[&](std::size_t offset){
            const auto high=std::uint32_t(header[offset+2])|(std::uint32_t(header[offset+3])<<8);
            return (high&0x8000u)?high|0xffff0000u:high;
        };
        const auto height=upper(0x58),width=upper(0x40);
        definition.effect_count=height*width;
    }

}

std::uint32_t make_character_instance(MissionRuntimeState& state, std::size_t item_index) {
    if (state.definitions[item_index].character == 0) return 0;
    const std::uint32_t handle =
        kCharacterInstanceBase + static_cast<std::uint32_t>(state.character_instances.size());
    state.character_instances.push_back(static_cast<std::uint32_t>(item_index));
    return handle;
}

// 0x402EC0 actor field writes for the three converted group tables.
void apply_resource_pass(MissionRuntimeState& state) {
    for (std::size_t group : {std::size_t(0), std::size_t(1), std::size_t(3)}) {
        for (std::size_t slot = 0; slot < state.groups[group].size(); ++slot) {
            AiActorState& actor = state.groups[group][slot];
            if (actor.definition_index == 0) continue;
            const auto& definition = state.definitions[actor.definition_index];
            actor.field_10 = definition.model;
            if (group != 3) {
                actor.field_14 = definition.husk_model;
            } else {
                actor.field_18 = make_character_instance(state, actor.definition_index);
            }
            state.health[group][slot] = static_cast<std::uint16_t>(definition.hp);
        }
    }
}

void clear_actor_slot(MissionRuntimeState& state, std::size_t group, std::size_t slot) {
    if (group >= state.groups.size() || slot >= state.groups[group].size()) return;
    AiActorState& actor = state.groups[group][slot];
    if (actor.person_slot) {
        // 0x45DA30 clears the 0xB4-byte person record; the 0x4604C0 and
        // 0x42CD30 release calls stay with root (no observed behavior here).
        state.people[*actor.person_slot] = AiPersonState{};
    }
    actor = AiActorState{};
    if (slot < state.health[group].size()) state.health[group][slot] = 0;
}

void apply_removals(MissionRuntimeState& state, const ResourcePlan& plan) {
    for (const auto& removal : plan.removals) {
        std::size_t group = 0;
        if (removal.stage == ResourceStage::group0) group = 0;
        else if (removal.stage == ResourceStage::group3) group = 3;
        else continue;
        clear_actor_slot(state, group, removal.slot);
        state.removals.push_back({group, removal.slot, removal.item_id, removal.reason});
    }
}

// 0x403C93-0x403DD4 height loops: groups 3, 0, 1, 2; group 3 adds 0x10000.
void apply_height_loops(MissionRuntimeState& state, const TerrainGeometry& terrain) {
    for (std::size_t group : {std::size_t(3), std::size_t(0), std::size_t(1), std::size_t(2)}) {
        for (AiActorState& actor : state.groups[group]) {
            const auto x = static_cast<std::int32_t>(actor.position[0]);
            const auto y = static_cast<std::int32_t>(actor.position[1]);
            const std::uint32_t height = static_cast<std::uint32_t>(terrain.base_height(x, y));
            const std::uint32_t z = actor.position[2] + height + (group == 3 ? 0x10000u : 0u);
            actor.position[2] = z;
            if (actor.person_slot) {
                state.people[*actor.person_slot].position[2] = z;
            }
        }
    }
}

void setup_placements(MissionRuntimeState& state, const MissionInitConfig& config) {
    state.placements.assign(kPlacementCount, ActorPlacement{});
    ActorPlacement& player = state.placements[0];
    // 0x444C50 writes name, +0x9C = 1 and both factory arguments; the enable
    // byte producer is 0x43FF30, exposed as config input.
    player.object_id = config.player_object_id;
    player.name = config.player_name;
    player.field_9c = 1;
    player.argument1 = config.player_face_id;
    player.argument2 = config.player_side;
    player.team_code = config.player_team;
    player.active = config.local_player_active;
    for (const auto& extra : config.extra_placements) {
        if (extra.index >= kPlacementCount || extra.index == 0) continue;
        ActorPlacement& placement = state.placements[extra.index];
        placement.object_id = extra.object_id;
        placement.name = extra.name;
        placement.field_9c = extra.field_9c;
        placement.argument1 = extra.argument1;
        placement.argument2 = extra.argument2;
        placement.team_code = extra.team_code;
        placement.active = extra.active;
    }
}

std::size_t first_free_person(const std::vector<AiPersonState>& people) {
    for (std::size_t slot = 0; slot < people.size(); ++slot) {
        if (people[slot].owner == 0) return slot;
    }
    return people.size();
}

bool actor_table_has_free_slot(const RuntimeActorFactory& factory) {
    for (const RuntimeActor& actor : factory.actors()) {
        if (actor.definition_index == 0) return true;
    }
    return false;
}

void build_and_run_factory(MissionRuntimeState& state, const MissionInitConfig& config,
                           const ActorBindConfig& bind_config) {
    std::vector<ActorDefinition> entries;
    entries.reserve(state.definitions.size());
    for (const auto& definition : state.definitions) {
        entries.push_back({definition.id, definition.model, definition.character,
                           definition.ai_data, definition.move_data, definition.hp});
    }
    ActorFactoryConfig factory_config;
    factory_config.mode = config.mode;
    factory_config.night = config.night;
    factory_config.scale_health = config.scale_health;
    state.actors.emplace(ActorDefinitionTable(std::move(entries)), factory_config);
    for (std::size_t slot = 0; slot < state.groups[3].size(); ++slot) {
        const AiActorState& source = state.groups[3][slot];
        RuntimeActor& actor = state.actors->actor(slot);
        actor.definition_index = source.definition_index;
        actor.object_id = source.object_id;
        actor.person = source.person_slot ? person_handle(*source.person_slot) : 0;
        actor.graphic = source.field_10;
        actor.husk = source.field_14;
        actor.character = source.field_18;
        actor.pose = {source.position[0], source.position[1], source.position[2],
                      source.angle_yaw, source.angle_pitch, 0};
        actor.flags = source.field_6c;
        actor.field_70 = source.field_70;
        actor.field_72 = source.field_72;
        actor.health = state.health[3][slot];
        actor.move_function = source.field_10c;
        actor.ai_function = source.field_110;
    }

    // Marker table 0x8ACE80 after the height loops.
    std::vector<MarkerImage> markers;
    markers.reserve(state.groups[2].size());
    for (const auto& marker : state.groups[2]) markers.push_back(ai_actor_image(marker));

    if (config.networked) {
        std::vector<ActorCreateHandles> handles(kPlacementCount);
        std::vector<std::size_t> reserved(kPlacementCount, kAiPersonSlotCount);
        for (std::size_t index = 0; index < state.placements.size(); ++index) {
            const ActorPlacement& placement = state.placements[index];
            if (!placement.active) continue;
            if (!actor_table_has_free_slot(*state.actors)) continue; // 0x45D520 fails first
            const std::size_t person_slot = first_free_person(state.people);
            if (person_slot >= state.people.size()) {
                throw AiError("AI person table is full (original writes through a null pointer)");
            }
            state.people[person_slot].owner = 1; // replaced with the actor handle below
            reserved[index] = person_slot;
            const std::uint32_t class_id =
                actor_class_item_id(config.mode, config.night, placement.argument2);
            const std::size_t definition_index = state.actors->definitions().find_index(class_id);
            handles[index] = {person_handle(person_slot),
                              make_character_instance(state, definition_index)};
        }
        std::uint32_t counter = config.bind_counter;
        state.spawns = spawn_placement_actors(*state.actors, state.placements, handles, counter,
                                              bind_config, MarkerPoseTable(std::move(markers)), true);
        state.bind_counter = counter;
        std::vector<std::uint8_t> created(kPlacementCount, 0);
        for (const ActorSpawn& spawn : state.spawns) {
            created[spawn.placement] = 1;
            const std::size_t person_slot =
                (spawn.person - kPersonHandleBase) / static_cast<std::size_t>(kAiPersonRecordBytes);
            if (person_slot >= state.people.size()) continue;
            state.people[person_slot].owner = actor_handle(3, spawn.actor);
            state.people[person_slot].name = spawn.name;
        }
        // A placement whose 0x45D520 call failed must not keep a person slot.
        for (std::size_t index = 0; index < reserved.size(); ++index) {
            if (!created[index] && reserved[index] < state.people.size()) {
                state.people[reserved[index]] = AiPersonState{};
            }
        }
    }
    state.group3_count = std::max(state.groups[3].size(), state.actors->count());
}

void refresh_live_types(MissionRuntimeState& state) {
    std::vector<AiActorImage> group0, group1, group3;
    group0.reserve(state.groups[0].size());
    for (const auto& actor : state.groups[0]) group0.push_back(ai_actor_image(actor));
    group1.reserve(state.groups[1].size());
    for (const auto& actor : state.groups[1]) group1.push_back(ai_actor_image(actor));
    group3.reserve(state.group3_count);
    for (std::size_t slot = 0; slot < state.group3_count; ++slot) {
        group3.push_back(mission_actor_image(state, 3, slot));
    }
    refresh_live_type_counts(group0, group1, group3, state.live_types);
}

void bind_local_player(MissionRuntimeState& state, const MissionInitConfig& config) {
    state.local_player = bind_local_player_actor(*state.actors, config.player_object_id,
                                                 config.weapon_slot_value, config.player_face_id,
                                                 config.player_name);
    if (!state.local_player) return;
    const RuntimeActor& actor = state.actors->actor(state.local_player->actor);
    if (actor.person == 0) return;
    const std::size_t person_slot =
        (actor.person - kPersonHandleBase) / static_cast<std::size_t>(kAiPersonRecordBytes);
    if (person_slot < state.people.size()) {
        state.people[person_slot].name = config.player_name;
    }
}

} // namespace

std::uint32_t person_handle(std::size_t slot) noexcept {
    return kPersonHandleBase + static_cast<std::uint32_t>(slot) * kAiPersonRecordBytes;
}

std::uint32_t actor_handle(std::size_t group, std::size_t slot) noexcept {
    return kActorHandleBase + static_cast<std::uint32_t>(group) * 0x10000u +
           static_cast<std::uint32_t>(slot);
}

std::uint32_t mission_actor_source_address(std::size_t group,std::size_t slot) {
    constexpr std::array<std::uint32_t,4> bases{0x7dde80u,0x867e80u,0x8ace80u,0x7cca80u};
    if(group>=bases.size())throw AssetError("Actor source group is invalid");
    return bases[group]+std::uint32_t(slot)*0x114u;
}

std::string original_campaign_text_file(std::uint32_t campaign_index) {
    char text[32];
    std::snprintf(text, sizeof(text), "dfcamp%02i.bin", int(campaign_index) + 1);
    return text;
}

bool mission_asset_present(const AssetStore& assets, std::string_view name) {
    const std::string key = lower_name(name);
    for (const auto& archive : assets.archives()) {
        for (const auto& entry : archive.entries()) {
            if (!entry.deleted && lower_name(entry.name) == key) return true;
        }
    }
    return false;
}

bool mission_actor_created(const MissionRuntimeState& state, std::size_t slot) {
    for (const ActorSpawn& spawn : state.spawns) {
        if (spawn.actor == slot) return true;
    }
    return false;
}

ActorImage mission_actor_image(const MissionRuntimeState& state, std::size_t group,
                               std::size_t slot) {
    if (group == 3 && (slot >= state.groups[3].size() || mission_actor_created(state, slot))) {
        if (state.actors && slot < state.actors->actors().size()) {
            return actor_image(state.actors->actor(slot));
        }
        return ActorImage{};
    }
    if (group >= state.groups.size() || slot >= state.groups[group].size()) {
        return ActorImage{};
    }
    ActorImage image = ai_actor_image(state.groups[group][slot]);
    const std::uint16_t health =
        slot < state.health[group].size() ? state.health[group][slot] : 0;
    std::memcpy(image.data() + 0x74, &health, sizeof(health));
    return image;
}

MissionRuntimeState assemble_mission_runtime(const MissionRuntimeResources& resources,
                                             const ItemCatalog& items,
                                             const TerrainGeometry& terrain,
                                             const RtxtTable* people_names,
                                             const MissionInitConfig& config) {
    return assemble_mission_runtime(resources,items,terrain,people_names,config,{});
}

MissionRuntimeState assemble_mission_runtime(const MissionRuntimeResources& resources,
                                             const ItemCatalog& items,
                                             const TerrainGeometry& terrain,
                                             const RtxtTable* people_names,
                                             const MissionInitConfig& config,
                                             const ItemSoundResolver& sound) {
    return assemble_mission_runtime(resources,items,terrain,people_names,config,sound,{});
}

MissionRuntimeState assemble_mission_runtime(const MissionRuntimeResources& resources,
                                             const ItemCatalog& items,
                                             const TerrainGeometry& terrain,
                                             const RtxtTable* people_names,
                                             const MissionInitConfig& config,
                                             const ItemSoundResolver& sound,
                                             const ItemModelResolver& model) {
    check_group_limits(resources.mission);
    MissionRuntimeState state;
    state.mission = resources.mission;
    state.definitions = build_runtime_items(items);
    // Original 0x4011C0: bind the AI/MOVE callback ids and ambient lookups
    // before any 0x449710 conversion reads definitions +0x84/+0x90.
    bind_runtime_item_callbacks(state.definitions,sound);
    fill_definition_handles(state, resources, model);
    state.resource_calls = resources.plan.calls.size();
    state.resource_steps = resources.plan.steps.size();
    state.null_lookups = resources.plan.null_lookups;
    state.skipped_null_slots = resources.plan.skipped_nulls;

    AiItemDefinitionTable ai_definitions;
    {
        std::vector<AiItemDefinition> entries;
        entries.reserve(state.definitions.size());
        for (const auto& definition : state.definitions) {
            entries.push_back({definition.id, definition.flags, definition.model, definition.ai_data,
                               definition.move_data, static_cast<std::uint16_t>(definition.hp)});
        }
        ai_definitions = AiItemDefinitionTable(std::move(entries));
    }
    AiMissionConfig ai_config;
    ai_config.multiplayer = config.multiplayer;
    ai_config.rank = config.rank;
    ai_config.animation_counter = config.animation_counter;
    AiActorFactory factory(std::move(ai_definitions), ai_config);
    for (std::size_t group = 0; group < state.groups.size(); ++group) {
        const auto& objects = state.mission.object_groups[group];
        state.groups[group].reserve(objects.size());
        state.health[group].assign(objects.size(), 0);
        for (std::size_t index = 0; index < objects.size(); ++index) {
            std::string name;
            if (people_names && objects[index].name_id != 0) {
                char key[16];
                std::snprintf(key, sizeof(key), "NAMEID%03i",
                              static_cast<int>(objects[index].name_id));
                name = people_names->get("People Names", key);
            }
            state.groups[group].push_back(
                factory.convert(objects[index], actor_handle(group, index), name));
        }
    }
    // One continued 0x794680 pool state; the conversion factory is consumed.
    state.people = factory.people().slots();

    load_type_slots(state.mission.group_records, state.type_slots);
    state.live_types = state.type_slots;

    apply_resource_pass(state);
    apply_removals(state, resources.plan);
    if (config.networked) apply_height_loops(state, terrain);

    setup_placements(state, config);
    ActorBindConfig bind_config;
    bind_config.game_type = config.game_type;
    bind_config.count_6001 = bind_count_6001(state.mission);
    bind_config.count_6002 = bind_count_6002(state.mission);
    bind_config.count_6003 = bind_count_6003(state.mission);
    bind_config.count_6004 = bind_count_6004(state.mission);
    build_and_run_factory(state, config, bind_config);
    if (config.networked) refresh_live_types(state);
    bind_local_player(state, config);
    return state;
}

MissionRuntimeState initialize_mission(const AssetStore& assets, std::string_view mission_file,
                                       const MissionInitConfig& config) {
    MissionAssetCache cache(assets);
    ResourcePlanOptions options;
    options.color32 = config.color32;
    options.multiplayer = config.multiplayer;
    options.squad_pass = config.squad_pass;
    options.powerup_pass = config.powerup_pass;
    options.patch_pass = config.patch_pass;
    const MissionRuntimeResources resources = cache.load_runtime(mission_file, options);
    const TerrainRuntime terrain =
        load_terrain_runtime(assets, mission_file, config.color32 ? 32 : 8);
    std::optional<RtxtTable> people;
    if (!config.campaign_text_file.empty() &&
        mission_asset_present(assets, config.campaign_text_file)) {
        people.emplace(assets.read(config.campaign_text_file).bytes);
    }
    return assemble_mission_runtime(resources, cache.items(), terrain.geometry,
                                    people ? &*people : nullptr, config,{},
                                    [&cache](std::string_view name)->const Model&{return cache.model(name);});
}

} // namespace dfre
