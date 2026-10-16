#include "dfre/actor_behavior.hpp"

#include <string>

namespace dfre {
namespace {

std::int32_t as_i32(std::uint32_t value) {
    return static_cast<std::int32_t>(value);
}

std::uint32_t as_u32(std::int32_t value) {
    return static_cast<std::uint32_t>(value);
}

// Original 477670 sorts the three signed inputs and returns
// max + (mid * 11 >> 5) + (min >> 2).
std::int32_t sar(std::int32_t value, int amount) {
    return value >> amount;
}

std::uint32_t abs_bits(std::int32_t value) {
    const std::uint32_t bits = as_u32(value);
    const std::uint32_t sign = (bits & 0x80000000u) != 0 ? 0xFFFFFFFFu : 0u;
    return (bits ^ sign) - sign;
}

void write_u16(AiActorImage& image, std::size_t at, std::uint16_t value) {
    image[at] = static_cast<std::uint8_t>(value & 0xFF);
    image[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void write_u32(AiActorImage& image, std::size_t at, std::uint32_t value) {
    image[at] = static_cast<std::uint8_t>(value & 0xFF);
    image[at + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    image[at + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    image[at + 3] = static_cast<std::uint8_t>(value >> 24);
}

void write_u32(AiPersonImage& image, std::size_t at, std::uint32_t value) {
    image[at] = static_cast<std::uint8_t>(value & 0xFF);
    image[at + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    image[at + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    image[at + 3] = static_cast<std::uint8_t>(value >> 24);
}

// Original 445CE0 / 442760 payload index: (pointer - 0x7CCA80) / 0x114.
std::uint32_t actor_index_from_raw(std::uint32_t raw) {
    return (raw - 0x7CCA80u) / 0x114u;
}

std::uint32_t take_input(BehaviorState& state, std::uint32_t site) {
    if(state.cursor>=state.inputs.size() && state.on_query) {
        if(state.actions.empty() || state.actions.back().kind!=BehaviorCallKind::query || state.actions.back().site!=site)
            throw BehaviorError("behavior query has no matching call");
        const BehaviorAction request=state.actions.back();
        state.inputs.push_back(state.on_query(request));
    }
    if (state.cursor >= state.inputs.size())
        throw BehaviorError("behavior input stream is exhausted");
    const BehaviorInput& input = state.inputs[state.cursor];
    if (input.site != site)
        throw BehaviorError("behavior input site 0x" + std::to_string(site) +
                            " does not match the scripted 0x" + std::to_string(input.site));
    ++state.cursor;
    return input.value;
}

std::uint32_t resolve_target_type(const BehaviorWorld& world, std::uint32_t raw) {
    if (!world.resolve_target) throw BehaviorError("behavior world has no target resolver");
    const auto target = world.resolve_target(raw);
    if (!target) throw BehaviorError("behavior world cannot resolve the target pointer");
    return target->field_70 >> 16;
}

void emit_message(BehaviorActor& view, BehaviorState& state, std::uint32_t msg) {
    state.emit(BehaviorCallKind::effect, behavior_site::kMessage,
               actor_index_from_raw(view.raw), msg);
}

// 45C810 animation selector. weapon_flags is the foreign weapon-table dword
// read at [0xAD3ED0] + actor+0x94 * 0xA8 + 0xA4.
std::uint32_t animation_id_45c810(BehaviorActor& view, BehaviorState& state) {
    state.emit(BehaviorCallKind::query, behavior_site::kWeaponFlags810,
               view.actor_extra.field_94);
    const std::uint32_t weapon_flags = state.take(behavior_site::kWeaponFlags810);
    if (view.person.field_9c == 2) {
        if (view.actor_extra.field_94 != 0 && (weapon_flags & 4) != 0) return 0x44;
        return 0x3C;
    }
    if (view.actor_extra.field_94 != 0 && (weapon_flags & 4) != 0) return 0x1C;
    return 0x14;
}

// 45C880 animation selector; see animation_id_45c810.
std::uint32_t animation_id_45c880(BehaviorActor& view, BehaviorState& state) {
    state.emit(BehaviorCallKind::query, behavior_site::kWeaponFlags880,
               view.actor_extra.field_94);
    const std::uint32_t weapon_flags = state.take(behavior_site::kWeaponFlags880);
    const std::uint8_t person_flags = view.person.flags24;
    if (view.person.field_9c == 2) {
        if ((person_flags & 0x20) != 0) return (weapon_flags & 4) != 0 ? 0x6F : 0x6E;
        if ((person_flags & 0x10) != 0) return (weapon_flags & 4) != 0 ? 0x0B : 0x0A;
        return (weapon_flags & 4) != 0 ? 0x65 : 0x64;
    }
    return (weapon_flags & 4) != 0 ? 2 : 1;
}

} // namespace

std::uint32_t BehaviorState::take(std::uint32_t site) {
    return take_input(*this, site);
}

void BehaviorState::emit(BehaviorCallKind kind, std::uint32_t site, std::uint32_t a1,
                         std::uint32_t a2, std::uint32_t a3) {
    const BehaviorAction action{kind,site,{a1,a2,a3}};
    actions.push_back(action);
    if(kind==BehaviorCallKind::effect && on_effect)on_effect(action);
}

std::uint32_t behavior_rng_next(BehaviorState& state) {
    const std::uint32_t old = state.rng;
    std::uint32_t value = old;
    value = (value << 11) | (value >> 21);  // rol eax, 0xb
    value += old;
    value = (value << 4) | (value >> 28);   // rol eax, 4
    value ^= 1;
    state.rng = value;
    return value & 0xFFFF;
}

std::uint32_t behavior_octagon2(std::int32_t a, std::int32_t b) {
    const std::uint32_t abs_a = abs_bits(a);
    const std::uint32_t abs_b = abs_bits(b);
    if (abs_a > abs_b) return abs_a + (abs_b >> 1);
    return abs_b + (abs_a >> 1);
}

std::uint32_t behavior_octagon3(std::int32_t a, std::int32_t b, std::int32_t c) {
    std::int32_t max = a, mid = b, min = c;
    if (max < mid) std::swap(max, mid);
    if (max < min) std::swap(max, min);
    if (mid < min) std::swap(mid, min);
    const std::int32_t scaled = sar(mid * 11, 5);
    return as_u32(max + scaled + sar(min, 2));
}

std::uint32_t behavior_facing_angle(std::int32_t dx, std::int32_t dy, BehaviorState& state) {
    const std::uint32_t abs_dx = abs_bits(dx);
    const std::uint32_t abs_dy = abs_bits(dy);
    if (as_u32(as_i32(abs_dy) >> 10) > abs_dx)
        return dy < 0 ? 0xC0000000u : 0x40000000u;
    if (as_u32(as_i32(abs_dx) >> 10) > abs_dy)
        return dx < 0 ? 0x80000000u : 0x0u;
    state.emit(BehaviorCallKind::query, behavior_site::kAngle, as_u32(dx), as_u32(dy));
    return state.take(behavior_site::kAngle);
}

std::uint32_t behavior_step(BehaviorActor& view, std::uint32_t value) {
    BehaviorPersonExtra& person = view.person_extra;
    view.actor.field_fc = value;
    person.field_2c += value;
    person.field_38 += value;
    person.field_3c += value;
    person.field_40 += value;
    if ((view.person.flags24 & 0x80) != 0) {
        person.field_34 += value;
        person.field_30 = 0;
    } else {
        person.field_34 = 0;
        person.field_30 += value;
    }
    view.actor_extra.field_6d |= 1;
    return value;
}

bool behavior_marker_query(BehaviorActor& view, const BehaviorWorld& world,
                           BehaviorState& state, BehaviorMarkerTarget& out) {
    BehaviorPersonExtra& person = view.person_extra;
    if (person.field_a4 == 1) {
        const std::uint32_t list_index = view.person.field_ac;
        if (list_index == 0) return false;
        if (list_index >= world.lists.size())
            throw BehaviorError("marker list index is outside the behavior world");
        const BehaviorMarkerList& list = world.lists[list_index];
        if (list.count == 0) return false;
        const std::size_t entry = view.person.field_b0;
        if (entry >= list.indices.size())
            throw BehaviorError("marker list entry is outside the 32-entry record");
        const std::uint32_t marker_index = list.indices[entry];
        if (marker_index >= world.markers.size())
            throw BehaviorError("marker index is outside the behavior world");
        const BehaviorMarker& marker = world.markers[marker_index];
        const std::int32_t dx = as_i32(marker.position[0] - view.actor.position[0]);
        const std::int32_t dy = as_i32(marker.position[1] - view.actor.position[1]);
        out.distance = behavior_octagon2(dx, dy);
        out.angle = behavior_facing_angle(dx, dy, state);
        out.radius = marker.radius;
        out.yaw = marker.yaw;
        return true;
    }
    if (person.field_a4 == 3) {
        if (view.person.field_ac == 0) return false;
        if (!world.resolve_marker)
            throw BehaviorError("behavior world has no marker resolver");
        const auto marker_index = world.resolve_marker(view.person.field_ac);
        if (!marker_index) return false;
        if (*marker_index >= world.markers.size())
            throw BehaviorError("resolved marker index is outside the behavior world");
        const BehaviorMarker& marker = world.markers[*marker_index];
        if (marker.object_id != view.person.field_b0) return false;
        const std::int32_t dx = as_i32(marker.position[0] - view.actor.position[0]);
        const std::int32_t dy = as_i32(marker.position[1] - view.actor.position[1]);
        const std::int32_t dz = as_i32(marker.position[2] - view.actor.position[2]);
        out.distance = behavior_octagon3(dx, dy, dz);
        out.angle = behavior_facing_angle(dx, dy, state);
        out.radius = 0x100000;
        return true;
    }
    return false;
}

bool behavior_waypoint_step(BehaviorActor& view, const BehaviorWorld& world,
                            BehaviorState& state) {
    BehaviorPersonExtra& person = view.person_extra;
    BehaviorMarkerTarget target;
    if (!behavior_marker_query(view, world, state, target)) {
        person.field_a4 = 0;
        behavior_step(view, 0x10);
        return false;
    }
    if (as_i32(target.distance) > as_i32(target.radius)) {
        view.actor.field_7c = animation_id_45c810(view, state);
        view.actor.angle_yaw = target.angle;
        if (as_i32(target.distance) > 0x80000) behavior_step(view, 0x40);
        else behavior_step(view, as_u32(as_i32(target.distance) >> 13));
        return true;
    }
    state.emit(BehaviorCallKind::effect, behavior_site::kRegionBit3,
               view.actor.field_72, view.person.field_ac, view.person.field_b0);
    view.person.field_b0 += 1;
    const std::uint32_t list_index = view.person.field_ac;
    if (list_index >= world.lists.size())
        throw BehaviorError("waypoint list index is outside the behavior world");
    if (view.person.field_b0 >= world.lists[list_index].count) view.person.field_b0 = 0;
    const bool next_entry = view.person.field_b0 != 0;
    if (next_entry || (world.lists[list_index].flags & 1) == 0) {
        // Re-query; when it fails, the original keeps the previous outputs.
        behavior_marker_query(view, world, state, target);
        view.actor.field_7c = animation_id_45c810(view, state);
        view.actor.angle_yaw = target.angle;
        behavior_step(view, 8);
        return true;
    }
    view.actor.field_7c = animation_id_45c880(view, state);
    person.field_a4 = 0;
    view.person.field_ac = 0;
    view.actor.angle_yaw = target.yaw;
    behavior_step(view, 0x10);
    return true;
}

void behavior_null(BehaviorActor& view) {
    view.actor.field_fc = 0x1000000;
}

void behavior_org0(BehaviorActor& view) {
    (void)view;
}

void behavior_org1(BehaviorActor& view, const BehaviorWorld& world, BehaviorState& state,
                   std::uint32_t param2) {
    AiActorState& actor = view.actor;
    AiPersonState& person = view.person;
    BehaviorActorExtra& actor_extra = view.actor_extra;
    BehaviorPersonExtra& person_extra = view.person_extra;

    actor_extra.field_94 = person.field_64;
    const bool block_flag = (person.flags24 & 0x40) != 0;

    if (!state.host) {
        if (param2 == 4) actor.field_fc = 0x400;
        actor.field_fc = 0x400;
        return;
    }
    if ((actor.field_6c & 4) != 0) {
        behavior_step(view, 0x400);
        return;
    }
    if (static_cast<std::int16_t>(actor_extra.field_74) <= 0) {
        if (actor_extra.field_84 == 0) {
            state.emit(BehaviorCallKind::query, behavior_site::kAnimInit, 0, 0, 1);
            actor_extra.field_84 = state.take(behavior_site::kAnimInit);
        }
        state.emit(BehaviorCallKind::effect, behavior_site::kSetAnimation, actor_extra.field_84, 0);
        actor.field_7c = actor_extra.field_84;
        actor.field_6c |= 0x04;
        actor_extra.field_6d |= 0x01;
        behavior_step(view, 0x40);
        state.emit(BehaviorCallKind::effect, behavior_site::kNotifyEvent11,
                   actor_index_from_raw(view.raw), actor_extra.field_84);
        return;
    }

    {
        std::uint32_t target_raw = 0;  // original edi
        bool seen = false;             // original local uStack_18
        std::uint32_t distance = 0;    // original edi after 477670
        std::int32_t diff_x = 0, diff_y = 0, diff_z = 0;

        if (person_extra.field_6c != 0) goto combat_branch;
        target_raw = person_extra.field_44;
        if (target_raw == 0 || block_flag) goto no_target;

        // ---- target branch (45B635) -----------------------------------------
        state.emit(BehaviorCallKind::effect, behavior_site::kRegionBit2,
                   actor.field_72, resolve_target_type(world, target_raw));
        if (as_i32(person_extra.field_40) <= 0xBA) {
            seen = (person.flags24 & 0x80) != 0;
            goto landmark_check;
        }
        person_extra.field_40 = 0;
        actor.position[2] += 0x10000;  // original `inc word [actor+0x2E]`
        state.emit(BehaviorCallKind::query, behavior_site::kSight, target_raw);
        {
            const std::uint32_t sight = state.take(behavior_site::kSight);
            actor.position[2] -= 0x10000;
            if (as_i32(sight) < 1) {
                person.flags24 &= 0x7F;
            } else {
                state.emit(BehaviorCallKind::effect, behavior_site::kRegionBit,
                           actor.field_72, resolve_target_type(world, target_raw));
                person.flags24 |= 0x80;
            }
            seen = as_i32(sight) >= 1;
        }
        state.emit(BehaviorCallKind::query, behavior_site::kTargetSearch);
        {
            const std::uint32_t next_raw = state.take(behavior_site::kTargetSearch);
            const std::uint32_t next_distance = state.inputs[state.cursor - 1].value2;
            if (next_raw == target_raw || as_i32(next_distance) < 0x4001)
                goto landmark_check;
            person_extra.field_44 = next_raw;
            target_raw = next_raw;
            state.emit(BehaviorCallKind::effect, behavior_site::kRegionBit,
                       actor.field_72, resolve_target_type(world, target_raw));
            seen = true;
            person.flags24 |= 0x80;
            behavior_step(view, 0x10);
        }
        goto target_copy;

    landmark_check:
        if (!seen && as_i32(person_extra.field_30) > as_i32(person.field_94)) {
            person_extra.field_44 = 0;
            behavior_step(view, 0x10);
            return;
        }
        if (!seen) goto shared_tail;

    target_copy:
        {
            if (!world.resolve_target)
                throw BehaviorError("behavior world has no target resolver");
            const auto target = world.resolve_target(target_raw);
            if (!target) throw BehaviorError("behavior world cannot resolve the target pointer");
            if ((target->field_6c & 4) != 0) {
                person.flags24 |= 0x40;
                if (target->field_f8 == view.raw)
                    state.emit(BehaviorCallKind::effect, behavior_site::kMessage,
                               actor_index_from_raw(target->field_f8), 5);
            }
            person_extra.field_48 = target->position[0];
            person_extra.field_4c = target->position[1];
            person_extra.field_50 = target->position[2];
        }

    shared_tail:
        diff_x = as_i32(person_extra.field_48 - actor.position[0]);
        diff_y = as_i32(person_extra.field_4c - actor.position[1]);
        diff_z = as_i32(person_extra.field_50 - actor.position[2]);
        distance = behavior_octagon3(as_i32(abs_bits(diff_x)), as_i32(abs_bits(diff_y)),
                                     as_i32(abs_bits(diff_z)));
        if (as_i32(person_extra.field_3c) > 0x1F0) {
            const std::uint32_t draw = behavior_rng_next(state);
            person_extra.field_3c = as_u32(-as_i32(draw)) & 0x3F;
            state.emit(BehaviorCallKind::query, behavior_site::kSelector);
            const std::uint32_t selector = state.take(behavior_site::kSelector);
            if ((person.flags25 & 1) == 0) {
                if (selector == 1 || selector == 3) emit_message(view, state, 4);
                else if (selector == 2) emit_message(view, state, 5);
            }
        }
        if (as_i32(distance) <= as_i32(person.field_84) && (person.flags24 & 2) == 0)
            person_extra.field_a0 = 1;
        if (person_extra.field_a4 != 0 && person_extra.field_a0 == 0) goto waypoint;
        if (person_extra.field_28 == 0) {
            person_extra.field_28 = 2;
            person_extra.field_2c = 0;
        }
        if (person_extra.field_28 == 1 &&
            as_i32(person_extra.field_2c) > as_i32(person.field_88) &&
            behavior_rng_next(state) < 0x8000) {
            state.emit(BehaviorCallKind::query, behavior_site::kNearby);
            if (state.take(behavior_site::kNearby) != 0) {
                person_extra.field_28 = 2;
                person_extra.field_2c = 0;
                actor.field_7c = animation_id_45c880(view, state);
                actor.angle_yaw = behavior_facing_angle(diff_x, diff_y, state);
                behavior_step(view, 0x40);
                return;
            }
        }
        if (person_extra.field_28 == 2 && (person.flags24 & 2) == 0 &&
            as_i32(person_extra.field_2c) > as_i32(person.field_8c)) {
            if (as_i32(distance) > as_i32(person.field_80) || !seen) {
                if (behavior_rng_next(state) < 0x8000) {
                    person_extra.field_28 = 1;
                    person_extra.field_2c = 0;
                    actor.field_7c = animation_id_45c810(view, state);
                    actor.angle_yaw = behavior_facing_angle(diff_x, diff_y, state);
                    state.emit(BehaviorCallKind::query, behavior_site::kFacing);
                    actor.angle_yaw = state.take(behavior_site::kFacing);
                    behavior_step(view, 0x40);
                    return;
                }
            } else {
                person_extra.field_2c = 0;
            }
        }
        if (person_extra.field_28 == 1) {
            if (as_i32(distance) <= as_i32(person.field_80) && seen) {
                person_extra.field_28 = 2;
                person_extra.field_2c = 0;
                actor.field_7c = animation_id_45c880(view, state);
                actor.angle_yaw = behavior_facing_angle(diff_x, diff_y, state);
                behavior_step(view, 0x40);
                return;
            }
            actor.field_7c = animation_id_45c810(view, state);
            actor.angle_yaw = behavior_facing_angle(diff_x, diff_y, state);
            state.emit(BehaviorCallKind::query, behavior_site::kFacing);
            actor.angle_yaw = state.take(behavior_site::kFacing);
            behavior_step(view, 0x40);
            return;
        }
        if (person_extra.field_28 != 2) goto step_40;
        actor.field_7c = animation_id_45c880(view, state);
        actor.angle_yaw = behavior_facing_angle(diff_x, diff_y, state);
        if (as_i32(distance) < 0x15E0000 && behavior_rng_next(state) < 0x8000) {
            state.emit(BehaviorCallKind::query, behavior_site::kRay, distance, 0x10000);
            if (state.take(behavior_site::kRay) == 0)
                state.emit(BehaviorCallKind::effect, behavior_site::kCombatStep);
        }
        if (as_i32(distance) >= 0x1E0000) {
            if (as_i32(distance) < 0x460000) {
                behavior_step(view, 0x20);
                return;
            }
            behavior_step(view, 0x40);
            return;
        }
        behavior_step(view, 0x10);
        return;

        // ---- no-target path (45BB64) ----------------------------------------
    no_target:
        if (person.field_9c == 2) {
            person_extra.field_a0 = 0;
            if (as_i32(person_extra.field_3c) > 0x1F0) {
                const std::uint32_t draw = behavior_rng_next(state);
                person_extra.field_3c = as_u32(-as_i32(draw)) & 0x3F;
                state.emit(BehaviorCallKind::query, behavior_site::kSelector);
                const std::uint32_t selector = state.take(behavior_site::kSelector);
                if ((person.flags25 & 1) == 0) {
                    if (selector == 1 || selector == 3) emit_message(view, state, 4);
                    else if (selector == 2) emit_message(view, state, 5);
                }
            }
            if (as_i32(person_extra.field_38) >= 0xF8) {
                person_extra.field_38 = 0;
                state.emit(BehaviorCallKind::query, behavior_site::kTargetSearch);
                const std::uint32_t found = state.take(behavior_site::kTargetSearch);
                const std::uint32_t found_distance = state.inputs[state.cursor - 1].value2;
                if (found != 0 && behavior_rng_next(state) < found_distance) {
                    state.emit(BehaviorCallKind::effect, behavior_site::kRegionBit,
                               actor.field_72, resolve_target_type(world, found));
                    person_extra.field_28 = 2;
                    person_extra.field_2c = 0;
                    person_extra.field_44 = found;
                    person.flags24 &= 0xBF;
                    behavior_step(view, 8);
                    return;
                }
            }
            if (person_extra.field_a4 != 0) goto waypoint;
            state.emit(BehaviorCallKind::effect, behavior_site::kZoneStep2);
            actor.angle_yaw += 0x800000;  // original `add word [actor+0x32],0x80`
            actor.field_7c = animation_id_45c880(view, state);
            behavior_step(view, 0x40);
            return;
        }

        person_extra.field_a0 = 0;
        state.emit(BehaviorCallKind::effect, behavior_site::kZoneStep);
        if (person.field_9c == 2) {
            emit_message(view, state, 2);
            if ((person.flags25 & 1) != 0) return;
            behavior_step(view, 0x10);
            return;
        }
        {
            const std::int32_t limit = person.field_9c == 1 ? 0xF8 : 0x174;
            if (limit <= as_i32(person_extra.field_38)) {
                person_extra.field_38 = 0;
                state.emit(BehaviorCallKind::query, behavior_site::kTargetSearch);
                const std::uint32_t found = state.take(behavior_site::kTargetSearch);
                const std::uint32_t found_distance = state.inputs[state.cursor - 1].value2;
                if (found != 0 && behavior_rng_next(state) < found_distance) {
                    const std::uint32_t found_type = resolve_target_type(world, found);
                    if ((person.flags25 & 1) == 0) emit_message(view, state, 3);
                    state.emit(BehaviorCallKind::effect, behavior_site::kRegionBit,
                               actor.field_72, found_type);
                    state.emit(BehaviorCallKind::effect, behavior_site::kRegionBit2,
                               actor.field_72, found_type);
                    person.field_9c = 2;
                    person_extra.field_44 = found;
                    person.flags24 &= 0xBF;
                    state.emit(BehaviorCallKind::effect, behavior_site::kTypeSlotMark,
                               actor.field_72);
                    person_extra.field_28 = 2;
                    person_extra.field_2c = 0;
                    behavior_step(view, 0x10);
                    return;
                }
            }
        }
        if (person_extra.field_a4 != 0) goto waypoint;
        if (actor.field_7c == 0) actor.field_7c = animation_id_45c880(view, state);
        goto step_40;

        // ---- combat branch (45B92F) -----------------------------------------
    combat_branch:
        state.emit(BehaviorCallKind::effect, behavior_site::kCombatStep);
        state.emit(BehaviorCallKind::query, behavior_site::kCombatResult);
        person_extra.field_6c = state.take(behavior_site::kCombatResult);
        if (person_extra.field_6c == 0) {
            behavior_step(view, 0x10);
            return;
        }
        state.emit(BehaviorCallKind::query, behavior_site::kWeaponStep,
                   actor_extra.field_94);
        behavior_step(view, state.take(behavior_site::kWeaponStep));
        return;

    waypoint:
        behavior_waypoint_step(view, world, state);
        return;

    step_40:
        behavior_step(view, 0x40);
        return;
    }
}

void behavior_plyr(BehaviorActor& view, const BehaviorWorld& world, BehaviorState& state) {
    AiActorState& actor = view.actor;
    BehaviorActorExtra& actor_extra = view.actor_extra;

    if ((actor.field_6c & 4) != 0) return;
    if (static_cast<std::int16_t>(view.actor_extra.field_74) <= 0) {
        if (actor_extra.field_84 == 0) {
            state.emit(BehaviorCallKind::query, behavior_site::kAnimInit, 0, 0, 1);
            actor_extra.field_84 = state.take(behavior_site::kAnimInit);
        }
        state.emit(BehaviorCallKind::effect, behavior_site::kSetAnimation, actor_extra.field_84, 0);
        actor.field_7c = actor_extra.field_84;
        actor.field_6c |= 0x04;
        view.actor_extra.field_6d |= 0x02;
        state.emit(BehaviorCallKind::effect, behavior_site::kPlayerSetup);
        state.emit(BehaviorCallKind::effect, behavior_site::kGroundSnap);
    }
    if (!state.host) behavior_step(view, 0x40);
    if (view.person.field_ac != 0 && (actor.field_70 == 1 || actor.field_70 == 2)) {
        const std::uint32_t list_index = view.person.field_ac;
        if (list_index >= world.lists.size())
            throw BehaviorError("plyr marker list index is outside the behavior world");
        const BehaviorMarkerList& list = world.lists[list_index];
        for (std::uint32_t entry = 0; entry < list.count; ++entry) {
            if (entry >= list.indices.size())
                throw BehaviorError("plyr marker entry is outside the 32-entry record");
            const std::uint32_t marker_index = list.indices[entry];
            if (marker_index >= world.markers.size())
                throw BehaviorError("plyr marker index is outside the behavior world");
            const BehaviorMarker& marker = world.markers[marker_index];
            const std::int32_t dx = as_i32(marker.position[0] - actor.position[0]);
            const std::int32_t dy = as_i32(marker.position[1] - actor.position[1]);
            const std::uint32_t abs_dx = abs_bits(dx);
            const std::uint32_t abs_dy = abs_bits(dy);
            if (abs_dx > marker.radius || abs_dy > marker.radius) continue;
            const std::uint32_t spread = abs_dx > abs_dy ? abs_dx + (abs_dy >> 1)
                                                         : abs_dy + (abs_dx >> 1);
            if (spread > marker.radius) continue;
            state.emit(BehaviorCallKind::effect, behavior_site::kRegionBit3,
                       actor.field_70 == 1 ? 1u : 2u, list_index, entry);
        }
    }
    behavior_step(view, 0x40);
}

AiActorImage behavior_actor_image(const BehaviorActor& view) {
    AiActorImage image = ai_actor_image(view.actor);
    image[0x6D] = view.actor_extra.field_6d;
    write_u16(image, 0x74, view.actor_extra.field_74);
    write_u32(image, 0x84, view.actor_extra.field_84);
    write_u32(image, 0x94, view.actor_extra.field_94);
    write_u32(image, 0xF8, view.actor_extra.field_f8);
    return image;
}

AiPersonImage behavior_person_image(const BehaviorActor& view) {
    AiPersonImage image = ai_person_image(view.person);
    write_u32(image, 0x28, view.person_extra.field_28);
    write_u32(image, 0x2C, view.person_extra.field_2c);
    write_u32(image, 0x30, view.person_extra.field_30);
    write_u32(image, 0x34, view.person_extra.field_34);
    write_u32(image, 0x38, view.person_extra.field_38);
    write_u32(image, 0x3C, view.person_extra.field_3c);
    write_u32(image, 0x40, view.person_extra.field_40);
    write_u32(image, 0x44, view.person_extra.field_44);
    write_u32(image, 0x48, view.person_extra.field_48);
    write_u32(image, 0x4C, view.person_extra.field_4c);
    write_u32(image, 0x50, view.person_extra.field_50);
    write_u32(image, 0x6C, view.person_extra.field_6c);
    write_u32(image, 0xA0, view.person_extra.field_a0);
    write_u32(image, 0xA4, view.person_extra.field_a4);
    // The behavior callbacks treat +0xAC/+0xB0 as live destination fields.
    write_u32(image, 0xAC, view.person.field_ac);
    write_u32(image, 0xB0, view.person.field_b0);
    return image;
}

} // namespace dfre
