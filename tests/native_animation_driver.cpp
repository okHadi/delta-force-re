// Animation driver tests: compiled item metadata, pending action flow, instance
// state writes, typed frame event dispatch and high-byte masking.
#include "dfre/animation_driver.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
void check(bool result, const char* message) {
    if (!result) { std::cerr << message << '\n'; std::exit(1); }
}
void put(Bytes& bytes, std::size_t position, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index)
        bytes.at(position + index) = std::uint8_t(value >> (8 * index));
}

// Two bones; slot 0 has three one-tick frames: a gated event A, a high-byte
// event B and an ungated frame-end event A that loops to frame 0. Slot 5 is one
// frame with no events.
Bytes synthetic_character() {
    const std::size_t bones = 120, map = 304, table = 312, anim_a = 1332, f0 = 1352, f1 = 1424,
        f2 = 1496, anim_b = 1576, b0 = 1596, total = 1668;
    Bytes bytes(total, 0);
    put(bytes, 0, 0x31524843);
    put(bytes, 4, bones);
    put(bytes, 8, 2);
    put(bytes, 12, map);
    put(bytes, 16, 2);
    put(bytes, 20, table);
    put(bytes, 24, 2);
    for (std::size_t index = 0; index < 2; ++index) {
        put(bytes, bones + index * 92 + 16, 0);
        put(bytes, bones + index * 92 + 20, 0);
        put(bytes, bones + index * 92 + 24, 1); // 92-byte bone
        put(bytes, map + index * 4, std::uint32_t(index));
        put(bytes, bones + index * 92 + 56, 0xffffffff);
    }
    put(bytes, table, anim_a);
    put(bytes, table + 5 * 4, anim_b);
    put(bytes, anim_a, 3);
    put(bytes, anim_a + 4, 2);
    put(bytes, anim_a + 8, 1);
    put(bytes, anim_b, 1);
    put(bytes, anim_b + 4, 2);
    put(bytes, anim_b + 8, 1);
    auto frame = [&](std::size_t position, std::uint32_t stride, std::uint32_t flags,
                     std::uint8_t event_a, std::uint8_t event_b, std::size_t conditions,
                     std::size_t condition_count, std::uint8_t first, std::uint8_t second,
                     std::uint16_t duration = 1) {
        put(bytes, position, stride);
        put(bytes, position + 4, flags);
        bytes.at(position + 8) = std::uint8_t(duration);
        bytes.at(position + 10) = event_a;
        bytes.at(position + 11) = event_b;
        bytes.at(position + 0x18) = 0;
        bytes.at(position + 0x1b) = std::uint8_t(condition_count);
        put(bytes, position + 0x1c, std::uint32_t(conditions));
        put(bytes, position + 0x20, std::uint32_t(position + 60));
        put(bytes, position + 0x28, 0);
        bytes.at(position + 52) = first;
        bytes.at(position + 53) = second;
        bytes.at(position + 56) = first;
        bytes.at(position + 57) = second;
        put(bytes, position + 60, 0);
        put(bytes, position + 64, 0);
        put(bytes, position + 68, 0);
    };
    frame(f0, 72, 0x10, 0x12, 0x00, f0 + 60, 0, 1, 2, 2); // two ticks: mid-frame gate
    frame(f1, 72, 0x20, 0x00, 0x41, f1 + 60, 0, 5, 6);
    frame(f2, 0, 0x00, 0x40, 0x00, f2 + 60, 1, 9, 10);
    // Condition entry at f2+60: entry[4] = animation slot 0, entry[5] = frame byte 1.
    bytes.at(f2 + 64) = 0;
    bytes.at(f2 + 65) = 1;
    frame(b0, 0, 0x00, 0x00, 0x00, b0 + 60, 0, 1, 1);
    return bytes;
}

dfre::AnimationDefinition synthetic_definition() {
    return dfre::decode_animation_definition("First 0 0.0 override\r\nSecond 5 1.0\r\n");
}

std::vector<dfre::AnimationCompiledAction> synthetic_compiled() {
    return {
        {"First", 1, 3, 1,32768,78643},
        {"Second", 2, 0, 2},
        {"Unknown", 3, 0, 0}, // no ANM entry: zero record
    };
}

void test_metadata() {
    const auto duplicates=dfre::decode_animation_definition("First 5 1.5 override\r\nFirst 7\r\n");
    const auto repeated=dfre::build_animation_driver_actions(duplicates,synthetic_compiled());
    check(repeated[0].animation_number==7 && repeated[0].velocity_per_tick==3276 && repeated[0].override_flag,"duplicate ANM writes changed");
    const auto& original=dfre::original_animation_compiled_actions();
    check(original.size()==195 && original.front().name=="Null" && original.back().item_index==255,"original compiled row count changed");

    const auto definition = synthetic_definition();
    const auto actions = dfre::build_animation_driver_actions(definition, synthetic_compiled());
    check(actions.size() == 3, "driver action count changed");
    check(actions[0].item_index == 1 && actions[0].animation_number == 0 &&
        actions[0].override_flag && actions[0].direction == 3 && actions[0].tag == 1,
        "First metadata changed");
    check(actions[0].weight_10==32768 && actions[0].aim_14==78643,"compiled aim/weight changed");
    check(actions[2].item_index==3 && actions[2].weight_10==0 && actions[2].direction==0 && actions[2].tag==0,"absent ANM row is not zero");
    check(actions[1].item_index == 2 && actions[1].animation_number == 5 &&
        !actions[1].override_flag && actions[1].velocity_per_tick == (65536 / 30) &&
        actions[1].tag == 2, "Second metadata changed");
}

void test_dispatch_table() {
    const auto check_row = [](std::uint8_t event, dfre::AnimationEventTarget target,
                              std::uint8_t value, std::uint8_t value2) {
        const auto dispatch = dfre::animation_event_dispatch(event);
        if (dispatch.target != target || dispatch.value != value || dispatch.value2 != value2)
            throw std::runtime_error("event dispatch changed");
    };
    for (int event = 0; event < 0x100; ++event) {
        const auto dispatch = dfre::animation_event_dispatch(std::uint8_t(event));
        const bool expected_43f5a0 = event >= 0x12 && event <= 0x16;
        const bool expected_45e0f0 = event >= 0x40 && event <= 0x43;
        if (expected_43f5a0)
            check_row(std::uint8_t(event), dfre::AnimationEventTarget::call_43f5a0,
                      std::uint8_t(7 + event - 0x12), 0);
        else if (expected_45e0f0)
            check_row(std::uint8_t(event), dfre::AnimationEventTarget::call_45e0f0,
                      std::uint8_t(1 - ((event >> 1) & 1)), std::uint8_t(1 - (event & 1)));
        else
            check_row(std::uint8_t(event), dfre::AnimationEventTarget::none, 0, 0);
        (void)dispatch;
    }
    check(dfre::animation_event_dispatch(0x17).target == dfre::AnimationEventTarget::none,
        "unknown event dispatched");
}

void test_driver() {
    const auto model = dfre::prepare_character_runtime(dfre::decode_character(synthetic_character()));
    dfre::AnimationController controller(model);
    const auto definition = synthetic_definition();
    dfre::AnimationDriver driver(controller,
        dfre::build_animation_driver_actions(definition, synthetic_compiled()));
    check(driver.current_action() == 0 && controller.animation_slot() == 0,
        "driver init changed");

    {
        dfre::AnimationController live(model);
        dfre::AnimationDriver joined(live,dfre::build_animation_driver_actions(definition,synthetic_compiled()));
        dfre::AnimationDriverState input;input.pending_action=1;input.update_flags=1;
        unsigned calls=0;
        const auto updated=joined.update(input,[&](dfre::AnimationDriverState& current){
            ++calls;
            check(joined.current_action()==1 && live.frame_index()==0,"tag effect order changed");
            current.heading_raw=0x400;
        });
        check(calls==1 && updated.ticked && live.accumulator()==8,"tag effect inputs did not reach the tick");
        joined.update(input,[&](dfre::AnimationDriverState&){++calls;});
        check(calls==1,"unchanged action repeated its tag effect");
    }

    dfre::AnimationDriverState state;
    state.update_flags = 1;
    state.pending_action = 1;
    state.heading_raw = 0x80;
    auto result = driver.update(state);
    check(result.action_started && result.action_index == 1, "pending action was not started");
    check(result.animation_slot == 0 && result.override_flag, "current item fields changed");
    check(result.action_direction == (3u << 29), "item+0 direction packing changed");
    check(result.move_heading == std::int32_t(0x80 + (3u << 29)), "move heading changed");
    check(result.action_tag_effect && result.ticked, "item tag effect changed");
    check(result.events == 0x12, "gated event A changed");
    check(result.dispatch == dfre::AnimationEventDispatch{
        dfre::AnimationEventTarget::call_43f5a0, 7, 0}, "event A dispatch changed");
    check(controller.random_a() == 0 && controller.random_b() == 0, "random selector was not cleared");
    check(controller.accumulator() == (0x80 >> 7), "instance heading write changed");

    result = driver.update(state);
    check(!result.action_started && result.action_index == 1, "pending action restarted");
    check(result.events == 0x12 && result.dispatch.target == dfre::AnimationEventTarget::call_43f5a0,
        "frame end repeated the gated event");
    check(controller.frame_index() == 1, "frame did not advance to 1");

    result = driver.update(state);
    check(result.events == 0x4100, "high byte event was not returned");
    check(result.dispatch.target == dfre::AnimationEventTarget::none,
        "high byte event dispatched without masking");
    check(controller.frame_index() == 2, "frame did not advance to 2");

    result = driver.update(state);
    check(result.events == 0x40, "frame end event A changed");
    check(result.dispatch == dfre::AnimationEventDispatch{
        dfre::AnimationEventTarget::call_45e0f0, 1, 1}, "event 0x40 dispatch changed");
    check(result.tick.action_changed, "frame loop change missing");
    check(controller.frame_index() == 0, "frame loop did not return to frame 0");

    state.pending_action = 2;
    result = driver.update(state);
    check(result.action_started && result.animation_slot == 5, "second action did not start");
    check(!result.action_tag_effect, "tag bit 1 triggered the tag effect");
    check(result.action_direction == 0, "second action direction changed");

    result = driver.update(state);
    check(controller.frame_index() == 0 && controller.animation_slot() == 0,
        "single frame animation did not end at slot 0");

    // A reserved item index plays item+0x04 = 0, which is animation slot 0.
    state.pending_action = 8;
    result = driver.update(state);
    check(result.action_started && result.animation_slot == 0 && result.action_direction == 0,
        "reserved item index did not play slot 0");

    // update_flags bit 0 gates the tick and the event dispatch.
    const auto frame_before = controller.frame_index();
    state.update_flags = 2;
    result = driver.update(state);
    check(!result.ticked && result.events == 0 &&
        result.dispatch.target == dfre::AnimationEventTarget::none, "tick gate changed");
    check(controller.frame_index() == frame_before, "tick gate still advanced the frame");
}

void test_event_target_names() {
    // The probe prints target names; keep the enum values stable for the checker.
    check(dfre::animation_event_dispatch(0x12).target == dfre::AnimationEventTarget::call_43f5a0 &&
        dfre::animation_event_dispatch(0x40).target == dfre::AnimationEventTarget::call_45e0f0,
        "event target enum changed");
}
} // namespace

int main() {
    try {
        test_metadata();
        test_dispatch_table();
        test_driver();
        test_event_target_names();
        std::cout << "animation driver tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
