// Original animation controller probe: ANM parsing and scripted controller
// traces for tools/check_animation_original.py.
#include "dfre/animation.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string read_text(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error(std::string("cannot open ")+path);
    return std::string(std::istreambuf_iterator<char>(file), {});
}
std::vector<std::uint8_t> read_bytes(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error(std::string("cannot open ")+path);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file), {});
}
void print_state(const dfre::AnimationController& controller, bool ok, int changed,
    const std::string& prefix = {}) {
    const auto status = controller.status();
    std::ostringstream out;
    out << "{" << prefix << "\"ok\":" << ok << ",\"changed\":" << changed
        << ",\"slot\":" << status.animation_slot << ",\"frame\":" << status.frame_index
        << ",\"elapsed\":" << status.frame_ticks_elapsed
        << ",\"ticks\":" << controller.frame_ticks()
        << ",\"frame_mask\":" << controller.frame_mask()
        << ",\"heading\":" << controller.heading()
        << ",\"accum\":" << controller.accumulator()
        << ",\"cond\":" << controller.condition_current()
        << ",\"latch\":" << controller.condition_latched()
        << ",\"event_bits\":" << controller.event_bits()
        << ",\"last_events\":" << controller.last_events()
        << ",\"correction\":" << controller.last_correction()
        << ",\"move\":" << controller.move_extra()
        << ",\"ra\":" << controller.random_a()
        << ",\"rb\":" << controller.random_b();
    auto write_pose = [&out](const char* key, const auto& pose) {
        out << ",\"" << key << "\":[";
        for (std::size_t bone = 0; bone < pose.size(); ++bone) {
            if (bone) out << ",";
            out << "[" << pose[bone].angles[0] << "," << pose[bone].angles[1] << ","
                << pose[bone].angles[2] << "," << pose[bone].scale[0] << ","
                << pose[bone].scale[1] << "," << pose[bone].scale[2] << "]";
        }
        out << "]";
    };
    write_pose("pose", controller.pose());
    write_pose("delta", controller.pose_delta());
    auto write_roots = [&out](const char* key, const auto& roots) {
        out << ",\"" << key << "\":[";
        for (std::size_t bone = 0; bone < roots.size(); ++bone) {
            if (bone) out << ",";
            out << "[" << roots[bone][0] << "," << roots[bone][1] << "," << roots[bone][2] << "]";
        }
        out << "]";
    };
    write_roots("root", controller.root_positions());
    write_roots("root_delta", controller.root_delta());
    out << ",\"motion\":[" << controller.motion()[0] << "," << controller.motion()[1] << ","
        << controller.motion()[2] << "]}";
    std::cout << out.str() << "\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: animation_probe --anm-json ANM [ANM...]\n"
                     "       animation_probe --trace-json CHR < commands\n"
                     "Commands: init\n"
                     "  status\n"
                     "  select SLOT FRAME\n"
                     "  tick H A24 A28 A2C STATE60 ANIM48 STANCE PITCH AIM_A AIM_B B78 B79 SW66\n"
                     "       EXTRA4C COND_WRITE COND38 RAND_WRITE RA RB CARRY\n"
                     "  condition OP CARRY H ACCUM MOTION_Y EXTRA4C STATE60 ANIM48 STANCE PITCH\n"
                     "       B78 B79 AIM_A AIM_B SW66\n"
                     "  random H ACCUM RA RB\n";
        return 2;
    }
    const std::string mode = argv[1];
    try {
        if (mode == "--anm-json") {
            if (argc < 3) throw std::runtime_error("an ANM path is required");
            for (int i = 2; i < argc; ++i) {
                const auto definition = dfre::decode_animation_definition(read_text(argv[i]));
                std::cout << "[";
                for (std::size_t index = 0; index < definition.actions.size(); ++index) {
                    const auto& action = definition.actions[index];
                    if (index) std::cout << ",";
                    std::cout << "{\"name\":\"" << action.name << "\",\"number\":" << action.number
                              << ",\"velocity\":" << action.velocity
                              << ",\"velocity_per_tick\":" << action.velocity_per_tick
                              << ",\"override\":" << (action.override_flag ? 1 : 0) << "}";
                }
                std::cout << "]\n";
            }
            return 0;
        }
        if (mode == "--trace-json") {
            if (argc != 3) throw std::runtime_error("--trace-json requires a CHR path");
            const auto model = dfre::prepare_character_runtime(dfre::decode_character(read_bytes(argv[2])));
            dfre::AnimationController controller(model);
            std::string line;
            while (std::getline(std::cin, line)) {
                std::istringstream stream(line);
                std::string command;
                if (!(stream >> command)) continue;
                if (command == "init") {
                    controller.restart();
                    print_state(controller, true, 0);
                } else if (command == "select") {
                    std::uint32_t slot = 0, frame = 0;
                    stream >> slot >> frame;
                    const bool ok = controller.select_action(slot, frame);
                    print_state(controller, ok, 0);
                } else if (command == "tick") {
                    dfre::AnimationInputs inputs;
                    int write_condition = 0, write_random = 0, carry = 0;
                    unsigned stance = 0, pitch = 0, aim_a = 0, aim_b = 0;
                    unsigned byte78 = 0, byte79 = 0, switch_flag = 0;
                    stream >> inputs.object_heading >> inputs.object_angles[0] >> inputs.object_angles[1]
                           >> inputs.object_angles[2] >> inputs.state_flags >> inputs.anim_flags
                           >> stance >> pitch >> aim_a >> aim_b
                           >> byte78 >> byte79 >> switch_flag >> inputs.extra_4c
                           >> write_condition >> inputs.condition_state >> write_random
                           >> inputs.random_a >> inputs.random_b >> carry;
                    inputs.stance = std::uint8_t(stance);
                    inputs.pitch = std::uint8_t(pitch);
                    inputs.aim_a = std::uint8_t(aim_a);
                    inputs.aim_b = std::uint8_t(aim_b);
                    inputs.byte78 = std::uint8_t(byte78);
                    inputs.byte79 = std::uint8_t(byte79);
                    inputs.switch_flag = std::uint8_t(switch_flag);
                    inputs.write_condition_state = write_condition != 0;
                    inputs.write_random_selector = write_random != 0;
                    inputs.condition_carry = carry != 0;
                    const auto result = controller.tick(inputs);
                    std::ostringstream extra;
                    extra << "\"events\":" << result.events << ",\"action_changed\":"
                          << (result.action_changed ? 1 : 0) << ",";
                    print_state(controller, true, result.action_changed ? 1 : 0, extra.str());
                } else if(command=="set-condition") {
                    std::uint32_t value=0;stream>>value;controller.set_condition_state(value);
                    print_state(controller,true,0);
                } else if (command == "status") {
                    print_state(controller, true, 0);
                } else if (command == "condition") {
                    int opcode = 0, carry = 0;
                    unsigned stance = 0, pitch = 0, byte78 = 0, byte79 = 0, aim_a = 0, aim_b = 0, switch_flag = 0;
                    dfre::AnimationConditionState state;
                    stream >> opcode >> carry >> state.heading >> state.accumulator >> state.motion_y
                           >> state.extra_4c >> state.state_flags >> state.anim_flags >> stance
                           >> pitch >> byte78 >> byte79 >> aim_a >> aim_b >> switch_flag;
                    state.stance = std::uint8_t(stance);
                    state.pitch = std::uint8_t(pitch);
                    state.byte78 = std::uint8_t(byte78);
                    state.byte79 = std::uint8_t(byte79);
                    state.aim_a = std::uint8_t(aim_a);
                    state.aim_b = std::uint8_t(aim_b);
                    state.switch_flag = std::uint8_t(switch_flag);
                    const bool passed = dfre::check_animation_condition(
                        std::uint8_t(opcode), carry != 0, state);
                    std::cout << "{\"condition\":" << (passed ? 1 : 0) << "}\n";
                } else if (command == "random") {
                    std::int32_t heading = 0, accumulator = 0;
                    std::uint32_t random_a = 0, random_b = 0;
                    stream >> heading >> accumulator >> random_a >> random_b;
                    std::cout << "{\"random\":" << unsigned(dfre::animation_random_selector(
                        heading, accumulator, random_a, random_b)) << "}\n";
                } else {
                    throw std::runtime_error("unknown command");
                }
            }
            return 0;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
    std::cerr << "unknown mode\n";
    return 2;
}
