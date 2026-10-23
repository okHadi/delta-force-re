#include "dfre/ai_update.hpp"

#include <cstring>

namespace dfre {
namespace {

const AiItemDefinition& definition_for(const AiItemDefinitionTable& definitions,
                                       const AiActorState& actor) {
    if (actor.definition_index >= definitions.entries().size())
        throw AiError("AI actor definition index is outside the definition table");
    return definitions.entries()[actor.definition_index];
}

// Original absolute-value idiom: sign = diff >> 31 (arithmetic); abs = (diff ^ sign) - sign.
std::int32_t wrapped_abs(std::int32_t value) {
    const std::uint32_t bits = static_cast<std::uint32_t>(value);
    const std::uint32_t sign = (bits & 0x80000000u) != 0 ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((bits ^ sign) - sign);
}

} // namespace

void rebuild_actor_targets(AiActorTables& tables, AiActorRef actor, AiUpdateState& state,
                           const AiItemDefinitionTable& definitions,
                           const AiBoundsSource& bounds) {
    if (actor.group > 3 || actor.index >= tables.groups[actor.group].size())
        throw AiError("AI rebuild target is outside the actor tables");
    AiActorState& self = tables.groups[actor.group][actor.index];
    self.field_108 = 0;
    self.field_104 = static_cast<std::uint32_t>(state.target_list.size());
    const auto& definition = definition_for(definitions, self);

    std::uint32_t range = 0;
    if (definition.field_28 == 1) range = 0x140000;
    else if (definition.field_28 == 3 || definition.field_28 == 6) range = 0xa0000;
    else return;

    const auto consider = [&](std::uint8_t group, std::size_t index) {
        const AiActorState& candidate = tables.groups[group][index];
        if (candidate.definition_index == 0) return;
        if ((candidate.field_6c & 2) != 0) return;
        if (candidate.field_10 == 0) return;
        if (group == actor.group && index == actor.index) return;
        const AiActorBounds candidate_bounds = bounds(candidate.field_10);
        const std::int32_t limit = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(candidate_bounds.radius_64) + range);
        const std::int32_t height_limit = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(candidate_bounds.height_60) + range);
        if (wrapped_abs(static_cast<std::int32_t>(candidate.position[0]) -
                        static_cast<std::int32_t>(self.position[0])) > limit)
            return;
        if (wrapped_abs(static_cast<std::int32_t>(candidate.position[1]) -
                        static_cast<std::int32_t>(self.position[1])) > limit)
            return;
        if (wrapped_abs(static_cast<std::int32_t>(candidate.position[2]) -
                        static_cast<std::int32_t>(self.position[2])) > height_limit)
            return;
        if (state.target_list.size() > 1999) return;
        state.target_list.push_back({group, index});
        self.field_108 += 1;
    };

    for (std::size_t index = 0; index < tables.groups[1].size(); ++index) {
        consider(1, index);
        if (state.target_list.size() > 1999) return;
    }
    for (std::size_t index = 0; index < tables.groups[0].size(); ++index) {
        consider(0, index);
        if (state.target_list.size() > 1999) return;
    }
}

void rebuild_target_lists(AiActorTables& tables, AiUpdateState& state,
                          const AiItemDefinitionTable& definitions,
                          const AiBoundsSource& bounds) {
    state.target_list.clear();
    state.call_counter = 0;
    for (std::size_t index = 0; index < tables.groups[3].size(); ++index) {
        const auto& actor = tables.groups[3][index];
        if (actor.definition_index != 0 && (actor.field_6c & 2) == 0)
            rebuild_actor_targets(tables, {3, index}, state, definitions, bounds);
    }
    for (std::size_t index = 0; index < tables.groups[0].size(); ++index) {
        const auto& actor = tables.groups[0][index];
        if (actor.definition_index != 0 && (actor.field_6c & 2) == 0)
            rebuild_actor_targets(tables, {0, index}, state, definitions, bounds);
    }
}

AiTickReport tick_ai(AiActorTables& tables, AiUpdateState& state,
                     const AiItemDefinitionTable& definitions,
                     const AiBoundsSource& bounds, std::uint32_t tick) {
    return tick_ai(tables,state,definitions,bounds,tick,AiTickHooks{});
}

AiTickReport tick_ai(AiActorTables& tables, AiUpdateState& state,
                     const AiItemDefinitionTable& definitions,
                     const AiBoundsSource& bounds, std::uint32_t tick, const AiTickHooks& hooks) {
    AiTickReport report;
    const auto emit_call = [&](AiTickCall call) {
        report.calls.push_back(call);
        if(hooks.on_call)hooks.on_call(call);
    };
    const auto emit_action = [&](AiActorAction action) {
        report.actions.push_back(action);
        if(hooks.on_action)hooks.on_action(action);
    };
    const auto emit_mark = [&](std::uint32_t mark) {
        report.profiler_marks.push_back(mark);
        if(hooks.on_mark)hooks.on_mark(mark);
    };
    if (state.call_counter < 0x40) {
        state.call_counter += 1;
    } else {
        rebuild_target_lists(tables, state, definitions, bounds);
    }
    if (state.paused) return report;

    if ((tick & 1) != 0) {
        emit_call({AiTickCall::Kind::local_player_control, {}});
        emit_call({AiTickCall::Kind::fixed_0045e050, {}});
    }

    if (state.demo) {
        if (!state.local_actor || *state.local_actor >= tables.groups[3].size())
            throw AiError("AI demo update needs the local group 3 actor");
        const auto local = *state.local_actor;
        emit_call({AiTickCall::Kind::per_actor_update, {3, *state.local_actor}});
        if (static_cast<std::int32_t>(tables.groups[3][local].field_fc) > 0) {
            tables.groups[3][local].field_fc -= 1;
            return report;
        }
        emit_action(
            {AiActorActionKind::update, {3, *state.local_actor}, tables.groups[3][local].field_110});
        tables.groups[3][local].field_fc -= 1;
        return report;
    }

    emit_mark(0xB);
    for (std::size_t index = 0; index < tables.groups[3].size(); ++index) {
        if (tables.groups[3][index].field_18 == 0) continue;
        emit_call({AiTickCall::Kind::per_actor_update, {3, index}});
        if (static_cast<std::int32_t>(tables.groups[3][index].field_fc) < 1) {
            emit_mark(0xC);
            emit_action({AiActorActionKind::update, {3, index}, tables.groups[3][index].field_110});
            emit_mark(0xB);
        }
        tables.groups[3][index].field_fc -= 1;
        if (tables.groups[3][index].field_78 != 0) tables.groups[3][index].field_78 -= 1;
    }

    emit_mark(9);
    for (std::size_t index = 0; index < tables.groups[0].size(); ++index) {
        if (tables.groups[0][index].definition_index == 0) continue;
        if (static_cast<std::int32_t>(tables.groups[0][index].field_fc) < 1)
            emit_action({AiActorActionKind::update, {0, index}, tables.groups[0][index].field_110});
        if (tables.groups[0][index].field_10c != 0 && tables.groups[0][index].field_10c != kAiDefaultCallback)
            emit_action({AiActorActionKind::secondary, {0, index}, tables.groups[0][index].field_10c});
        tables.groups[0][index].field_fc -= 1;
    }

    for (std::size_t index = 0; index < tables.extra.size() && index < 0x400; ++index) {
        AiActorState& actor = tables.extra[index];
        if (actor.definition_index == 0) continue;
        const auto& definition = definition_for(definitions, actor);
        const std::uint32_t next = actor.field_fc + 1u;
        actor.field_fc = next;
        if (static_cast<std::int32_t>(definition.field_9c) <= static_cast<std::int32_t>(next)) {
            actor = AiActorState{};
            if(hooks.on_clear)hooks.on_clear({kAiExtraTableGroup,index});
        }
        if (actor.field_10c != 0 && actor.field_10c != kAiDefaultCallback)
            emit_action(
                {AiActorActionKind::secondary, {kAiExtraTableGroup, index}, actor.field_10c});
    }

    emit_mark(10);
    emit_call({AiTickCall::Kind::fixed_00464eb0, {}});
    emit_call({AiTickCall::Kind::fixed_00465bd0, {}});
    emit_mark(9);

    for (std::size_t index = tick & 0x1F; index < tables.groups[1].size(); index += 0x20) {
        AiActorState& actor = tables.groups[1][index];
        if (actor.definition_index == 0) continue;
        if (static_cast<std::int32_t>(actor.field_fc) < 1) {
            emit_action({AiActorActionKind::update, {1, index}, actor.field_110});
        } else {
            actor.field_fc -= 0x20;
        }
    }
    for (std::size_t index = tick & 0x3F; index < tables.groups[2].size(); index += 0x40) {
        AiActorState& actor = tables.groups[2][index];
        if (actor.definition_index == 0) continue;
        if (static_cast<std::int32_t>(actor.field_fc) < 1) {
            emit_action({AiActorActionKind::update, {2, index}, actor.field_110});
        } else {
            actor.field_fc -= 0x40;
        }
    }
    return report;
}

} // namespace dfre
