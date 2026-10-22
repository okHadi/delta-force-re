// Animation controller and ANM definition tests. Synthetic CHR bytes mirror the
// observed CHR1 layout; optional CHR and ANM paths add real-data checks.
#include "dfre/animation.hpp"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
void check(bool result, const char* message) {
    if (!result) { std::cerr << message << '\n'; std::exit(1); }
}
void put(Bytes& bytes, std::size_t position, std::uint32_t value) {
    for (int index = 0; index < 4; ++index) bytes.at(position + index) = std::uint8_t(value >> (8 * index));
}
std::uint32_t get(const Bytes& bytes, std::size_t position) {
    return std::uint32_t(bytes.at(position)) | (std::uint32_t(bytes.at(position + 1)) << 8) |
        (std::uint32_t(bytes.at(position + 2)) << 16) | (std::uint32_t(bytes.at(position + 3)) << 24);
}

// Two bones, two animations: slot 0 has a two-frame loop with a frame event and
// timed interpolation; slot 5 ends at slot 0 frame 0.
Bytes synthetic_character() {
    const std::size_t bones = 120, map = 304, table = 312, anim_a = 1332, a0 = 1352, a1 = 1424,
        anim_b = 1504, b0 = 1524, b1 = 1596, total = 1668;
    Bytes bytes(total, 0);
    put(bytes, 0, 0x31524843);
    put(bytes, 4, bones);
    put(bytes, 8, 2);
    put(bytes, 12, map);
    put(bytes, 16, 2);
    put(bytes, 20, table);
    put(bytes, 24, 2);
    for (std::size_t index = 0; index < 2; ++index) {
        put(bytes, bones + index * 92, 0);
        put(bytes, bones + index * 92 + 16, 0);
        put(bytes, bones + index * 92 + 20, 0);
        put(bytes, bones + index * 92 + 24, 1); // 92-byte bone
        put(bytes, map + index * 4, std::uint32_t(index));
        put(bytes, bones + index * 92 + 56, 0xffffffff); // root bone
    }
    put(bytes, table, anim_a);
    put(bytes, table + 5 * 4, anim_b);
    auto animation = [&](std::size_t position, std::uint32_t correction) {
        put(bytes, position, 2);
        put(bytes, position + 4, 2);
        put(bytes, position + 8, 1); // one root bone
        put(bytes, position + 12, correction);
    };
    animation(anim_a, 0);
    animation(anim_b, 0);
    auto frame = [&](std::size_t position, std::uint32_t stride, std::uint32_t flags,
                     std::uint16_t duration, std::uint8_t event_a, std::uint8_t event_b,
                     std::int32_t motion_x, std::int32_t motion_y, std::int32_t motion_z,
                     std::size_t root_table, std::uint8_t root_index, std::size_t conditions,
                     std::size_t condition_count, std::uint8_t angle0, std::uint8_t angle1,
                     std::uint8_t angle2, std::uint8_t angle3) {
        put(bytes, position, stride);
        put(bytes, position + 4, flags);
        bytes.at(position + 8) = std::uint8_t(duration);
        bytes.at(position + 9) = std::uint8_t(duration >> 8);
        bytes.at(position + 10) = event_a;
        bytes.at(position + 11) = event_b;
        put(bytes, position + 12, std::uint32_t(motion_x));
        put(bytes, position + 16, std::uint32_t(motion_y));
        put(bytes, position + 20, std::uint32_t(motion_z));
        bytes.at(position + 0x18) = root_index;
        bytes.at(position + 0x1b) = std::uint8_t(condition_count);
        put(bytes, position + 0x1c, std::uint32_t(conditions));
        put(bytes, position + 0x20, std::uint32_t(root_table));
        put(bytes, position + 0x24, 0);
        put(bytes, position + 0x28, 0);
        const std::size_t angles = position + 52;
        bytes.at(angles) = angle0;
        bytes.at(angles + 1) = angle1;
        bytes.at(angles + 4) = angle2;
        bytes.at(angles + 5) = angle3;
    };
    // Slot 0 frame 0: two ticks, event 0x21, byte 0x77 without its gate flag.
    frame(a0, 72, 0x00000011, 2, 0x21, 0x77, 10, 20, 30, a0 + 60, 0, a0 + 60, 0, 1, 2, 3, 4);
    put(bytes, a0 + 60, 100);
    put(bytes, a0 + 64, 200);
    put(bytes, a0 + 68, 300);
    // Slot 0 frame 1: one tick, event 0x4200, always loops to frame 1.
    frame(a1, 0, 0x00000020, 1, 0, 0x42, 0, 0, 0, a1 + 68, 0, a1 + 60, 1, 5, 6, 7, 8);
    put(bytes, a1 + 60, 0);
    put(bytes, a1 + 61, 0);
    put(bytes, a1 + 62, 0);
    put(bytes, a1 + 63, 0);
    put(bytes, a1 + 64, 0); // action byte 0 (slot 0)
    put(bytes, a1 + 65, 2); // frame byte 2 -> frame index 1
    put(bytes, a1 + 66, 0);
    put(bytes, a1 + 67, 0);
    put(bytes, a1 + 68, 110);
    put(bytes, a1 + 72, 210);
    put(bytes, a1 + 76, 310);
    // Slot 5: two one-tick frames that end at slot 0 frame 0.
    frame(b0, 72, 0x00000000, 1, 0, 0, 1, 2, 3, b0 + 60, 0, b0 + 60, 0, 10, 11, 12, 13);
    put(bytes, b0 + 60, 1);
    put(bytes, b0 + 64, 2);
    put(bytes, b0 + 68, 3);
    frame(b1, 0, 0x00000000, 1, 0, 0, 4, 5, 6, b1 + 60, 0, b1 + 60, 0, 20, 21, 22, 23);
    put(bytes, b1 + 60, 4);
    put(bytes, b1 + 64, 5);
    put(bytes, b1 + 68, 6);
    check(get(bytes, a0) == 72 && get(bytes, a1) == 0 && get(bytes, anim_b) == 2, "synthetic layout");
    return bytes;
}

bool same_pose(const std::vector<dfre::CharacterBonePose>& left,
    const std::vector<dfre::CharacterBonePose>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index)
        if (left[index].angles != right[index].angles || left[index].scale != right[index].scale)
            return false;
    return true;
}

std::vector<dfre::CharacterBonePose> frame_pose(const dfre::CharacterModel& model,
    std::uint32_t slot, std::size_t frame) {
    for (const auto& animation : model.animations) if (animation.slot == slot)
        return dfre::character_frame_pose(animation.frames.at(frame));
    throw std::runtime_error("synthetic animation is missing");
}

void test_definition() {
    const std::string text =
        "// comment line\r\n"
        "\r\n"
        "Null      0   0.0   override\r\n"
        "Standing_1HW  2  0.0  // Looping Stance\r\n"
        "Walking0_1HW\t79,1.0 extra\r\n"
        "\"Quoted Action\" 42 2.5 override\r\n"
        "Bad\r\n"
        "  Standing_KNIFE  1  0.0\r\n";
    const auto definition = dfre::decode_animation_definition(text);
    check(definition.actions.size() == 5, "ANM action count changed");
    check(definition.actions[0].name == "Null" && definition.actions[0].number == 0 &&
        definition.actions[0].override_flag, "ANM Null entry changed");
    check(definition.actions[1].name == "Standing_1HW" && definition.actions[1].number == 2 &&
        !definition.actions[1].override_flag && definition.actions[1].velocity == 0.0,
        "ANM Standing_1HW entry changed");
    check(definition.actions[2].name == "Walking0_1HW" && definition.actions[2].number == 79 &&
        definition.actions[2].velocity == 1.0 && !definition.actions[2].override_flag,
        "ANM comma or tab token changed");
    check(definition.actions[3].name == "Quoted Action" && definition.actions[3].number == 42 &&
        definition.actions[3].velocity == 2.5 && definition.actions[3].override_flag,
        "ANM quoted token changed");
    check(definition.actions[4].name == "Standing_KNIFE" && definition.actions[4].number == 1,
        "ANM leading space changed");
    check(definition.actions[2].velocity_per_tick == ((1 * 65536) / 30), "ANM per-tick velocity changed");
    check(definition.find("Walking0_1HW") == &definition.actions[2], "ANM lookup changed");
    check(definition.find("Missing") == nullptr, "ANM lookup accepted a missing name");
}

void test_controller() {
    const auto model = dfre::prepare_character_runtime(dfre::decode_character(synthetic_character()));
    dfre::AnimationController controller(model);
    check(controller.has_animation() && controller.animation_slot() == 0 && controller.frame_index() == 0,
        "controller init selection changed");
    check(controller.frame_ticks() == 2, "controller init frame duration changed");
    check(controller.frame_mask() == 0x11, "controller init frame mask changed");
    check(controller.root_count() == 1 && controller.root_positions().size() == 8,
        "current root count must differ from storage capacity");
    controller.set_condition_state(0xffffffff);
    check(controller.condition_current() == 0xffffffff, "condition state write changed");
    controller.set_condition_state(0);
    const auto seeded = frame_pose(model, 0, 0);
    check(same_pose(controller.pose(), seeded), "controller init pose is not the frame pose");
    check(controller.root_delta()[0] == std::array<std::int32_t, 3>{50, 100, 150},
        "timed root delta changed");
    check(controller.pose_delta()[0].angles == std::array<std::int32_t, 3>{0, 0, 0},
        "seeded frame delta changed");

    dfre::AnimationInputs inputs;
    auto result = controller.tick(inputs);
    check(result.events == 0x21 && !result.action_changed, "mid-frame event gating changed");
    check(controller.frame_index() == 0 && controller.frame_ticks() == 1, "mid-frame countdown changed");
    check(same_pose(controller.pose(), seeded), "zero delta moved the pose");
    result = controller.tick(inputs);
    check(result.events == 0x7721 && controller.frame_index() == 1 && controller.frame_ticks() == 1,
        "frame advance changed");
    check(controller.frame_mask() == 0x20, "frame mask did not follow frame advance");
    check(controller.pose_delta()[0].angles == std::array<std::int32_t, 3>{4 << 17, 4 << 17, 0} &&
        controller.pose_delta()[1].angles == std::array<std::int32_t, 3>{4 << 17, 4 << 17, 0},
        "single-tick angle delta changed");
    result = controller.tick(inputs);
    check(result.events == 0x4200 && result.action_changed, "frame loop event or change changed");
    check(same_pose(controller.pose(), frame_pose(model, 0, 1)), "pose did not reach frame 1");
    check(controller.frame_index() == 1, "frame loop did not stay on frame 1");
    check(controller.condition_current() == 0, "condition state was not cleared");

    check(controller.select_action(5, 0), "select slot 5 failed");
    check(controller.frame_ticks() == 1, "slot 5 frame duration changed");
    check(same_pose(controller.pose(), frame_pose(model, 0, 1)), "select moved the pose");
    check(controller.root_positions()[0] == std::array<std::int32_t, 3>{110, 210, 310},
        "root table copy changed");
    check(controller.root_delta()[0] == std::array<std::int32_t, 3>{-109, -208, -307},
        "slot 5 root delta changed");
    check(controller.motion() == std::array<std::int32_t, 3>{109, 210, 309},
        "rotated frame motion changed");
    check(controller.tick(inputs).events == 0 && controller.frame_index() == 1,
        "slot 5 frame advance changed");
    check(same_pose(controller.pose(), frame_pose(model, 5, 0)), "slot 5 pose changed");
    result = controller.tick(inputs);
    check(!result.action_changed && controller.animation_slot() == 0 && controller.frame_index() == 0,
        "animation end did not select slot 0 frame 0");

    const auto slot = controller.animation_slot();
    check(!controller.select_action(7, 0), "select accepted an empty slot");
    check(controller.animation_slot() == slot, "failed select changed the animation");

    // Condition opcodes verified against the original x86.
    dfre::AnimationConditionState state;
    check(dfre::check_animation_condition(0x00, false, state), "condition 0 changed");
    state.switch_flag = 1;
    check(dfre::check_animation_condition(0x22, false, state), "condition 0x22 changed");
    state.switch_flag = 0;
    state.aim_a = 1;
    check(dfre::check_animation_condition(0x25, false, state), "condition 0x25 changed");
    state.aim_a = 0;
    state.byte79 = 0;
    check(dfre::check_animation_condition(0x27, false, state), "condition 0x27 changed");
    check(!dfre::check_animation_condition(0x28, true, state), "condition 0x28 changed");
    state.stance = 3;
    check(dfre::check_animation_condition(0x1b, false, state), "condition 0x1b changed");
    state.stance = 0;
    state.pitch = std::uint8_t(0xf0);
    check(!dfre::check_animation_condition(0x38, false, state), "condition 0x38 changed");
    state.pitch = 20;
    check(dfre::check_animation_condition(0x39, false, state), "condition 0x39 changed");
    check(dfre::animation_random_selector(0, 0, 0, 0) == 0, "random selector zero changed");
    check(dfre::animation_random_selector(0x100000, 0x100000, 3, 7) == 0x3f,
        "random selector bits changed");
    check(dfre::animation_random_selector(-1, -1, 1, 2) == 0x0a,
        "random selector negative sum changed");
}

void test_real_data(const char* chr_path, const char* anm_path) {
    std::ifstream chr_file(chr_path, std::ios::binary);
    check(bool(chr_file), "cannot open character");
    const Bytes chr_bytes((std::istreambuf_iterator<char>(chr_file)), {});
    const auto model = dfre::prepare_character_runtime(dfre::decode_character(chr_bytes));
    check(model.animations.size() == 204, "PLAYER01.CHR animation count changed");
    check(model.bone_map.size() == 15, "PLAYER01.CHR bone map changed");
    std::ifstream anm_file(anm_path, std::ios::binary);
    check(bool(anm_file), "cannot open animation definition");
    const std::string text((std::istreambuf_iterator<char>(anm_file)), {});
    const auto definition = dfre::decode_animation_definition(text);
    check(definition.actions.size() == 195, "PLAYANIM.ANM action count changed");
    const auto* standing = definition.find("Standing_1HW");
    const auto* walking = definition.find("Walking0_1HW");
    const auto* prone = definition.find("Prone_1HW");
    check(standing && standing->number == 2 && standing->velocity == 0.0, "Standing_1HW entry changed");
    check(walking && walking->number == 79 && walking->velocity == 1.0 &&
        walking->velocity_per_tick == ((65536) / 30), "Walking0_1HW entry changed");
    check(prone && prone->number == 15 && prone->override_flag, "Prone_1HW entry changed");
    dfre::AnimationController controller(model);
    check(controller.animation_slot() == 0 && controller.frame_index() == 0, "PLAYER01 init changed");
    check(controller.select_action(definition, "Walking0_1HW"), "Walking0_1HW select failed");
    check(controller.animation_slot() == 79 && controller.frame_ticks() == 5, "walking frame timing changed");
    dfre::AnimationInputs inputs;
    auto result = controller.tick(inputs);
    check(controller.frame_index() == 0 && controller.frame_ticks() == 4, "walking tick timing changed");
    std::size_t loops = 0;
    for (int tick = 0; tick < 200; ++tick) {
        result = controller.tick(inputs);
        if (result.action_changed) ++loops;
    }
    check(loops >= 2, "walking animation did not loop");
    check(controller.animation_slot() == 79, "walking animation left its slot");
    check(controller.select_action(definition, "Standing_SLW") && controller.animation_slot() == 176,
        "Standing_SLW select changed");
    check(model.animations.front().frames.front().packed_angles.size() == 15,
        "PLAYER01 frame pose count changed");
}

void test_headers() {
    // The status probe matches 0x42cd90: duration minus countdown.
    const auto model = dfre::prepare_character_runtime(dfre::decode_character(synthetic_character()));
    dfre::AnimationController controller(model);
    auto status = controller.status();
    check(status.animation_slot == 0 && status.frame_index == 0 && status.frame_ticks_elapsed == 0,
        "controller status changed");
    dfre::AnimationInputs inputs;
    controller.tick(inputs);
    status = controller.status();
    check(status.frame_ticks_elapsed == 1, "controller status elapsed changed");
}
} // namespace

int main(int argc, char** argv) {
    try {
        test_definition();
        test_controller();
        test_headers();
        if (argc >= 3) test_real_data(argv[1], argv[2]);
        std::cout << "animation controller tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
