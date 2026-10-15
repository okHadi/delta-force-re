#pragma once
#include "dfre/movement.hpp"
#include <array>
#include <cstdint>

// Motion stage two: original step vector, ground/gravity update and movement
// animation choice from Df.exe FUN_00487370 and FUN_00488200.
// SHA256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// Seconds/units are original; see docs/motion-step.md. No invented constants.

namespace dfre {
// {sine, cosine} of one full-turn angle in 22-bit fixed units, as returned by
// fixed_sine_cosine from dfre/models.hpp. The original helper is 0x4776C0:
// param_2 is the sine table (0xBD0068), param_3 is the cosine table (0xBD0868).
using SineCosine=std::array<std::int32_t,2>;

// Original 0x4873C6 and 0x487457 angle inputs.
SineCosine frame_base_sincos(std::uint32_t yaw,std::uint32_t frame_angle) noexcept;
SineCosine frame_pitch_sincos(std::uint32_t pitch) noexcept;

// Step vector, original 0x487432 (3D), 0x487834 (2D) and 0x487883 (vehicle).
struct FrameStepInput {
    std::uint32_t frame_angle=0;         // frame +0x00, added to the yaw
    std::int32_t frame_speed=0;          // frame +0x08
    bool frame_vertical=false;           // frame +0x0C != 0 selects the vehicle rule
    bool walk_3d=false;                  // actor +0x6E bit 2
    std::int32_t definition_side=0;      // definition +0x1C
    std::int32_t definition_vertical=0;  // definition +0x20
    std::int32_t definition_up=0;        // definition +0x24
    std::int32_t position_z=0;           // actor +0x2C
    std::int32_t water_level=0;          // 0xBD5248
    SineCosine base{};                   // sine/cosine of yaw + frame_angle
    SineCosine pitch_pair{};             // sine/cosine of the actor pitch
};
struct StepVector { std::int32_t x=0,y=0,z=0;
    bool operator==(const StepVector& other) const noexcept {return x==other.x&&y==other.y&&z==other.z;} };
StepVector frame_step(const FrameStepInput& input) noexcept;

// Terrain slope response, original 0x4874BF-0x487561. The gate is inline in the
// original: the response runs only for an active person (flags bit 0) that is
// not airborne (0x2000), not in water (0x20000) and not on a ground object.
struct SlopeResponse {StepVector step{};bool slope_blocked=false;bool wall=false;bool applied=false;};
SlopeResponse terrain_slope_response(const StepVector& step,std::uint32_t flags,std::int32_t slope_x,
    std::int32_t slope_y,const SineCosine& base) noexcept;

// Original 487991-4879CC: while airborne the previous grounded step is reused.
struct CachedStep {std::int32_t x=0,y=0,z=0;
    bool operator==(const CachedStep& other) const noexcept {return x==other.x&&y==other.y&&z==other.z;} };
StepVector cached_step(const StepVector& step,bool airborne,CachedStep& cache) noexcept;

// Actor fields touched by the ground and gravity update, original 0x487565-0x4877DE.
// `flags` is the original dword at actor +0x6C.
inline constexpr std::uint32_t actor_active=1;              // +0x6C bit 0
inline constexpr std::uint32_t actor_airborne=0x2000;       // +0x6D bit 0x20
inline constexpr std::uint32_t actor_in_water=0x20000;      // +0x6E bit 2
inline constexpr std::uint32_t actor_slope_blocked=0x40000; // +0x6E bit 4
inline constexpr std::uint32_t actor_slope_step=0x80000;    // +0x6E bit 8
struct MotionActor {
    std::array<std::int32_t,3> position{}; // +0x24, +0x28, +0x2C
    std::uint32_t yaw=0;                   // +0x30
    std::int32_t vertical=0;               // +0x44
    std::uint32_t flags=0;                 // +0x6C
    bool on_ground_object=false;           // actor +0x100 != 0
};
// The ground entity kept at actor +0x100, fields from 0x487573-0x487655.
struct GroundObject {
    std::array<std::int32_t,3> position{};
    std::uint32_t rotation=0;              // +0x90, zero means no rotation
    std::int32_t offset_x=0;               // +0x3C
    std::int32_t offset_y=0;               // +0x40
};
// Explicit terrain and collision query results.
struct GroundQuery {
    std::int32_t height=0;                 // 0x47DF40(x,y) or 0x486FD0(actor,pos,8000)
    std::int32_t water_level=0;            // 0xBD5248
    std::int32_t clearance=0xfffffff;      // minimum from the 0x4876E0 scan, 0xFFFFFFF when empty
    bool full_pass=false;                  // (tick_flags & 3) == 3 at 0x4876D4
};
struct GroundResult {bool snapped=false;bool entered_water=false;bool left_water=false;bool airborne=false;};
GroundResult ground_update(MotionActor& actor,const GroundObject* ground_object,const GroundQuery& query,
    const StepVector& step,bool is_local_player) noexcept;

// Movement animation choice and stance pulses, original 0x48834E-0x488969.
// Field names are placeholders for the original globals named in the comments.
struct MovementActorState {
    std::uint16_t control=0;               // 0xC23A00 word
    std::int32_t z=0;                      // actor +0x2C
    std::int32_t vertical=0;               // actor +0x44
    std::uint32_t flags=0;                 // actor +0x6C dword
    bool has_ground_object=false;          // actor +0x100 != 0
    std::int32_t mode=0;                   // 0xC23824
    std::int32_t prone=0;                  // 0xC23828
    std::int32_t stance=0;                 // 0xC23830
    std::uint8_t pulse_jump=0;             // 0xC23A10
    std::uint8_t pulse_prone=0;            // 0xC23A08
    std::uint8_t pulse_crouch=0;           // 0xC23A04
    std::uint8_t pulse_stand=0;            // 0xC23A0C
};
// Returns the animation id written to actor +0x7C, and updates the state.
// slot_flags is the profile byte at 0xAD3ED0 + slot*0xA8 + 0xA4.
// jump_impulse is 0xC2382C, the vertical value a jump starts with.
std::int32_t movement_animation(MovementActorState& state,std::uint8_t slot_flags,bool steady,
    std::int32_t jump_impulse) noexcept;
} // namespace dfre
