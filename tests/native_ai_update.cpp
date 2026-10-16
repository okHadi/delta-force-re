#include "dfre/ai_update.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void check(bool yes, const char* message) {
    if (!yes) throw std::runtime_error(message);
}

dfre::AiActorState make_actor(std::uint32_t definition, std::uint32_t fc) {
    dfre::AiActorState actor;
    actor.active = true;
    actor.definition_index = definition;
    actor.field_fc = fc;
    return actor;
}

dfre::AiItemDefinitionTable make_definitions(std::uint32_t field28, std::uint32_t field9c) {
    dfre::AiItemDefinition null_definition;
    dfre::AiItemDefinition type_definition;
    type_definition.id = 3012;
    type_definition.field_28 = field28;
    type_definition.field_9c = field9c;
    return dfre::AiItemDefinitionTable({null_definition, type_definition});
}

dfre::AiBoundsSource bounds_from(std::map<std::uint32_t, dfre::AiActorBounds> table) {
    return [table = std::move(table)](std::uint32_t handle) {
        const auto found = table.find(handle);
        if (found == table.end()) throw dfre::AiError("missing test bounds");
        return found->second;
    };
}

const std::vector<std::size_t> EMPTY;
} // namespace

int main() {
    try {
        const auto definitions = make_definitions(0, 10);
        const auto bounds = bounds_from({{5, {100, 200}}});
        const auto no_bounds = [](std::uint32_t) -> dfre::AiActorBounds {
            throw dfre::AiError("bounds not expected");
        };

        // Call counter: 64 increments, then 0x487E10 runs and zeroes it.
        {
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            for (std::uint32_t i = 0; i < 0x40; ++i) {
                const auto report = dfre::tick_ai(tables, state, definitions, no_bounds, 0);
                check(state.call_counter == i + 1, "Call counter increment changed");
                check(report.actions.empty(), "Empty tick produced actions");
                check(report.calls.size() == 2 &&
                          report.calls[0].kind == dfre::AiTickCall::Kind::fixed_00464eb0 &&
                          report.calls[1].kind == dfre::AiTickCall::Kind::fixed_00465bd0,
                      "Empty tick call set changed");
                check(report.profiler_marks == std::vector<std::uint32_t>{0xB, 9, 10, 9},
                      "Empty tick profiler marks changed");
            }
            dfre::tick_ai(tables, state, definitions, no_bounds, 0);
            check(state.call_counter == 0, "Rebuild trigger did not reset the call counter");
        }

        // Paused state stops before control calls; the counter still advances.
        {
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            state.paused = true;
            const auto report = dfre::tick_ai(tables, state, definitions, no_bounds, 7);
            check(state.call_counter == 1, "Paused tick lost the counter step");
            check(report.calls.empty() && report.actions.empty() && report.profiler_marks.empty(),
                  "Paused tick produced work");        }

        // Odd ticks add the local control calls before the actor work.
        {
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            auto report = dfre::tick_ai(tables, state, definitions, no_bounds, 7);
            check(report.calls.size() == 4, "Odd tick call count changed");
            check(report.calls[0].kind == dfre::AiTickCall::Kind::local_player_control &&
                      report.calls[1].kind == dfre::AiTickCall::Kind::fixed_0045e050 &&
                      report.calls[2].kind == dfre::AiTickCall::Kind::fixed_00464eb0 &&
                      report.calls[3].kind == dfre::AiTickCall::Kind::fixed_00465bd0,
                  "Odd tick call order changed");
            check(report.profiler_marks == std::vector<std::uint32_t>{0xB, 9, 10, 9},
                  "Odd tick profiler marks changed");
            report = dfre::tick_ai(tables, state, definitions, no_bounds, 8);
            check(report.calls.size() == 2 && report.calls[0].kind == dfre::AiTickCall::Kind::fixed_00464eb0 &&
                      report.calls[1].kind == dfre::AiTickCall::Kind::fixed_00465bd0,
                  "Even tick call set changed");
        }

        // Group 3: +0x18 gate, per-actor update, +0xFC/+0x78 countdowns, +0x110 action.
        {
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            auto active = make_actor(1, 0);
            active.field_18 = 5;
            active.field_78 = 3;
            active.field_110 = 0x40;
            tables.groups[3].push_back(active);
            auto gated = make_actor(1, 4);
            gated.field_18 = 0;
            tables.groups[3].push_back(gated);
            const auto report = dfre::tick_ai(tables, state, definitions, no_bounds, 0);
            check(report.calls.size() == 3 && report.calls[0].kind == dfre::AiTickCall::Kind::per_actor_update &&
                      report.calls[0].actor.group == 3 && report.calls[0].actor.index == 0 &&
                      report.calls[1].kind == dfre::AiTickCall::Kind::fixed_00464eb0 &&
                      report.calls[2].kind == dfre::AiTickCall::Kind::fixed_00465bd0,
                  "Group 3 update call changed");
            check(report.actions.size() == 1 && report.actions[0].kind == dfre::AiActorActionKind::update &&
                      report.actions[0].target == 0x40,
                  "Group 3 update action changed");
            check(report.profiler_marks == std::vector<std::uint32_t>{0xB, 0xC, 0xB, 9, 10, 9},
                  "Group 3 profiler marks changed");
            check(tables.groups[3][0].field_fc == 0xFFFFFFFFu, "Group 3 field FC decrement changed");
            check(tables.groups[3][0].field_78 == 2, "Group 3 field 78 decrement changed");
            check(tables.groups[3][1].field_fc == 4 && tables.groups[3][1].field_78 == 0,
                  "Gated group 3 record changed");
        }

        // Group 0: +0xFC action when below 1, +0x10C secondary excluding the default stub.
        {
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            tables.groups[0].push_back(make_actor(0, 5));
            auto secondary = make_actor(1, 5);
            secondary.field_10c = 0x60;
            tables.groups[0].push_back(secondary);
            auto defaulted = make_actor(1, 5);
            defaulted.field_10c = dfre::kAiDefaultCallback;
            tables.groups[0].push_back(defaulted);
            auto due = make_actor(1, 0);
            due.field_110 = 0x70;
            tables.groups[0].push_back(due);
            const auto report = dfre::tick_ai(tables, state, definitions, no_bounds, 0);
            check(report.actions.size() == 2, "Group 0 action count changed");
            check(report.actions[0].kind == dfre::AiActorActionKind::secondary &&
                      report.actions[0].actor.index == 1 && report.actions[0].target == 0x60,
                  "Group 0 secondary action changed");
            check(report.actions[1].kind == dfre::AiActorActionKind::update &&
                      report.actions[1].actor.index == 3 && report.actions[1].target == 0x70,
                  "Group 0 update action changed");
            check(tables.groups[0][0].field_fc == 5 && tables.groups[0][1].field_fc == 4 &&
                      tables.groups[0][3].field_fc == 0xFFFFFFFFu,
                  "Group 0 field FC transitions changed");
        }

        // Extra table: +0xFC increments; definition +0x9C clears the record.
        {
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            auto kept = make_actor(1, 8);
            kept.field_10c = 0x80;
            tables.extra.push_back(kept);
            auto expiring = make_actor(1, 9);
            expiring.field_10c = 0x90;
            tables.extra.push_back(expiring);
            const auto report = dfre::tick_ai(tables, state, definitions, no_bounds, 0);
            check(report.actions.size() == 1 && report.actions[0].target == 0x80 &&
                      report.actions[0].actor.group == dfre::kAiExtraTableGroup,
                  "Extra table callback changed");
            check(tables.extra[0].field_fc == 9 && tables.extra[0].definition_index == 1,
                  "Extra table kept record changed");
            check(tables.extra[1].definition_index == 0 && tables.extra[1].field_fc == 0 &&
                      tables.extra[1].field_10c == 0,
                  "Extra table expiry did not clear the record");
        }

        // Groups 1 and 2: round-robin subsets and the 0x20/0x40 decrements.
        {
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            for (std::uint32_t i = 0; i < 40; ++i) tables.groups[1].push_back(make_actor(1, 0x40));
            for (std::uint32_t i = 0; i < 40; ++i) tables.groups[2].push_back(make_actor(1, 0x80));
            dfre::tick_ai(tables, state, definitions, no_bounds, 1);
            check(tables.groups[1][0].field_fc == 0x40, "Group 1 touched an unselected record");
            check(tables.groups[1][1].field_fc == 0x20, "Group 1 selected decrement changed");
            check(tables.groups[1][33].field_fc == 0x20, "Group 1 second selected decrement changed");
            check(tables.groups[2][0].field_fc == 0x80, "Group 2 touched an unselected record");
            check(tables.groups[2][1].field_fc == 0x40, "Group 2 selected decrement changed");
            check(tables.groups[2][24].field_fc == 0x80, "Group 2 stride changed");
            auto report = dfre::tick_ai(tables, state, definitions, no_bounds, 0);
            check(report.actions.empty(), "Groups 1/2 produced actions with live counters");
            tables.groups[1][0].field_fc = 0;
            tables.groups[2][0].field_fc = 0;
            report = dfre::tick_ai(tables, state, definitions, no_bounds, 0);
            check(report.actions.size() == 2 && report.actions[0].actor.group == 1 &&
                      report.actions[1].actor.group == 2,
                  "Groups 1/2 due actions changed");
        }

        // Demo branch: local group 3 actor only.
        {
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            state.demo = true;
            state.local_actor = 0;
            tables.groups[3].push_back(make_actor(1, 2));
            auto report = dfre::tick_ai(tables, state, definitions, no_bounds, 0);
            check(report.calls.size() == 1 && report.calls[0].kind == dfre::AiTickCall::Kind::per_actor_update &&
                      report.actions.empty() && report.profiler_marks.empty() &&
                      tables.groups[3][0].field_fc == 1,
                  "Demo live-countdown branch changed");
            tables.groups[3][0].field_fc = 0;
            tables.groups[3][0].field_110 = 0x50;
            report = dfre::tick_ai(tables, state, definitions, no_bounds, 0);
            check(report.actions.size() == 1 && report.actions[0].target == 0x50 &&
                      tables.groups[3][0].field_fc == 0xFFFFFFFFu,
                  "Demo due branch changed");
        }

        // 0x487C50: definition type gates and exact range boundaries.
        {
            const auto typed = make_definitions(1, 0);
            auto bounds_map = std::map<std::uint32_t, dfre::AiActorBounds>{{5, {100, 200}}};
            const auto bounds5 = bounds_from(bounds_map);
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            auto self = make_actor(1, 0);
            self.field_10 = 5;
            tables.groups[3].push_back(self);
            auto edge = make_actor(1, 0);
            edge.field_10 = 5;
            edge.position[0] = 100 + 0x140000;
            tables.groups[1].push_back(edge);
            auto outside = make_actor(1, 0);
            outside.field_10 = 5;
            outside.position[0] = 100 + 0x140000 + 1;
            tables.groups[1].push_back(outside);
            auto no_handle = make_actor(1, 0);
            no_handle.position[0] = 10;
            tables.groups[1].push_back(no_handle);
            auto flagged = make_actor(1, 0);
            flagged.field_10 = 5;
            flagged.field_6c = 2;
            tables.groups[1].push_back(flagged);
            auto self_candidate = make_actor(1, 0);
            self_candidate.field_10 = 5;
            tables.groups[0].push_back(self_candidate);
            auto candidate0 = make_actor(1, 0);
            candidate0.field_10 = 5;
            tables.groups[0].push_back(candidate0);
            dfre::rebuild_actor_targets(tables, {3, 0}, state, typed, bounds5);
            check(state.target_list.size() == 3, "Target list size changed");
            check(state.target_list[0].group == 1 && state.target_list[0].index == 0,
                  "Group 1 order changed");
            check(state.target_list[1].group == 0 && state.target_list[1].index == 0 &&
                      state.target_list[2].group == 0 && state.target_list[2].index == 1,
                  "Group 0 order changed");
            check(tables.groups[3][0].field_104 == 0 && tables.groups[3][0].field_108 == 3,
                  "Selected group 3 actor list fields changed");
            check(tables.groups[1][1].field_108 == 0 && tables.groups[1][3].field_108 == 0,
                  "Rejected candidate got a list");
        }

        // Self-skip inside group 0 (original compares the record pointer).
        {
            const auto typed = make_definitions(1, 0);
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            auto self = make_actor(1, 0);
            self.field_10 = 5;
            tables.groups[0].push_back(self);
            auto other = make_actor(1, 0);
            other.field_10 = 5;
            tables.groups[0].push_back(other);
            dfre::rebuild_actor_targets(tables, {0, 0}, state, typed, bounds);
            check(state.target_list.size() == 1 && state.target_list[0].group == 0 &&
                      state.target_list[0].index == 1,
                  "Self candidate was not skipped");
        }

        // Type gate: only 1, 3, and 6 scan; +0x104/+0x108 are always written.
        {
            for (const std::uint32_t type : {0u, 1u, 2u, 3u, 5u, 6u, 7u}) {
                const auto typed = make_definitions(type, 0);
                dfre::AiActorTables tables;
                dfre::AiUpdateState state;
                state.target_list.push_back({2, 9});
                auto self = make_actor(1, 0);
                tables.groups[3].push_back(self);
                auto candidate = make_actor(1, 0);
                candidate.field_10 = 5;
                tables.groups[1].push_back(candidate);
                dfre::rebuild_target_lists(tables, state, typed, bounds);
                const bool scans = type == 1 || type == 3 || type == 6;
                check(tables.groups[3][0].field_104 == 0, "List start field changed");
                check(tables.groups[3][0].field_108 == (scans ? 1 : 0), "Type gate scan changed");
                check(state.target_list.size() == (scans ? 1u : 0u), "Type gate list size changed");
            }
        }

        // Cap: a full shared list stops further appends (original 1999 check).
        {
            const auto typed = make_definitions(1, 0);
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            state.target_list.assign(2000, {1, 0});
            auto self = make_actor(1, 0);
            tables.groups[3].push_back(self);
            auto candidate = make_actor(1, 0);
            candidate.field_10 = 5;
            tables.groups[1].push_back(candidate);
            dfre::rebuild_actor_targets(tables, {3, 0}, state, typed, bounds);
            check(state.target_list.size() == 2000, "Target list cap changed");
            check(tables.groups[3][0].field_108 == 0, "Capped actor list count changed");
            check(tables.groups[3][0].field_104 == 2000, "Capped actor list start changed");
        }

        // 0x487E10 selection: group 3 then group 0, skipping def index 0 and +0x6C bit 2.
        {
            const auto typed = make_definitions(0, 0);
            dfre::AiActorTables tables;
            dfre::AiUpdateState state;
            state.call_counter = 0x40;
            state.target_list.push_back({9, 9});
            auto selected3 = make_actor(1, 0);
            auto flagged3 = make_actor(1, 0);
            flagged3.field_6c = 2;
            auto empty3 = make_actor(0, 0);
            tables.groups[3] = {selected3, flagged3, empty3};
            auto first0 = make_actor(1, 0);
            auto second0 = make_actor(1, 0);
            tables.groups[0] = {first0, second0};
            const auto report = dfre::tick_ai(tables, state, typed, bounds, 0);
            check(state.call_counter == 0, "Tick trigger did not zero the counter");
            check(state.target_list.empty(), "Tick trigger did not clear the list");
            check(tables.groups[3][0].field_104 == 0 && tables.groups[3][0].field_108 == 0,
                  "Group 3 selection fields changed");
            check(tables.groups[3][1].field_104 == 0 && tables.groups[3][1].field_108 == 0,
                  "Flagged record was rebuilt");
            check(tables.groups[0][1].field_104 == 0, "Group 0 selection fields changed");
            check(report.profiler_marks == std::vector<std::uint32_t>{0xB, 9, 10, 9},
                  "Trigger tick marks changed");
        }

        // Runtime calls can replace vector storage while actor slots stay fixed.
        {
            dfre::AiActorTables tables;
            auto actor=make_actor(1,7);actor.field_18=5;actor.field_78=7;actor.field_110=123;
            tables.groups[3].push_back(actor);
            dfre::AiUpdateState state;
            dfre::AiTickHooks hooks;
            unsigned actions=0;
            hooks.on_call=[&](const dfre::AiTickCall& call) {
                if(call.kind!=dfre::AiTickCall::Kind::per_actor_update)return;
                auto replacement=tables.groups[3];replacement.push_back({});
                tables.groups[3].swap(replacement);
                tables.groups[3][0].field_fc=0;
            };
            hooks.on_action=[&](const dfre::AiActorAction& action) {
                check(action.target==123 && tables.groups[3][0].field_fc==0,
                    "AI callback used state from before the actor update");
                tables.groups[3][0].field_fc=13;tables.groups[3][0].field_78=4;++actions;
            };
            dfre::tick_ai(tables,state,definitions,no_bounds,2,hooks);
            check(actions==1 && tables.groups[3][0].field_fc==12 && tables.groups[3][0].field_78==3,
                "AI countdown ran before its runtime callback");
        }
        {
            dfre::AiActorTables tables;
            auto actor=make_actor(1,0);actor.field_110=123;tables.groups[0].push_back(actor);
            dfre::AiUpdateState state;
            dfre::AiTickHooks hooks;
            std::vector<unsigned> calls;
            hooks.on_action=[&](const dfre::AiActorAction& action) {
                calls.push_back(action.target);
                if(action.kind==dfre::AiActorActionKind::update) {
                    tables.groups[0][0].field_10c=456;tables.groups[0][0].field_fc=11;
                } else tables.groups[0][0].field_fc+=7;
            };
            dfre::tick_ai(tables,state,definitions,no_bounds,2,hooks);
            check(calls==std::vector<unsigned>{123,456} && tables.groups[0][0].field_fc==17,
                "AI secondary callback used state from before the primary callback");
        }

        std::cout << "native_ai_update checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
