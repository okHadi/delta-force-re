#include "dfre/animation_driver.hpp"
#include <stdexcept>

// Port of the animation portion of Df.exe 0x487370 with the event dispatch
// 0x4872C0. See docs/animation-driver.md.
namespace dfre {
namespace {

const AnimationDriverAction* find_action(const std::vector<AnimationDriverAction>& actions,
    std::int32_t item_index) {
    for (const auto& action : actions) if (std::int32_t(action.item_index) == item_index)
        return &action;
    return nullptr;
}

} // namespace

const std::vector<AnimationCompiledAction>& original_animation_compiled_actions() {
    // Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
    static const std::vector<AnimationCompiledAction> rows{
        {"Null",0,0,0,65536,65536},
        {"Standing_1HW",1,0,0,65536,65536},
        {"Standing_2HW",2,0,0,65536,65536},
        {"Standing_SLW",3,0,0,65536,65536},
        {"Standing_SCW",4,0,0,65536,65536},
        {"Standing_BIN",5,0,0,65536,65536},
        {"Standing_SWTCH",6,0,0,65536,65536},
        {"Standing_KNIFE",7,0,0,65536,65536},
        {"Standing_S_1HW",10,0,2,65536,65536},
        {"Standing_S_2HW",11,0,2,65536,65536},
        {"Standing_S_SLW",12,0,2,65536,65536},
        {"Standing_S_SCW",13,0,2,65536,65536},
        {"Standing_S_KNIFE",14,0,0,65536,65536},
        {"Walking0_1HW",20,0,0,131072,131072},
        {"Walking1_1HW",21,1,0,131072,131072},
        {"Walking2_1HW",22,2,0,131072,131072},
        {"Walking3_1HW",23,3,0,131072,131072},
        {"Walking4_1HW",24,4,0,131072,131072},
        {"Walking5_1HW",25,5,0,131072,131072},
        {"Walking6_1HW",26,6,0,131072,131072},
        {"Walking7_1HW",27,7,0,131072,131072},
        {"Walking0_2HW",28,0,0,131072,131072},
        {"Walking1_2HW",29,1,0,131072,131072},
        {"Walking2_2HW",30,2,0,131072,131072},
        {"Walking3_2HW",31,3,0,131072,131072},
        {"Walking4_2HW",32,4,0,131072,131072},
        {"Walking5_2HW",33,5,0,131072,131072},
        {"Walking6_2HW",34,6,0,131072,131072},
        {"Walking7_2HW",35,7,0,131072,131072},
        {"Walking0_S_1HW",40,0,0,131072,131072},
        {"Walking1_S_1HW",41,1,0,131072,131072},
        {"Walking2_S_1HW",42,2,0,131072,131072},
        {"Walking3_S_1HW",43,3,0,131072,131072},
        {"Walking4_S_1HW",44,4,0,131072,131072},
        {"Walking5_S_1HW",45,5,0,131072,131072},
        {"Walking6_S_1HW",46,6,0,131072,131072},
        {"Walking7_S_1HW",47,7,0,131072,131072},
        {"Walking0_S_2HW",48,0,0,131072,131072},
        {"Walking1_S_2HW",49,1,0,131072,131072},
        {"Walking2_S_2HW",50,2,0,131072,131072},
        {"Walking3_S_2HW",51,3,0,131072,131072},
        {"Walking4_S_2HW",52,4,0,131072,131072},
        {"Walking5_S_2HW",53,5,0,131072,131072},
        {"Walking6_S_2HW",54,6,0,131072,131072},
        {"Walking7_S_2HW",55,7,0,131072,131072},
        {"Running0_1HW",60,0,0,262144,262144},
        {"Running1_1HW",61,1,0,262144,262144},
        {"Running2_1HW",62,2,0,262144,262144},
        {"Running3_1HW",63,3,0,262144,262144},
        {"Running4_1HW",64,4,0,262144,262144},
        {"Running5_1HW",65,5,0,262144,262144},
        {"Running6_1HW",66,6,0,262144,262144},
        {"Running7_1HW",67,7,0,262144,262144},
        {"Running0_2HW",68,0,0,262144,262144},
        {"Running1_2HW",69,1,0,262144,262144},
        {"Running2_2HW",70,2,0,262144,262144},
        {"Running3_2HW",71,3,0,262144,262144},
        {"Running4_2HW",72,4,0,262144,262144},
        {"Running5_2HW",73,5,0,262144,262144},
        {"Running6_2HW",74,6,0,262144,262144},
        {"Running7_2HW",75,7,0,262144,262144},
        {"Running0_S_1HW",80,0,0,262144,262144},
        {"Running1_S_1HW",81,1,0,262144,262144},
        {"Running2_S_1HW",82,2,0,262144,262144},
        {"Running3_S_1HW",83,3,0,262144,262144},
        {"Running4_S_1HW",84,4,0,262144,262144},
        {"Running5_S_1HW",85,5,0,262144,262144},
        {"Running6_S_1HW",86,6,0,262144,262144},
        {"Running7_S_1HW",87,7,0,262144,262144},
        {"Running0_S_2HW",88,0,0,262144,262144},
        {"Running1_S_2HW",89,1,0,262144,262144},
        {"Running2_S_2HW",90,2,0,262144,262144},
        {"Running3_S_2HW",91,3,0,262144,262144},
        {"Running4_S_2HW",92,4,0,262144,262144},
        {"Running5_S_2HW",93,5,0,262144,262144},
        {"Running6_S_2HW",94,6,0,262144,262144},
        {"Running7_S_2HW",95,7,0,262144,262144},
        {"Crouched_1HW",100,0,0,32768,65536},
        {"Crouched_2HW",101,0,0,32768,65536},
        {"Crouched_SLW",102,0,0,32768,65536},
        {"Crouched_SCW",103,0,0,32768,65536},
        {"Crouched_BIN",104,0,0,32768,65536},
        {"Crouched_SWTCH",105,0,0,32768,65536},
        {"Crouched_KNIFE",106,0,0,32768,65536},
        {"Prone_1HW",110,0,0,5242,65536},
        {"Prone_2HW",111,0,0,5242,65536},
        {"Prone_SLW",112,0,0,5242,65536},
        {"Prone_SCW",113,0,0,5242,65536},
        {"Prone_BIN",114,0,0,5242,65536},
        {"Prone_SWTCH",115,0,0,5242,65536},
        {"Prone_KNIFE",116,0,0,5242,65536},
        {"Crawling_CR0",120,0,0,13107,78643},
        {"Crawling_CR1",121,1,0,13107,78643},
        {"Crawling_CR2",122,2,0,13107,78643},
        {"Crawling_CR3",123,3,0,13107,78643},
        {"Crawling_CR4",124,4,0,13107,78643},
        {"Crawling_CR5",125,5,0,13107,78643},
        {"Crawling_CR6",126,6,0,13107,78643},
        {"Crawling_CR7",127,7,0,13107,78643},
        {"Jumping_1HW",130,0,0,262144,262144},
        {"Jumping_2HW",131,0,0,262144,262144},
        {"Jumping_S_1HW",132,0,0,262144,262144},
        {"Jumping_S_2HW",133,0,0,262144,262144},
        {"Floating",140,0,0,13107,131072},
        {"Swimming",141,0,0,13107,131072},
        {"F1",150,0,0,65536,65536},
        {"F2",151,0,0,65536,65536},
        {"F3",152,0,0,65536,65536},
        {"F4",153,0,0,65536,65536},
        {"F5",154,0,0,65536,65536},
        {"F6",155,0,0,65536,65536},
        {"F7",156,0,0,65536,65536},
        {"F8",157,0,0,65536,65536},
        {"F9",158,0,0,65536,65536},
        {"F10",159,0,0,65536,65536},
        {"F11",160,0,0,65536,65536},
        {"F12",161,0,0,65536,65536},
        {"F1_C",170,0,0,65536,65536},
        {"F2_C",171,0,0,65536,65536},
        {"F3_C",172,0,0,65536,65536},
        {"F4_C",173,0,0,65536,65536},
        {"F5_C",174,0,0,65536,65536},
        {"F6_C",175,0,0,65536,65536},
        {"F7_C",176,0,0,65536,65536},
        {"F8_C",177,0,0,65536,65536},
        {"F9_C",178,0,0,65536,65536},
        {"F10_C",179,0,0,65536,65536},
        {"F11_C",180,0,0,65536,65536},
        {"F12_C",181,0,0,65536,65536},
        {"Death_B_P01DF",190,0,0,65536,65536},
        {"Death_B_P01DL",191,0,0,65536,65536},
        {"Death_B_P01DB",192,0,0,65536,65536},
        {"Death_B_P01DR",193,0,0,65536,65536},
        {"Death_B_P02DF",194,0,0,65536,65536},
        {"Death_B_P02DL",195,0,0,65536,65536},
        {"Death_B_P02DB",196,0,0,65536,65536},
        {"Death_B_P02DR",197,0,0,65536,65536},
        {"Death_B_P03DF",198,0,0,65536,65536},
        {"Death_B_P03DL",199,0,0,65536,65536},
        {"Death_B_P03DB",200,0,0,65536,65536},
        {"Death_B_P03DR",201,0,0,65536,65536},
        {"Death_B_P04DF",202,0,0,65536,65536},
        {"Death_B_P04DL",203,0,0,65536,65536},
        {"Death_B_P04DB",204,0,0,65536,65536},
        {"Death_B_P04DR",205,0,0,65536,65536},
        {"Death_B_P05DF",206,0,0,65536,65536},
        {"Death_B_P05DL",207,0,0,65536,65536},
        {"Death_B_P05DB",208,0,0,65536,65536},
        {"Death_B_P05DR",209,0,0,65536,65536},
        {"Death_B_P06DF",210,0,0,65536,65536},
        {"Death_B_P06DL",211,0,0,65536,65536},
        {"Death_B_P06DB",212,0,0,65536,65536},
        {"Death_B_P06DR",213,0,0,65536,65536},
        {"Death_B_P07DF",214,0,0,65536,65536},
        {"Death_B_P07DL",215,0,0,65536,65536},
        {"Death_B_P07DB",216,0,0,65536,65536},
        {"Death_B_P07DR",217,0,0,65536,65536},
        {"Death_B_P08DF",218,0,0,65536,65536},
        {"Death_B_P08DL",219,0,0,65536,65536},
        {"Death_B_P08DB",220,0,0,65536,65536},
        {"Death_B_P08DR",221,0,0,65536,65536},
        {"Death_B_P09DF",222,0,0,65536,65536},
        {"Death_B_P09DL",223,0,0,65536,65536},
        {"Death_B_P09DB",224,0,0,65536,65536},
        {"Death_B_P09DR",225,0,0,65536,65536},
        {"Death_B_P10DF",226,0,0,65536,65536},
        {"Death_B_P10DL",227,0,0,65536,65536},
        {"Death_B_P10DB",228,0,0,65536,65536},
        {"Death_B_P10DR",229,0,0,65536,65536},
        {"Death_B_P11DF",230,0,0,65536,65536},
        {"Death_B_P11DL",231,0,0,65536,65536},
        {"Death_B_P11DB",232,0,0,65536,65536},
        {"Death_B_P11DR",233,0,0,65536,65536},
        {"Death_B_P12DF",234,0,0,65536,65536},
        {"Death_B_P12DL",235,0,0,65536,65536},
        {"Death_B_P12DB",236,0,0,65536,65536},
        {"Death_B_P12DR",237,0,0,65536,65536},
        {"Death_B_P13DF",238,0,0,65536,65536},
        {"Death_B_P13DL",239,0,0,65536,65536},
        {"Death_B_P13DB",240,0,0,65536,65536},
        {"Death_B_P13DR",241,0,0,65536,65536},
        {"Death_B_P14DF",242,0,0,65536,65536},
        {"Death_B_P14DL",243,0,0,65536,65536},
        {"Death_B_P14DB",244,0,0,65536,65536},
        {"Death_B_P14DR",245,0,0,65536,65536},
        {"Death_B_P15DF",246,0,0,65536,65536},
        {"Death_B_P15DL",247,0,0,65536,65536},
        {"Death_B_P15DB",248,0,0,65536,65536},
        {"Death_B_P15DR",249,0,0,65536,65536},
        {"Death_G_DF",250,0,1,65536,65536},
        {"Death_G_DL",251,0,1,65536,65536},
        {"Death_G_DB",252,0,1,65536,65536},
        {"Death_G_DR",253,0,1,65536,65536},
        {"Death_Water",254,0,0,65536,65536},
        {"Death_Fire",255,0,1,65536,65536},
    };
    return rows;
}

std::vector<AnimationDriverAction> build_animation_driver_actions(
    const AnimationDefinition& definition, const std::vector<AnimationCompiledAction>& compiled) {
    for (const auto& entry : definition.actions) {
        bool known = false;
        for (const auto& row : compiled) known = known || row.name == entry.name;
        if (!known) throw std::invalid_argument("ANM action has no compiled item slot: " + entry.name);
    }
    std::vector<AnimationDriverAction> actions;
    actions.reserve(compiled.size());
    for (const auto& row : compiled) {
        AnimationDriverAction action;
        action.item_index = row.item_index;
        for (const auto& entry : definition.actions) {
            if (entry.name != row.name) continue;
            action.animation_number = entry.number;
            if (entry.velocity_present) action.velocity_per_tick = entry.velocity_per_tick;
            action.override_flag = action.override_flag || entry.override_flag;
            action.direction = row.direction;
            action.tag = row.tag;
            action.weight_10 = row.weight_10;
            action.aim_14 = row.aim_14;
        }
        actions.push_back(action);
    }
    return actions;
}

AnimationEventDispatch animation_event_dispatch(std::uint8_t event) {
    switch (event) {
        case 0x12: return {AnimationEventTarget::call_43f5a0, 7, 0};
        case 0x13: return {AnimationEventTarget::call_43f5a0, 8, 0};
        case 0x14: return {AnimationEventTarget::call_43f5a0, 9, 0};
        case 0x15: return {AnimationEventTarget::call_43f5a0, 10, 0};
        case 0x16: return {AnimationEventTarget::call_43f5a0, 11, 0};
        case 0x40: return {AnimationEventTarget::call_45e0f0, 1, 1};
        case 0x41: return {AnimationEventTarget::call_45e0f0, 1, 0};
        case 0x42: return {AnimationEventTarget::call_45e0f0, 0, 1};
        case 0x43: return {AnimationEventTarget::call_45e0f0, 0, 0};
        default: return {};
    }
}

AnimationDriver::AnimationDriver(AnimationController& controller,
    std::vector<AnimationDriverAction> actions) : controller_(&controller),
    actions_(std::move(actions)) {
    restart();
}

void AnimationDriver::restart() {
    controller_->restart();
    current_action_ = 0; // obj+0x80 and obj+0x7C start together
}

const AnimationDriverAction* AnimationDriver::action(std::uint32_t item_index) const {
    return find_action(actions_, std::int32_t(item_index));
}

AnimationDriverUpdate AnimationDriver::update(AnimationDriverState state,
    const std::function<void(AnimationDriverState&)>& on_action_tag) {
    AnimationDriverUpdate result;
    // 0x4877F4: pending action change plays item+0x04 at frame 0. A reserved
    // item index holds zeros, so the original selects animation slot 0.
    if (state.pending_action != current_action_) {
        const auto* pending = action(std::uint32_t(state.pending_action));
        const std::uint32_t slot = pending ? std::uint32_t(pending->animation_number) : 0;
        controller_->select_action(slot, 0); // 0x42CEF0 -> 0x43A150, failures leave the instance
        current_action_ = state.pending_action;
        result.action_started = true;
        if (pending && (pending->tag & 1)) {
            result.action_tag_effect = true; // 0x460D20(obj,0,0,0), before the tick
            if(on_action_tag)on_action_tag(state);
        }
    }
    // 0x4873B4: current item fields (packing proof for item+0x00).
    const auto* current = action(std::uint32_t(current_action_));
    result.action_index = current_action_;
    result.animation_slot = current ? current->animation_number : 0;
    result.override_flag = current != nullptr && current->override_flag;
    result.action_direction = current ? std::uint32_t(current->direction) << 29 : 0;
    result.move_heading = std::int32_t(std::uint32_t(state.heading_raw) +
        result.action_direction);
    // 0x4873D3: instance writes and the controller tick, gated by param_3 bit 0.
    if (state.update_flags & 1) {
        AnimationInputs inputs;
        inputs.object_heading = state.heading_raw;   // instance+0x34 = heading >> 7 in the tick
        for (std::size_t index = 0; index < 3; ++index)
            inputs.object_angles[index] = state.object_angles[index];
        inputs.anim_flags = state.anim_flags;
        inputs.stance = state.stance;
        inputs.pitch = state.pitch;
        inputs.byte78 = state.byte78;
        inputs.byte79 = state.byte79;
        inputs.aim_a = state.aim_a;
        inputs.aim_b = state.aim_b;
        inputs.switch_flag = state.switch_flag;
        inputs.extra_4c = state.extra_4c;
        inputs.state_flags = 0;             // instance+0x60 = 0
        inputs.write_random_selector = true; // instance+0x28 = +0x2C = 0
        inputs.random_a = 0;
        inputs.random_b = 0;
        result.tick = controller_->tick(inputs);
        result.ticked = true;
        result.events = result.tick.events;
        // 0x4872C0 masks the 16-bit events to the low byte before dispatch.
        result.dispatch = animation_event_dispatch(std::uint8_t(result.events & 0xff));
    }
    return result;
}

} // namespace dfre
