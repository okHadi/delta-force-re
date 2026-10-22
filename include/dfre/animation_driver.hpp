#pragma once
#include "dfre/animation.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Game-to-instance animation driver: the animation portion of the original
// object update 0x487370 (pending action 0x4877F4, instance state writes
// 0x4873D3-0x487400, tick 0x42CE40, frame event dispatch 0x4872C0) on top of
// the public AnimationController. Addresses are Df.exe sha256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// Motion physics, AI and weapon systems own their fields; this module only
// reads them as inputs and reports the original dispatches without inventing
// meanings.

namespace dfre {

// One compiled action row from the executable table at 0x4EF7C8 (0x34 bytes):
// name[0x20], item index (A) at +0x20, move direction (B) at +0x24, tag (E) at
// +0x30. C/D at +0x28/+0x2C supply weight_10/aim_14 after 16.16 conversion.
// The ANM text supplies the animation number, velocity and override.
struct AnimationCompiledAction {
    std::string name;
    std::uint32_t item_index{}; // 0x42CF60 uses A as the item slot (A*0x1C)
    std::uint8_t direction{};   // item+0x00 = direction << 29
    std::uint8_t tag{};         // item+0x18 low byte; bit 0 triggers 0x460D20
    std::int32_t weight_10{};   // trunc(compiled C * 65536)
    std::int32_t aim_14{};      // trunc(compiled D * 65536)
};

// Item fields the game observes for one action (0x42CF60 + 0x42D100 packing).
struct AnimationDriverAction {
    std::uint32_t item_index{};       // addressed by obj+0x7C / obj+0x80
    std::int32_t animation_number{};  // item+0x04
    std::int32_t velocity_per_tick{}; // item+0x08
    bool override_flag{};             // item+0x0C
    std::uint8_t direction{};         // item+0x00 source (direction << 29)
    std::uint8_t tag{};               // item+0x18 low byte source
    std::int32_t weight_10{};         // item+0x10
    std::int32_t aim_14{};            // item+0x14
};

// Original 195 named rows at 0x4EF7C8, in stored order.
const std::vector<AnimationCompiledAction>& original_animation_compiled_actions();

// Apply ANM rows in file order. Absent rows keep zero item fields.
// Repeated rows replace the number, replace a supplied velocity, and retain override.
std::vector<AnimationDriverAction> build_animation_driver_actions(
    const AnimationDefinition& definition, const std::vector<AnimationCompiledAction>& compiled);

// Typed frame event dispatch from 0x4872C0. Only the low event byte is used
// (`and eax,0xff`); the high byte from the controller tick is masked off.
enum class AnimationEventTarget : std::uint8_t {
    none,
    call_43f5a0, // object-position effect: 0x43E7D0(table[[[obj+4]+0xD4]*12+value], x, y, z+0x10000)
    call_45e0f0, // object effect with two flags/tables
};

struct AnimationEventDispatch {
    AnimationEventTarget target{AnimationEventTarget::none};
    std::uint8_t value{};  // 0x43F5A0 value (7..11) or 0x45E0F0 first argument
    std::uint8_t value2{}; // 0x45E0F0 second argument
};

inline bool operator==(const AnimationEventDispatch& left, const AnimationEventDispatch& right) {
    return left.target == right.target && left.value == right.value && left.value2 == right.value2;
}
inline bool operator!=(const AnimationEventDispatch& left, const AnimationEventDispatch& right) {
    return !(left == right);
}

// Original dispatch rows: 0x12..0x16 -> 0x43F5A0(obj, 7..11); 0x40 -> (1,1),
// 0x41 -> (1,0), 0x42 -> (0,1), 0x43 -> (0,0) through 0x45E0F0; all others do
// nothing.
AnimationEventDispatch animation_event_dispatch(std::uint8_t event);

// Game fields the driver reads. Actor, weapon and motion systems own them.
// obj+0x80 is driver state and is available through current_action().
struct AnimationDriverState {
    std::int32_t update_flags{};      // 0x487370 param_3; bit 0 gates the tick
    std::int32_t pending_action{};    // obj+0x7C
    std::int32_t heading_raw{};       // obj+0x30 (32-bit angle)
    std::int32_t object_angles[3]{};  // obj+0x24, +0x28, +0x2C
    std::uint32_t anim_flags{};       // instance+0x48
    std::uint8_t stance{};            // instance+0x64
    std::uint8_t pitch{};             // instance+0x65
    std::uint8_t byte78{};            // instance+0x78
    std::uint8_t byte79{};            // instance+0x79
    std::uint8_t aim_a{};             // instance+0x7A
    std::uint8_t aim_b{};             // instance+0x7B
    std::uint8_t switch_flag{};       // instance+0x66
    std::int32_t extra_4c{};          // instance+0x4C
};

struct AnimationDriverUpdate {
    bool action_started{};            // pending != current; 0x42CEF0 ran
    std::int32_t action_index{};      // obj+0x80 after the update
    std::int32_t animation_slot{0};   // item+0x04 of the current item (0 when reserved)
    std::uint32_t action_direction{}; // item+0x00 = direction << 29
    std::int32_t move_heading{};      // obj+0x30 + item+0x00 (motion worker input)
    bool override_flag{};             // item+0x0C of the current item
    bool action_tag_effect{};         // item+0x18 bit 0 -> 0x460D20(obj, 0, 0, 0)
    bool ticked{};                    // update_flags bit 0
    std::uint16_t events{};           // controller tick return (ax)
    AnimationEventDispatch dispatch{};// 0x4872C0 result for the low byte
    AnimationTickResult tick{};       // controller tick result
};

// Wraps one AnimationController with the original object-side flow:
// 1. 0x4877F4: play the pending action at frame 0 (0x42CEF0 -> 0x43A150) and
//    report the item+0x18 bit 0 side effect.
// 2. 0x4873B4: expose the current item fields for the motion worker.
// 3. 0x4873D3: when update_flags bit 0 is set, write instance +0x28 = +0x2C =
//    +0x60 = 0 and +0x34 = heading >> 7, run the tick, then dispatch the low
//    event byte. The high event byte is returned unmasked but never dispatched.
class AnimationDriver {
public:
    AnimationDriver(AnimationController& controller, std::vector<AnimationDriverAction> actions);

    void restart();
    AnimationDriverUpdate update(AnimationDriverState state,
        const std::function<void(AnimationDriverState&)>& on_action_tag = {});

    std::int32_t current_action() const { return current_action_; }
    const AnimationDriverAction* action(std::uint32_t item_index) const;
    const std::vector<AnimationDriverAction>& actions() const { return actions_; }
    AnimationController& controller() { return *controller_; }
    const AnimationController& controller() const { return *controller_; }

private:
    AnimationController* controller_;
    std::vector<AnimationDriverAction> actions_;
    std::int32_t current_action_{};
};

} // namespace dfre
