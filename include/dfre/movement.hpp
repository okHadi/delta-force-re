#pragma once
#include "dfre/player.hpp"
#include <array>
#include <cstdint>

// Original single-player input-to-motion leaves from Df.exe
// SHA256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// Addresses are original virtual addresses. See docs/movement-runtime.md.

namespace dfre {
// Local player control flags. Original 0xC23A00 (first byte) and 0xC23A01.
// Keyboard bindings dispatch through 0x479590 and 0x477D40; only bits used by
// the ported leaves are named. Other bits stay unnamed.
inline constexpr std::uint8_t control_no_run      = 0x01; // 0x9F, 0x47829D
inline constexpr std::uint8_t control_forward     = 0x02; // 0x8F, 0x478370
inline constexpr std::uint8_t control_back        = 0x04; // 0x8E, 0x477EA4
inline constexpr std::uint8_t control_slide_left  = 0x08; // 0x93, 0x4782D3
inline constexpr std::uint8_t control_slide_right = 0x10; // 0x94, 0x4782FD
inline constexpr std::uint8_t control_look_up     = 0x20; // 0x92, 0x478ED3
inline constexpr std::uint8_t control_look_down   = 0x40; // 0x91, 0x478E96
inline constexpr std::uint8_t control_turn_left   = 0x01; // 0x95, 0x478352
inline constexpr std::uint8_t control_turn_right  = 0x02; // 0x96, 0x478F26
inline constexpr std::uint8_t control_jump        = 0x04; // 0x90, 0x478E63

// Absolute look actions pushed with the original mouse delta.
inline constexpr std::uint16_t action_look_up_abs   = 0x9b;
inline constexpr std::uint16_t action_look_dn_abs   = 0x9c;
inline constexpr std::uint16_t action_turn_left_abs = 0x9d;
inline constexpr std::uint16_t action_turn_right_abs= 0x9e;

// Turn step written at 0x488248; doubled when control_no_run is clear.
inline constexpr std::uint32_t look_step_base = 0x00ffffff;

// Actor fields advanced by 0x487658. Field offsets are original.
struct ActorMotion {
    std::array<std::int32_t,3> position{}; // actor 0x24, 0x28, 0x2c
    std::int32_t vertical_velocity=0;      // actor 0x44
};

// 0x479630: mouse axes to signed action values. `sensitivity` is config
// 0x67F500, `flip` is config 0x67F4FC, `steady` is the 0x484740 predicate.
// Positive turn means action 0x9E (yaw minus), positive look means 0x9B
// (pitch plus); the absolute actions are 0x9D/0x9C for negative values.
struct MouseLook {
    std::int32_t turn=0;
    std::int32_t look=0;
};
MouseLook mouse_look(std::int32_t axis_x,std::int32_t axis_y,std::int32_t sensitivity,bool flip,bool steady) noexcept;
// 0x488379-0x4883FD: per-tick yaw/pitch step from control flags.
PlayerPose apply_look_step(const PlayerPose& pose,std::uint8_t flags0,std::uint8_t flags1) noexcept;
// 0x4883CB-0x48894B: pitch limit, wrapping 32-bit compare.
std::uint32_t clamp_pitch(std::uint32_t pitch) noexcept;
// 0x478EE2, 0x478F35, 0x478F16, 0x47823C: absolute look actions.
PlayerPose apply_look_action(std::uint16_t action,std::int32_t value,const PlayerPose& pose) noexcept;
// 0x487658-0x487671: advance one actor by a step.
void apply_step(ActorMotion& actor,std::int32_t dx,std::int32_t dy,std::int32_t dz) noexcept;
// 0x4884FA-0x488660: movement bits to octant, forward is 0 and numbering runs
// counter-clockwise. The selector value is the second original output (edi).
int move_octant(std::uint8_t flags0) noexcept;
int move_anim_selector(std::uint8_t flags0,bool actor_flag_6e_bit3) noexcept;
} // namespace dfre
