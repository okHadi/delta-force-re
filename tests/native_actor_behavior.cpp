#include "dfre/actor_behavior.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void check(bool yes, const char* message) {
    if (!yes) throw std::runtime_error(message);
}

struct Fixture {
    dfre::AiActorState actor;
    dfre::AiPersonState person;
    dfre::BehaviorActorRef ref{3, 0};
    std::uint32_t raw = 0x7CCA80;
    dfre::BehaviorActorExtra actor_extra;
    dfre::BehaviorPersonExtra person_extra;
    dfre::BehaviorWorld world;
    dfre::BehaviorState state;

    Fixture() {
        actor.active = true;
        actor.definition_index = 1;
        actor_extra.field_74 = 1;
        actor.field_70 = 0x0004;  // live type 4 (short 4)
        person.flags24 = 0;
        state.rng = 0x12345678;
    }

    dfre::BehaviorActor view() {
        return {ref, raw, actor, person, actor_extra, person_extra};
    }
};

std::uint32_t find_action(const dfre::BehaviorState& state, std::uint32_t site) {
    for (const auto& action : state.actions)
        if (action.site == site) return action.args[0] + 1;
    return 0;
}

const dfre::BehaviorAction* action_at(const dfre::BehaviorState& state, std::size_t index) {
    return index < state.actions.size() ? &state.actions[index] : nullptr;
}
} // namespace

int main() {
    try {
        {
            Fixture fixture;
            fixture.state.on_effect=[&](const dfre::BehaviorAction& action){fixture.actor.field_fc=action.args[0];};
            fixture.state.on_query=[&](const dfre::BehaviorAction& action) {
                return dfre::BehaviorInput{action.site,fixture.actor.field_fc,99};
            };
            fixture.state.emit(dfre::BehaviorCallKind::effect,1,42);
            check(fixture.actor.field_fc==42,"effect call was delayed");
            fixture.state.emit(dfre::BehaviorCallKind::query,2);
            check(fixture.state.take(2)==42 && fixture.state.inputs.back().value2==99,
                "query did not see the current actor state");
            check(fixture.state.cursor==1,"live query consumed more than one answer");
        }
        // null writes the sentinel delay; org0 is a no-op.
        {
            Fixture fixture;
            auto view = fixture.view();
            dfre::behavior_null(view);
            check(view.actor.field_fc == 0x1000000, "NullAI delay changed");
            fixture.actor.field_fc = 7;
            auto second = fixture.view();
            dfre::behavior_org0(second);
            check(fixture.actor.field_fc == 7 && fixture.state.actions.empty(),
                  "org0 callback changed state");
        }

        // 45B3A0 step writes.
        {
            Fixture fixture;
            fixture.person.flags24 = 0x80;
            fixture.person_extra.field_2c = 10;
            auto view = fixture.view();
            dfre::behavior_step(view, 0x40);
            check(fixture.actor.field_fc == 0x40, "Step delay changed");
            check(fixture.person_extra.field_2c == 0x4A && fixture.person_extra.field_38 == 0x40 &&
                      fixture.person_extra.field_3c == 0x40 && fixture.person_extra.field_40 == 0x40,
                  "Step counters changed");
            check(fixture.person_extra.field_34 == 0x40 && fixture.person_extra.field_30 == 0,
                  "Step flag-0x80 arm changed");
            check(fixture.actor_extra.field_6d == 1, "Step flag byte changed");
            Fixture plain;
            plain.person_extra.field_34 = 5;
            auto plain_view = plain.view();
            dfre::behavior_step(plain_view, 8);
            check(plain.person_extra.field_34 == 0 && plain.person_extra.field_30 == 8,
                  "Step non-0x80 arm changed");
        }

        // 477860 LCG draws from a known seed.
        {
            Fixture fixture;
            fixture.state.rng = 1;
            const std::uint32_t first = dfre::behavior_rng_next(fixture.state);
            const std::uint32_t second = dfre::behavior_rng_next(fixture.state);
            check(first == 32785 && second == 33041, "RNG sequence changed");
        }

        // 477670 octagon distances.
        {
            check(dfre::behavior_octagon3(100, 0, 0) == 100, "Octagon max changed");
            check(dfre::behavior_octagon3(100, 100, 0) == 100 + 34, "Octagon mid scaled changed");
            check(dfre::behavior_octagon3(100, 50, 20) == 100 + 17 + 5, "Octagon min changed");
            check(dfre::behavior_octagon2(0x140064, 0) == 0x140064, "2D octagon x changed");
            check(dfre::behavior_octagon2(0, 0x140064) == 0x140064, "2D octagon y changed");
        }

        // 477770 integer paths and the FPU input path.
        {
            Fixture fixture;
            check(dfre::behavior_facing_angle(0x400000, 10, fixture.state) == 0,
                  "Facing +x changed");
            check(dfre::behavior_facing_angle(-0x400000, 10, fixture.state) == 0x80000000u,
                  "Facing -x changed");
            check(dfre::behavior_facing_angle(10, 0x400000, fixture.state) == 0x40000000u,
                  "Facing +y changed");
            check(dfre::behavior_facing_angle(10, -0x400000, fixture.state) == 0xC0000000u,
                  "Facing -y changed");
            check(fixture.state.actions.empty() && fixture.state.cursor == 0,
                  "Facing integer path consumed input");
            fixture.state.inputs = {{dfre::behavior_site::kAngle, 0x1234, 0}};
            const std::uint32_t angle = dfre::behavior_facing_angle(1000, 900, fixture.state);
            check(angle == 0x1234 && fixture.state.cursor == 1, "Facing FPU path changed");
            check(fixture.state.actions.size() == 1 &&
                      fixture.state.actions[0].site == dfre::behavior_site::kAngle &&
                      fixture.state.actions[0].args[0] == 1000 &&
                      fixture.state.actions[0].args[1] == 900,
                  "Facing FPU action changed");
        }

        // 45C6A0 marker query, state 1 and state 3.
        {
            Fixture fixture;
            fixture.world.markers.push_back({77, {0x1000000, 0x2000, 0}, 0x200, 0x1000});
            fixture.world.lists.push_back({0, 0, {}});   // original list slot 0
            fixture.world.lists.push_back({0, 1, {0}});  // original list slot 1
            fixture.person_extra.field_a4 = 1;
            fixture.person.field_ac = 1;
            fixture.person.field_b0 = 0;
            auto view = fixture.view();
            dfre::BehaviorMarkerTarget target;
            check(dfre::behavior_marker_query(view, fixture.world, fixture.state, target),
                  "Marker query failed");
            check(target.distance == 0x1001000 && target.radius == 0x1000 && target.yaw == 0x200 &&
                      target.angle == 0,
                  "Marker query outputs changed");
            check(fixture.state.actions.empty(), "Marker query angle path changed");

            fixture.person_extra.field_a4 = 3;
            fixture.person.field_b0 = 77;
            fixture.world.resolve_marker = [](std::uint32_t) { return std::optional<std::size_t>(0); };
            check(dfre::behavior_marker_query(view, fixture.world, fixture.state, target),
                  "Marker direct query failed");
            check(target.radius == 0x100000, "Marker direct radius changed");
            fixture.person.field_b0 = 76;
            check(!dfre::behavior_marker_query(view, fixture.world, fixture.state, target),
                  "Marker object-id gate changed");
            fixture.person_extra.field_a4 = 2;
            check(!dfre::behavior_marker_query(view, fixture.world, fixture.state, target),
                  "Unknown destination state queried");
        }

        // plyr non-host setup and marker messages.
        {
            Fixture fixture;
            fixture.state.host = false;
            fixture.state.inputs = {{dfre::behavior_site::kAnimInit, 0x14, 0}};
            fixture.actor_extra.field_74 = 0;
            fixture.actor.field_70 = 1;
            fixture.person.field_ac = 1;
            fixture.world.markers.push_back({0, {100, 0, 0}, 0, 1000});
            fixture.world.lists.push_back({0, 0, {}});
            fixture.world.lists.push_back({0, 1, {0}});
            auto view = fixture.view();
            dfre::behavior_plyr(view, fixture.world, fixture.state);
            check(fixture.actor_extra.field_84 == 0x14 && fixture.actor.field_7c == 0x14,
                  "plyr animation init changed");
            check(fixture.actor.field_6c == 0x04 && fixture.actor_extra.field_6d == 0x03,
                  "plyr flag writes changed");
            check(fixture.state.cursor == 1, "plyr input cursor changed");
            check(fixture.state.actions.size() == 5, "plyr action count changed");
            check(fixture.state.actions[0].site == dfre::behavior_site::kAnimInit &&
                      fixture.state.actions[1].site == dfre::behavior_site::kSetAnimation &&
                      fixture.state.actions[2].site == dfre::behavior_site::kPlayerSetup &&
                      fixture.state.actions[3].site == dfre::behavior_site::kGroundSnap &&
                      fixture.state.actions[4].site == dfre::behavior_site::kRegionBit3 &&
                      fixture.state.actions[4].args[0] == 1 && fixture.state.actions[4].args[1] == 1 &&
                      fixture.state.actions[4].args[2] == 0,
                  "plyr action order changed");
            check(fixture.actor.field_fc == 0x40, "plyr final step changed");
        }

        // org1: actor flag gate, delay branch, and the non-host writes.
        {
            Fixture fixture;
            fixture.state.host = false;
            auto view = fixture.view();
            fixture.actor_extra.field_74 = 7;
            dfre::behavior_org1(view, fixture.world, fixture.state, 4);
            check(fixture.actor.field_fc == 0x400, "org1 non-host delay changed");
            Fixture flagged;
            flagged.actor.field_6c = 4;
            auto flagged_view = flagged.view();
            dfre::behavior_org1(flagged_view, flagged.world, flagged.state, 0);
            check(flagged.actor.field_fc == 0x400 && flagged.person_extra.field_2c == 0x400,
                  "org1 flagged step changed");
        }

        // org1: combat branch consumes the combat result and weapon step.
        {
            Fixture fixture;
            fixture.person_extra.field_6c = 1;
            fixture.person.field_64 = 3;
            fixture.state.inputs = {
                {dfre::behavior_site::kCombatResult, 1, 0},
                {dfre::behavior_site::kWeaponStep, 0x55, 0},
            };
            auto view = fixture.view();
            dfre::behavior_org1(view, fixture.world, fixture.state, 0);
            check(fixture.state.actions.size() == 3, "org1 combat action count changed");
            check(fixture.state.actions[0].site == dfre::behavior_site::kCombatStep &&
                      fixture.state.actions[1].site == dfre::behavior_site::kCombatResult &&
                      fixture.state.actions[2].site == dfre::behavior_site::kWeaponStep &&
                      fixture.state.actions[2].args[0] == 3,
                  "org1 combat action order changed");
            check(fixture.actor.field_fc == 0x55, "org1 combat step value changed");
        }

        // org1: no-target state-2 path with a failed target search.
        {
            Fixture fixture;
            fixture.person.field_9c = 2;
            fixture.person_extra.field_3c = 0x200;  // triggers the selector block
            fixture.person_extra.field_38 = 0x100;  // triggers the search block
            fixture.state.inputs = {
                {dfre::behavior_site::kSelector, 2, 0},
                {dfre::behavior_site::kTargetSearch, 0, 0},
                {dfre::behavior_site::kWeaponFlags880, 0, 0},
            };
            auto view = fixture.view();
            dfre::behavior_org1(view, fixture.world, fixture.state, 0);
            check(fixture.state.cursor == 3, "org1 no-target inputs changed");
            const auto* first = action_at(fixture.state, 0);
            check(first && first->site == dfre::behavior_site::kSelector &&
                      first->kind == dfre::BehaviorCallKind::query,
                  "org1 selector action changed");
            check(fixture.person_extra.field_38 == 0x40, "org1 search timer changed");
            check(fixture.actor.angle_yaw == 0x800000 && fixture.actor.field_7c == 0x64,
                  "org1 no-target tail changed");
            // state != 2 path with the 0x174 limit and a successful search.
            Fixture second;
            second.person.field_9c = 0;
            second.actor.field_72 = 4;
            second.person_extra.field_38 = 0x200;
            second.state.inputs = {
                {dfre::behavior_site::kTargetSearch, 0x2006000u, 28827},
            };
            second.world.resolve_target = [](std::uint32_t) {
                dfre::BehaviorTargetView target;
                target.field_70 = 0x00040000;  // type 4
                return std::optional<dfre::BehaviorTargetView>(target);
            };
            auto second_view = second.view();
            dfre::behavior_org1(second_view, second.world, second.state, 0);
            check(second.person.field_9c == 2 && second.person_extra.field_44 == 0x2006000u,
                  "org1 acquisition state changed");
            check(find_action(second.state, dfre::behavior_site::kTypeSlotMark) == 4 + 1,
                  "org1 type slot mark changed");
            check(second.state.cursor == 1 && second.state.actions.size() == 6,
                  "org1 acquisition actions changed");
            check(second.state.actions[0].site == dfre::behavior_site::kZoneStep &&
                      second.state.actions[1].site == dfre::behavior_site::kTargetSearch &&
                      second.state.actions[2].site == dfre::behavior_site::kMessage &&
                      second.state.actions[5].site == dfre::behavior_site::kTypeSlotMark,
                  "org1 acquisition order changed");
        }

        std::cout << "native_actor_behavior checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
