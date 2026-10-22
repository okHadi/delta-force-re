// Animation driver probe: compiled action packing, typed event dispatch and
// scripted 0x487370 animation updates for tools/check_animation_driver_original.py.
#include "dfre/animation_driver.hpp"
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
// Tags file rows: name item_index direction tag (extracted from the executable
// compiled action table by the checker).
std::vector<dfre::AnimationCompiledAction> read_compiled(const char* path) {
    if(std::string(path)=="-")return dfre::original_animation_compiled_actions();
    std::istringstream stream(read_text(path));
    std::vector<dfre::AnimationCompiledAction> rows;
    std::string line;
    while(std::getline(stream,line)){
        std::istringstream fields(line);dfre::AnimationCompiledAction row;
        unsigned index=0,direction=0,tag=0;
        if(!(fields>>row.name>>index>>direction>>tag))continue;
        row.item_index=index;row.direction=std::uint8_t(direction);row.tag=std::uint8_t(tag);
        fields>>row.weight_10>>row.aim_14;
        rows.push_back(row);
    }
    return rows;
}
const char* target_name(dfre::AnimationEventTarget target) {
    switch (target) {
        case dfre::AnimationEventTarget::call_43f5a0: return "43f5a0";
        case dfre::AnimationEventTarget::call_45e0f0: return "45e0f0";
        default: return "none";
    }
}
void print_controller(const dfre::AnimationController& controller, std::ostringstream& out) {
    const auto status = controller.status();
    out << "\"slot\":" << status.animation_slot << ",\"frame\":" << status.frame_index
        << ",\"elapsed\":" << status.frame_ticks_elapsed << ",\"ticks\":" << controller.frame_ticks()
        << ",\"frame_mask\":" << controller.frame_mask()
        << ",\"heading\":" << controller.heading() << ",\"accum\":" << controller.accumulator()
        << ",\"cond\":" << controller.condition_current() << ",\"latch\":" << controller.condition_latched()
        << ",\"event_bits\":" << controller.event_bits() << ",\"last_events\":" << controller.last_events()
        << ",\"correction\":" << controller.last_correction() << ",\"move\":" << controller.move_extra()
        << ",\"ra\":" << controller.random_a() << ",\"rb\":" << controller.random_b();
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
        << controller.motion()[2] << "]";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: animation_driver_probe --actions-json CHR ANM TAGS\n"
                     "       animation_driver_probe --trace-json CHR ANM TAGS < commands\n"
                     "Commands: init | status\n"
                     "  update FLAGS PENDING HEADING A24 A28 A2C ANIM48 STANCE PITCH AIM_A AIM_B\n"
                     "         B78 B79 SW66 EXTRA4C [EVENT]\n"
                     "  dispatch EVENT\n";
        return 2;
    }
    const std::string mode = argv[1];
    try {
        if ((mode == "--actions-json" || mode == "--table-json" || mode == "--trace-json") && argc < 5)
            throw std::runtime_error(mode + " requires CHR, ANM and TAGS paths");
        if(mode=="--table-json"){
            const auto definition=dfre::decode_animation_definition(read_text(argv[3]));
            const auto rows=read_compiled(argv[4]);
            const auto actions=dfre::build_animation_driver_actions(definition,rows);
            std::cout<<"[";
            for(std::uint32_t slot=0;slot<256;++slot){
                dfre::AnimationDriverAction action;
                for(const auto& candidate:actions)if(candidate.item_index==slot)action=candidate;
                if(slot)std::cout<<',';
                std::cout<<'['<<(std::uint32_t(action.direction)<<29)<<','<<std::uint32_t(action.animation_number)<<','
                    <<std::uint32_t(action.velocity_per_tick)<<','<<unsigned(action.override_flag)<<','
                    <<std::uint32_t(action.weight_10)<<','<<std::uint32_t(action.aim_14)<<','<<unsigned(action.tag)<<']';
            }
            std::cout<<"]\n";return 0;
        }
        if (mode == "--actions-json") {
            const auto model = dfre::prepare_character_runtime(dfre::decode_character(read_bytes(argv[2])));
            const auto definition = dfre::decode_animation_definition(read_text(argv[3]));
            const auto rows = read_compiled(argv[4]);
            const auto actions = dfre::build_animation_driver_actions(definition, rows);
            std::cout << "[";
            bool first = true;
            for (const auto& row : rows) {
                const dfre::AnimationDriverAction* action = nullptr;
                for (const auto& candidate : actions)
                    if (candidate.item_index == row.item_index) action = &candidate;
                if (!action) continue;
                if (!first) std::cout << ",";
                first = false;
                std::cout << "{\"name\":\"" << row.name << "\",\"item_index\":" << action->item_index
                          << ",\"number\":" << action->animation_number
                          << ",\"velocity_per_tick\":" << action->velocity_per_tick
                          << ",\"override\":" << (action->override_flag ? 1 : 0)
                          << ",\"direction\":" << unsigned(action->direction)
                          << ",\"tag\":" << unsigned(action->tag)
                          << ",\"weight_10\":" << std::uint32_t(action->weight_10)
                          << ",\"aim_14\":" << std::uint32_t(action->aim_14) << "}";
            }
            std::cout << "]\n";
            (void)model;
            return 0;
        }
        if (mode == "--trace-json") {
            const auto model = dfre::prepare_character_runtime(dfre::decode_character(read_bytes(argv[2])));
            const auto definition = dfre::decode_animation_definition(read_text(argv[3]));
            const auto rows = read_compiled(argv[4]);
            dfre::AnimationController controller(model);
            dfre::AnimationDriver driver(controller, dfre::build_animation_driver_actions(definition, rows));
            std::string line;
            while (std::getline(std::cin, line)) {
                std::istringstream stream(line);
                std::string command;
                if (!(stream >> command)) continue;
                if (command == "init") {
                    driver.restart();
                    std::ostringstream out;
                    out << "{\"ok\":1,\"action_index\":" << driver.current_action() << ",";
                    print_controller(controller, out);
                    std::cout << out.str() << "}\n";
                } else if (command == "status") {
                    std::ostringstream out;
                    out << "{\"ok\":1,\"action_index\":" << driver.current_action() << ",";
                    print_controller(controller, out);
                    std::cout << out.str() << "}\n";
                } else if (command == "update") {
                    dfre::AnimationDriverState state;
                    unsigned stance = 0, pitch = 0, aim_a = 0, aim_b = 0, byte78 = 0, byte79 = 0,
                        switch_flag = 0;
                    stream >> state.update_flags >> state.pending_action >> state.heading_raw
                           >> state.object_angles[0] >> state.object_angles[1] >> state.object_angles[2]
                           >> state.anim_flags >> stance >> pitch >> aim_a >> aim_b
                           >> byte78 >> byte79 >> switch_flag >> state.extra_4c;
                    state.stance = std::uint8_t(stance);
                    state.pitch = std::uint8_t(pitch);
                    state.aim_a = std::uint8_t(aim_a);
                    state.aim_b = std::uint8_t(aim_b);
                    state.byte78 = std::uint8_t(byte78);
                    state.byte79 = std::uint8_t(byte79);
                    state.switch_flag = std::uint8_t(switch_flag);
                    const auto result = driver.update(state);
                    std::ostringstream out;
                    out << "{\"ok\":1,\"action_started\":" << (result.action_started ? 1 : 0)
                        << ",\"action_index\":" << result.action_index
                        << ",\"animation_slot\":" << result.animation_slot
                        << ",\"direction\":" << result.action_direction
                        << ",\"move_heading\":" << result.move_heading
                        << ",\"override\":" << (result.override_flag ? 1 : 0)
                        << ",\"tag_effect\":" << (result.action_tag_effect ? 1 : 0)
                        << ",\"ticked\":" << (result.ticked ? 1 : 0)
                        << ",\"events\":" << result.events
                        << ",\"dispatch_target\":\"" << target_name(result.dispatch.target) << "\""
                        << ",\"dispatch_value\":" << unsigned(result.dispatch.value)
                        << ",\"dispatch_value2\":" << unsigned(result.dispatch.value2)
                        << ",\"action_changed\":" << (result.tick.action_changed ? 1 : 0) << ",";
                    print_controller(controller, out);
                    std::cout << out.str() << "}\n";
                } else if (command == "dispatch") {
                    unsigned event = 0;
                    stream >> event;
                    const auto dispatch = dfre::animation_event_dispatch(std::uint8_t(event));
                    std::cout << "{\"target\":\"" << target_name(dispatch.target) << "\",\"value\":"
                              << unsigned(dispatch.value) << ",\"value2\":" << unsigned(dispatch.value2)
                              << "}\n";
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
