// Mission tick probe: one case per line, one result line per case.
// Case formats are documented in docs/mission-tick.md. The original-side
// checker (tools/check_mission_tick_original.py) writes the same case data
// into Df.exe memory and compares the result lines.
#include "dfre/mission_tick.hpp"
#include "dfre/models.hpp"

#include <cstdint>
#include <cstring>
#include <memory>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> fields;
    std::stringstream stream(text);
    std::string field;
    while (std::getline(stream, field, separator)) fields.push_back(field);
    return fields;
}

std::int64_t signed_number(const std::string& text) {
    std::size_t at = 0;
    bool negative = false;
    if (!text.empty() && text[0] == '-') {
        negative = true;
        at = 1;
    }
    if (at == text.size()) throw std::runtime_error("empty number");
    std::int64_t value = 0;
    for (; at < text.size(); ++at) {
        if (text[at] < '0' || text[at] > '9') throw std::runtime_error("bad number");
        value = value * 10 + (text[at] - '0');
    }
    return negative ? -value : value;
}

std::uint32_t number(const std::string& text) {
    const auto value = signed_number(text);
    if (value < 0) throw std::runtime_error("expected unsigned number: " + text);
    return static_cast<std::uint32_t>(value);
}

// ---------------------------------------------------------------------------
// Synthetic prepared CHR: two bones, slots 0 and 5, one shared root table
// entry so the pose list is stable across frame ticks (see native_animation).
// ---------------------------------------------------------------------------
using Bytes = std::vector<std::uint8_t>;
void put(Bytes& bytes, std::size_t at, std::uint32_t value) {
    bytes.at(at) = std::uint8_t(value);
    bytes.at(at + 1) = std::uint8_t(value >> 8);
    bytes.at(at + 2) = std::uint8_t(value >> 16);
    bytes.at(at + 3) = std::uint8_t(value >> 24);
}

Bytes synthetic_character() {
    const std::size_t bones = 120, map = 304, table = 312, anim_a = 1332, a0 = 1352, a1 = 1424,
                      anim_b = 1504, b0 = 1524, b1 = 1596, total = 1668;
    Bytes bytes(total, 0);
    put(bytes, 0, 0x31524843);
    put(bytes, 4, std::uint32_t(bones));
    put(bytes, 8, 2);
    put(bytes, 12, std::uint32_t(map));
    put(bytes, 16, 2);
    put(bytes, 20, std::uint32_t(table));
    put(bytes, 24, 2);
    for (std::size_t index = 0; index < 2; ++index) {
        put(bytes, bones + index * 92, 0);
        put(bytes, bones + index * 92 + 16, 0);
        put(bytes, bones + index * 92 + 20, 0);
        put(bytes, bones + index * 92 + 24, 1);
        put(bytes, map + index * 4, std::uint32_t(index));
        put(bytes, bones + index * 92 + 56, 0xFFFFFFFF);
    }
    put(bytes, table, std::uint32_t(anim_a));
    put(bytes, table + 5 * 4, std::uint32_t(anim_b));
    auto animation = [&](std::size_t position) {
        put(bytes, position, 2);
        put(bytes, position + 4, 2);
        put(bytes, position + 8, 1);  // one root bone
        put(bytes, position + 12, 0);
    };
    animation(anim_a);
    animation(anim_b);
    auto frame = [&](std::size_t position, std::uint32_t stride, std::uint32_t flags,
                     std::uint16_t duration, std::size_t root_table) {
        put(bytes, position, stride);
        put(bytes, position + 4, flags);
        bytes.at(position + 8) = std::uint8_t(duration);
        bytes.at(position + 9) = std::uint8_t(duration >> 8);
        bytes.at(position + 10) = 0;
        bytes.at(position + 11) = 0;
        put(bytes, position + 12, 0);
        put(bytes, position + 16, 0);
        put(bytes, position + 20, 0);
        bytes.at(position + 0x18) = 0;
        bytes.at(position + 0x1b) = 0;
        put(bytes, position + 0x1c, 0);
        put(bytes, position + 0x20, std::uint32_t(root_table));
        put(bytes, position + 0x24, 0);
        put(bytes, position + 0x28, 0);
    };
    // Both slot-0 frames share the same root table: the pose list is stable.
    frame(a0, 72, 0, 100, a0 + 60);
    put(bytes, a0 + 60, 1);
    put(bytes, a0 + 64, 2);
    put(bytes, a0 + 68, 3);
    frame(a1, 0, 0x20, 100, a0 + 60);  // loops back to frame 0
    frame(b0, 72, 0, 100, b0 + 60);
    put(bytes, b0 + 60, 1);
    put(bytes, b0 + 64, 2);
    put(bytes, b0 + 68, 3);
    frame(b1, 0, 0, 100, b0 + 60);
    return bytes;
}

struct ControllerBox {
    dfre::CharacterModel model;
    dfre::AnimationController controller;
    explicit ControllerBox() : model(dfre::prepare_character_runtime(
                                   dfre::decode_character(synthetic_character()))),
                               controller(model) {}
};

dfre::AnimationDefinition item_definition(const std::string& name, std::int32_t speed,
                                          bool override_flag) {
    dfre::AnimationDefinition definition;
    dfre::AnimationAction action;
    action.name = name;
    action.number = 0;
    action.velocity = 0.0;
    action.velocity_per_tick = speed;
    action.override_flag = override_flag;
    definition.actions.push_back(action);
    return definition;
}

// A uniform height map: base_height = byte * 16384, slope = (0, 0).
dfre::TerrainGeometry uniform_terrain(std::uint8_t height_byte) {
    dfre::AssetBytes height(1024 * 1024, height_byte);
    dfre::AssetBytes character(1024 * 1024, 0);
    std::vector<std::uint32_t> detail(256 * 256, 0);
    return dfre::TerrainGeometry(std::move(height), std::move(character), std::move(detail));
}

struct TraceSummary {
    std::string text;
    void add(std::uint32_t kind, std::uint32_t address, std::int32_t arg1) {
        if (!text.empty()) text += ";";
        text += std::to_string(kind) + ":" + std::to_string(address) + ":" +
                std::to_string(arg1);
    }
};

std::string actor_state_line(dfre::MissionTickState& state, const dfre::AiActorRef& ref,
                             const dfre::MissionActorRuntime& runtime,
                             const TraceSummary& summary) {
    const dfre::AiActorState& actor = *state.actor_at(ref);
    std::ostringstream out;
    out << "px=" << static_cast<std::int32_t>(actor.position[0])
        << " py=" << static_cast<std::int32_t>(actor.position[1])
        << " pz=" << static_cast<std::int32_t>(actor.position[2])
        << " yaw=" << actor.angle_yaw << " pitch=" << actor.angle_pitch
        << " vertical=" << runtime.vertical_44
        << " flags=" << state.extra_for(ref).field_6d << ":" << unsigned(runtime.byte_6e)
        << ":" << unsigned(actor.field_6c)
        << " cache=" << runtime.step_cache.x << ":" << runtime.step_cache.y << ":"
        << runtime.step_cache.z << " ground100=" << runtime.ground_100
        << " f7c=" << actor.field_7c << " f80=" << (runtime.driver ? std::uint32_t(runtime.driver->current_action()) : 0u) << " fc=" << actor.field_fc
        << " f108=" << actor.field_108 << " roll=" << runtime.roll_38
        << " trace=" << (summary.text.empty() ? "-" : summary.text);
    return out.str();
}

// ---------------------------------------------------------------------------
// actor mode: one 0x487370 join case.
// ---------------------------------------------------------------------------
int run_actor(const std::string& line, std::ostream& out) {
    const auto fields = split(line, ' ');
    std::size_t at = 0;
    const auto next = [&]() { return fields.at(at++); };
    const std::uint8_t slot = static_cast<std::uint8_t>(number(next()));
    const std::uint32_t tick = number(next());
    const bool local = number(next()) != 0;
    dfre::MissionTickConfig config;
    config.local_actor = local ? std::optional<std::size_t>(slot) : std::nullopt;
    const std::int32_t px = static_cast<std::int32_t>(signed_number(next()));
    const std::int32_t py = static_cast<std::int32_t>(signed_number(next()));
    const std::int32_t pz = static_cast<std::int32_t>(signed_number(next()));
    const std::uint32_t yaw = number(next());
    const std::uint32_t pitch = number(next());
    const std::uint32_t roll = number(next());
    const std::int32_t vertical = static_cast<std::int32_t>(signed_number(next()));
    const std::uint8_t flag6c = static_cast<std::uint8_t>(number(next()));
    const std::uint8_t flag6d = static_cast<std::uint8_t>(number(next()));
    const std::uint8_t flag6e = static_cast<std::uint8_t>(number(next()));
    const std::uint32_t f108 = number(next());
    const std::uint32_t fc = number(next());
    const std::uint32_t f78 = number(next());
    const std::uint32_t f110 = number(next());
    const std::uint32_t item_direction = number(next());
    const std::int32_t item_speed = static_cast<std::int32_t>(signed_number(next()));
    const bool item_override = number(next()) != 0;
    const std::uint8_t height_byte = static_cast<std::uint8_t>(number(next()));
    const std::int32_t water = static_cast<std::int32_t>(signed_number(next()));
    const std::uint32_t ground_handle = number(next());
    const std::int32_t ground_rot = static_cast<std::int32_t>(signed_number(next()));
    const std::int32_t ground_ox = static_cast<std::int32_t>(signed_number(next()));
    const std::int32_t ground_oy = static_cast<std::int32_t>(signed_number(next()));
    const std::int32_t ground_px = static_cast<std::int32_t>(signed_number(next()));
    const std::int32_t ground_py = static_cast<std::int32_t>(signed_number(next()));
    const std::int32_t ground_pz = static_cast<std::int32_t>(signed_number(next()));
    const std::uint32_t link_90 = number(next());
    const std::uint32_t target_count = number(next());
    std::vector<dfre::AiActorRef> targets;
    for (std::uint32_t index = 0; index < target_count; ++index) {
        const auto group = static_cast<std::uint8_t>(number(next()));
        const auto slot_index = number(next());
        targets.push_back({group, slot_index});
    }
    const std::uint32_t collision_result = number(next());
    const std::uint32_t mask = number(next());

    ControllerBox box;
    auto storage=std::make_unique<dfre::MissionTickState>();
    auto& state=*storage;
    state.config = config;
    state.tables.groups[3].resize(slot + 1);
    state.tables.groups[0].resize(4);
    state.tables.groups[1].resize(4);
    dfre::AiItemDefinition def;
    def.id = 9;
    def.field_28 = 0;
    state.definitions = dfre::AiItemDefinitionTable({def,dfre::AiItemDefinition{1}});
    dfre::AiActorState& actor = state.tables.groups[3][slot];
    actor.definition_index = 0;
    actor.position[0] = static_cast<std::uint32_t>(px);
    actor.position[1] = static_cast<std::uint32_t>(py);
    actor.position[2] = static_cast<std::uint32_t>(pz);
    actor.angle_yaw = yaw;
    actor.angle_pitch = pitch;
    actor.field_6c = flag6c;
    actor.field_78 = f78;
    actor.field_fc = fc;
    actor.field_110 = f110;
    actor.field_108 = f108;
    for (std::size_t index = 0; index < state.tables.groups[0].size(); ++index)
        state.tables.groups[0][index].definition_index = 1;
    for (std::size_t index = 0; index < state.tables.groups[1].size(); ++index)
        state.tables.groups[1][index].definition_index = 1;
    state.ai.target_list = targets;
    for (const auto& target : targets) {
        if (dfre::AiActorState* candidate = state.actor_at(target))
            candidate->definition_index = 1;
    }
    state.world.trigger_held_47a290=[](std::uint32_t){return false;};
    state.world.weapon_effect=[](auto&,const auto&){return true;};
    state.world.external_stage=[](auto& current,const auto& call)->std::optional<std::uint32_t>{
        if(call.address==0x482b00)dfre::mission_weapon_part(current,false);return 0;
    };
    dfre::init_mission_tick(state, config, std::move(state.tables), std::move(state.definitions),
                            {}, 0x12345678);
    state.ai.target_list=targets;
    // init re-sizes extras; rebuild the actor field before binding.
    dfre::AiActorRef ref{3, slot};
    state.bind_animation(ref, box.controller,
                         dfre::build_animation_driver_actions(
                             item_definition("A", item_speed, item_override),
                             {dfre::AnimationCompiledAction{"A", 0,
                                                            static_cast<std::uint8_t>(item_direction),
                                                            0}}));
    dfre::MissionActorRuntime& runtime = state.runtime_for(ref);
    runtime.roll_38 = roll;
    runtime.vertical_44 = vertical;
    runtime.ground_100 = ground_handle;
    runtime.link_90 = link_90;
    state.extra_for(ref).field_6d = flag6d;
    runtime.byte_6e = flag6e;

    dfre::TerrainGeometry terrain = uniform_terrain(height_byte);
    state.world.ground_entity=[](const auto&,const auto&)->std::optional<dfre::GroundEntity>{return std::nullopt;};
    state.world.terrain = &terrain;
    state.world.water_level = water;
    state.world.frame_mask = [mask](const dfre::AiActorRef&) { return mask; };
    state.world.steady = []() { return true; };
    std::uint32_t link_written = 0;
    std::array<std::int32_t, 3> link_position{};
    state.world.linked_position =
        [&](std::uint32_t handle, const std::array<std::int32_t, 3>& position) {
            link_written = handle;
            link_position = position;
        };
    state.world.target_collision = [collision_result](const dfre::AiActorRef&,
                                                      const dfre::AiActorRef&) {
        return collision_result;
    };
    if (ground_handle != 0) {
        state.world.ground_object = [=](std::uint32_t handle) -> std::optional<dfre::GroundObject> {
            if (handle != ground_handle) return std::nullopt;
            dfre::GroundObject object;
            object.position = {ground_px, ground_py, ground_pz};
            object.rotation = static_cast<std::uint32_t>(ground_rot);
            object.offset_x = ground_ox;
            object.offset_y = ground_oy;
            return object;
        };
    }

    dfre::MissionTickReport report;
    state.world.trigger_held_47a290=[](std::uint32_t){return false;};
    state.world.weapon_effect=[](auto&,const auto&){return true;};
    state.world.external_stage=[](auto& current,const auto& call)->std::optional<std::uint32_t>{
        if(call.address==0x482b00)dfre::mission_weapon_part(current,false);return 0;
    };
    dfre::mission_actor_update(state, ref, tick, report);
    TraceSummary summary;
    for (const dfre::MissionTraceEntry& entry : report.trace) {
        if (entry.kind == dfre::MissionTraceKind::effect)
            summary.add(1, entry.address, static_cast<std::int32_t>(entry.args[0]));
        else if (entry.kind == dfre::MissionTraceKind::unresolved)
            summary.add(2, entry.address, static_cast<std::int32_t>(entry.args[0]));
        else if (entry.kind == dfre::MissionTraceKind::stage)
            summary.add(0, entry.address, static_cast<std::int32_t>(entry.args[0]));
    }
    // The pose list the checker must write into the original instance.
    const auto& poses = box.controller.root_positions();
    std::ostringstream pose_text;
    pose_text << poses.size();
    for (const auto& pose : poses)
        pose_text << ":" << pose[0] << ":" << pose[1] << ":" << pose[2];
    out << actor_state_line(state, ref, runtime, summary) << " link=" << link_written << ":"
        << link_position[0] << ":" << link_position[1] << ":" << link_position[2]
        << " poses=" << pose_text.str() << "\n";
    return 0;
}

// ---------------------------------------------------------------------------
// tick mode: one mission_tick frame.
// ---------------------------------------------------------------------------
struct RecordSpec {
    std::uint32_t definition = 0;
    std::uint8_t byte6c = 0;
    std::uint32_t f18 = 0;
    std::uint32_t f78 = 0;
    std::uint32_t fc = 0;
    std::uint32_t f110 = 0;
    std::uint32_t f10c = 0;
    std::int32_t px = 0, py = 0, pz = 0;
    std::uint32_t handle = 0;
};

RecordSpec parse_record(const std::string& token) {
    const auto fields = split(token, ':');
    if (fields.size() != 11) throw std::runtime_error("record needs 11 fields");
    RecordSpec record;
    record.definition = number(fields[0]);
    record.byte6c = static_cast<std::uint8_t>(number(fields[1]));
    record.f18 = number(fields[2]);
    record.f78 = number(fields[3]);
    record.fc = number(fields[4]);
    record.f110 = number(fields[5]);
    record.f10c = number(fields[6]);
    record.px = static_cast<std::int32_t>(signed_number(fields[7]));
    record.py = static_cast<std::int32_t>(signed_number(fields[8]));
    record.pz = static_cast<std::int32_t>(signed_number(fields[9]));
    record.handle = number(fields[10]);
    return record;
}

int run_tick(const std::string& line, std::ostream& out) {
    const auto fields = split(line, ' ');
    std::size_t at = 0;
    const auto next = [&]() { return fields.at(at++); };
    const auto defs_token = next();
    std::vector<dfre::AiItemDefinition> definitions;
    for (const auto& group : split(defs_token, ';')) {
        const auto parts = split(group, ':');
        if (parts.size() != 5) throw std::runtime_error("definition needs id:f28:f9c:f84:f90");
        dfre::AiItemDefinition entry;
        entry.id = number(parts[0]);
        entry.field_28 = number(parts[1]);
        entry.field_9c = number(parts[2]);
        entry.field_84 = number(parts[3]);
        entry.field_90 = number(parts[4]);
        definitions.push_back(entry);
    }
    if (definitions.empty()) definitions.push_back({});
    dfre::AiActorTables tables;
    for (int group = 0; group < 4; ++group) {
        const std::uint32_t count = number(next());
        tables.groups[group].resize(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            const RecordSpec record = parse_record(next());
            dfre::AiActorState& actor = tables.groups[group][index];
            actor.active = true;
            actor.definition_index = record.definition;
            actor.field_6c = record.byte6c;
            actor.field_18 = record.f18;
            actor.field_78 = record.f78;
            actor.field_fc = record.fc;
            actor.field_110 = record.f110;
            actor.field_10c = record.f10c;
            actor.field_10 = record.handle;
            actor.position[0] = static_cast<std::uint32_t>(record.px);
            actor.position[1] = static_cast<std::uint32_t>(record.py);
            actor.position[2] = static_cast<std::uint32_t>(record.pz);
        }
    }
    const std::uint32_t extra_count = number(next());
    tables.extra.resize(extra_count);
    for (std::uint32_t index = 0; index < extra_count; ++index) {
        const RecordSpec record = parse_record(next());
        dfre::AiActorState& actor = tables.extra[index];
        actor.active = true;
        actor.definition_index = record.definition;
        actor.field_6c = record.byte6c;
        actor.field_fc = record.fc;
        actor.field_10c = record.f10c;
        actor.field_110 = record.f110;
    }
    const std::uint32_t counter = number(next());
    const bool paused = number(next()) != 0;
    const bool demo = number(next()) != 0;
    const bool local_set = number(next()) != 0;
    const std::uint32_t local = number(next());
    const std::uint32_t tick = number(next());
    const std::uint8_t height_byte = static_cast<std::uint8_t>(number(next()));
    const std::int32_t water = static_cast<std::int32_t>(signed_number(next()));
    const std::uint32_t mask = number(next());
    const std::uint32_t collision_result = number(next());
    const std::uint32_t scripted_call = number(next());

    dfre::MissionTickConfig config;
    config.paused = paused;
    config.demo = demo;
    if (local_set) config.local_actor = local;
    auto storage=std::make_unique<dfre::MissionTickState>();
    auto& state=*storage;
    // Bind a synthetic controller for every group-3 actor before init copies
    // the tables; runtimes survive init.
    std::vector<std::unique_ptr<ControllerBox>> boxes;
    for (std::size_t index = 0; index < tables.groups[3].size(); ++index)
        boxes.emplace_back(new ControllerBox());
    const dfre::AiActorTables tables_keep = tables;  // for the report below
    state.world.trigger_held_47a290=[](std::uint32_t){return false;};
    state.world.weapon_effect=[](auto&,const auto&){return true;};
    state.world.external_stage=[](auto& current,const auto& call)->std::optional<std::uint32_t>{
        if(call.address==0x482b00)dfre::mission_weapon_part(current,false);return 0;
    };
    dfre::init_mission_tick(state, config, std::move(tables),
                            dfre::AiItemDefinitionTable(definitions), {}, 0x12345678);
    for (std::size_t index = 0; index < state.tables.groups[3].size(); ++index) {
        const dfre::AiActorRef ref{3, index};
        state.bind_animation(ref, boxes[index]->controller,
                             dfre::build_animation_driver_actions(
                                 item_definition("A", 0, false),
                                 {dfre::AnimationCompiledAction{"A", 0, 0, 0}}));
    }

    dfre::TerrainGeometry terrain = uniform_terrain(height_byte);
    state.world.ground_entity=[](const auto&,const auto&)->std::optional<dfre::GroundEntity>{return std::nullopt;};
    state.world.terrain = &terrain;
    state.world.water_level = water;
    state.world.frame_mask = [mask](const dfre::AiActorRef&) { return mask; };
    state.world.steady = []() { return true; };
    state.world.bounds = [](std::uint32_t) { return dfre::AiActorBounds{100, 100}; };
    state.world.target_collision = [collision_result](const dfre::AiActorRef&,
                                                      const dfre::AiActorRef&) {
        return collision_result;
    };
    state.tick_67dbf4 = tick;
    state.ai.call_counter = counter;

    dfre::MissionTickInputs inputs;
    state.world.trigger_held_47a290=[](std::uint32_t){return false;};
    state.world.weapon_effect=[](auto&,const auto&){return true;};
    state.world.external_stage=[](auto& current,const auto& call)->std::optional<std::uint32_t>{
        if(call.address==0x482b00)dfre::mission_weapon_part(current,false);return 0;
    };
    dfre::MissionTickReport report = dfre::mission_tick(state, inputs);

    std::ostringstream calls;
    std::ostringstream actions;
    // Derive the compact call/action tokens from the trace.
    for (const auto& entry : report.trace) {
        if (entry.kind == dfre::MissionTraceKind::stage && entry.address == 0x487370) {
            if (!calls.str().empty()) calls << ",";
            calls << "u," << unsigned(entry.group) << "," << entry.index;
        } else if (entry.kind == dfre::MissionTraceKind::stage && entry.address == 0x488200) {
            if (!calls.str().empty()) calls << ",";
            calls << "l";
        } else if (entry.kind == dfre::MissionTraceKind::external_stage &&
                   entry.address == 0x45E050) {
            if (!calls.str().empty()) calls << ",";
            calls << "a";
        } else if (entry.kind == dfre::MissionTraceKind::stage && entry.address == 0x464EB0) {
            if (!calls.str().empty()) calls << ",";
            calls << "b";
        } else if (entry.kind == dfre::MissionTraceKind::external_stage &&
                   entry.address == 0x465BD0) {
            if (!calls.str().empty()) calls << ",";
            calls << "c";
        }
    }
    for (const auto& action : report.ai_actions) {
        if (!actions.str().empty()) actions << ";";
        actions << (action.kind == dfre::AiActorActionKind::update ? "u" : "s") << ","
                << unsigned(action.actor.group) << "," << action.actor.index << ","
                << action.target;
    }
    std::ostringstream marks;
    for (std::size_t index = 0; index < report.ai_marks.size(); ++index) {
        if (index) marks << ",";
        marks << report.ai_marks[index];
    }
    out << "calls=" << (calls.str().empty() ? "-" : calls.str())
        << " actions=" << (actions.str().empty() ? "-" : actions.str())
        << " marks=" << (marks.str().empty() ? "-" : marks.str())
        << " counter=" << state.ai.call_counter
        << " scripted=" << scripted_call;
    for (std::size_t group = 0; group < 4; ++group) {
        for (std::size_t index = 0; index < state.tables.groups[group].size(); ++index) {
            const dfre::AiActorRef ref{static_cast<std::uint8_t>(group), index};
            const dfre::AiActorState& actor = *state.actor_at(ref);
            const dfre::MissionActorRuntime& runtime = state.runtime_for(ref);
            out << " g" << group << "r" << index << "=" << actor.definition_index << ":"
                << static_cast<std::int32_t>(actor.position[0]) << ":"
                << static_cast<std::int32_t>(actor.position[1]) << ":"
                << static_cast<std::int32_t>(actor.position[2]) << ":" << actor.angle_yaw << ":"
                << actor.angle_pitch << ":" << runtime.vertical_44 << ":"
                << unsigned(actor.field_6c) << ":" << unsigned(state.extra_for(ref).field_6d)
                << ":" << unsigned(runtime.byte_6e) << ":" << actor.field_78 << ":"
                << actor.field_7c << ":" << (runtime.driver ? std::uint32_t(runtime.driver->current_action()) : 0u) << ":" << actor.field_fc << ":"
                << actor.field_104 << ":" << actor.field_108 << ":" << runtime.ground_100;
        }
    }
    for (std::size_t index = 0; index < state.tables.extra.size(); ++index) {
        const dfre::AiActorRef ref{4, index};
        const dfre::AiActorState& actor = *state.actor_at(ref);
        out << " xr" << index << "=" << actor.definition_index << ":" << actor.field_fc;
    }
    out << " count3=" << state.tables.groups[3].size();
    out << " local=" << (local_set ? 1 : 0) << ":" << local;
    (void)tables_keep;
    out << "\n";
    return 0;
}

int run_steady(const std::string& line,std::ostream& out) {
    const auto fields=split(line,' ');
    if(fields.size()!=4 && fields.size()!=5)throw std::runtime_error("steady needs SCOPE CONTROL WEAPON_ID AIM_BLOCK [TOGGLE]");
    auto storage=std::make_unique<dfre::MissionTickState>();
    auto& state=*storage;
    state.scope_c23a24=number(fields[0]);state.globals.control_c23a00=number(fields[1]);
    const auto weapon=number(fields[2]);
    if(weapon>=dfre::kWeaponCount)throw std::runtime_error("steady weapon id outside the table");
    std::array<std::uint8_t,dfre::kWeaponSlotCount> slots{};slots.fill(std::uint8_t(weapon));
    state.weapons.set_slot_map(slots);
    if(number(fields[3])!=0)state.weapons.toggle_aim_block();
    if(fields.size()==5 && number(fields[4])!=0){
        state.globals.pause_6e4714=1;
        state.world.bounds=[](std::uint32_t){return dfre::AiActorBounds{};};
        state.world.trigger_held_47a290=[](std::uint32_t){return false;};
    state.world.weapon_effect=[](auto&,const auto&){return true;};
    state.world.external_stage=[](auto& current,const auto& call)->std::optional<std::uint32_t>{
        if(call.address==0x482b00)dfre::mission_weapon_part(current,false);return 0;
    };
        dfre::MissionTickInputs input;dfre::DispatchResult result;
        result.calls.push_back({dfre::DispatchCallKind::weapon_unknown,0x4847c0,{},false});
        input.dispatch.push_back(result);dfre::mission_tick(state,input);
    }
    out<<dfre::mission_steady(state)<<'\n';return 0;
}

int run_frame(const std::string& line,std::ostream& out) {
    const auto parts=split(line,'|');
    if(parts.size()!=3)throw std::runtime_error("frame needs state|stop|mutations");
    auto storage=std::make_unique<dfre::MissionTickState>();
    auto& state=*storage;
    std::vector<std::uint32_t*> fields{
        &state.globals.mission_67f528,&state.globals.multiplayer_67f520,
        &state.globals.pause_6e4714,&state.globals.pause2_6e4718,
        &state.tick_67dbf4,&state.globals.command_tick_6e471c,
        &state.tick_6e4720,&state.tick_6e4724,&state.mission_timer_67dbe8,
        &state.frame_counter_4ced10,&state.frame_event_67dbf0,
        &state.frame_work_67dbcc,&state.previous_work_67dbd0,
        &state.last_clock_67dbd4,&state.clock_delta_67dbd8,
        &state.statistic_67dbdc,&state.clock_491014,&state.game_type_67f53c,
        &state.shutdown_503424,&state.globals.menu_state_67dbbc};
    const auto values=split(parts[0],',');
    if(values.size()!=fields.size())throw std::runtime_error("frame state size");
    for(std::size_t j=0;j<fields.size();++j)*fields[j]=number(values[j]);
    struct Change {std::uint32_t address;std::size_t field;std::uint32_t value;};
    std::vector<Change> changes;
    if(parts[2]!="-")for(const auto& item:split(parts[2],';')){
        const auto v=split(item,',');if(v.size()!=3)throw std::runtime_error("frame mutation size");
        changes.push_back({number(v[0]),number(v[1]),number(v[2])});
        if(changes.back().field>=fields.size())throw std::runtime_error("frame mutation field");
    }
    const auto stop=number(parts[1]);
    const auto provider=[&](auto&,const dfre::MissionStageCall& call)->std::optional<std::uint32_t>{
        for(const auto& change:changes)if(change.address==call.address)*fields[change.field]=change.value;
        return call.address==0x461350?stop:0;
    };
    const auto report=dfre::mission_frame(state,provider);
    for(std::size_t j=0;j<fields.size();++j){if(j)out<<',';out<<*fields[j];}
    out<<','<<report.result<<','<<unsigned(report.exit_reason)<<'|';
    bool first=true;
    for(const auto& call:report.frame_calls){
        if(!first)out<<';';first=false;out<<call.address;
        for(std::size_t j=0;j<call.argument_count;++j)out<<','<<call.args[j];
    }
    out<<'\n';return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    if (argc < 2) {
        std::cerr << "usage: mission_tick_probe sincos|actor|tick|frame|steady [case lines on stdin]\n";
        return 1;
    }
    const std::string mode = argv[1];
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        try {
            if (mode == "sincos") {
                const auto angle = number(line);
                const auto pair = dfre::fixed_sine_cosine(angle);
                std::cout << pair[0] << " " << pair[1] << "\n";
            } else if (mode == "actor") {
                run_actor(line, std::cout);
            } else if (mode == "steady" || mode == "--steady") {
                run_steady(line,std::cout);
            } else if (mode == "frame") {
                run_frame(line, std::cout);
            } else if (mode == "tick") {
                run_tick(line, std::cout);
            } else {
                std::cerr << "unknown mode: " << mode << "\n";
                return 1;
            }
        } catch (const std::exception& error) {
            std::cerr << "case failed: " << error.what() << " line=" << line << "\n";
            return 1;
        }
    }
    return 0;
}
