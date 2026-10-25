#pragma once
#include "dfre/models.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Original DF1 animation controller (Df.exe 0x439e48-0x43b2b0) built on the
// decoded CHR1 character data in models.hpp. Addresses cite the shipped
// executable sha256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// Raw field values are kept in original units; see docs/animation-runtime.md.

namespace dfre {

// One parsed ANM action line (0x42cf60 line handler behind 0x456720/0x4565f0).
// ANM files are decoded game scripts; each line is "name number velocity
// [override] [// comment]".
struct AnimationAction {
    std::string name;
    std::int32_t number{};            // atoi of token 2: CHR animation slot
    double velocity{};                // token 3: metres per second
    std::int32_t velocity_per_tick{}; // trunc(velocity * 65536) / 30
    bool override_flag{};             // fourth token equals "override"
    bool velocity_present{true};      // token 3 writes the existing slot velocity
};

struct AnimationDefinition {
    std::vector<AnimationAction> actions;
    // Original name match is exact (0x499e5e); returns nullptr when absent.
    const AnimationAction* find(std::string_view name) const;
};

// Parses decoded ANM text with the original line and token rules.
AnimationDefinition decode_animation_definition(std::string_view text);

// Raw game values a tick reads. The random selector (0x28/0x2c) and condition
// state (0x38) stay in controller state; the write_ flags mirror the caller
// writing them into the instance before the tick.
struct AnimationInputs {
    std::int32_t object_heading{};          // [object+0x30]
    std::int32_t object_angles[3]{};        // [object+0x24],[+0x28],[+0x2c]
    std::uint32_t state_flags{};            // [instance+0x60]
    std::uint32_t anim_flags{};             // [instance+0x48]
    std::uint8_t stance{};                  // [instance+0x64]
    std::uint8_t pitch{};                   // [instance+0x65]
    std::uint8_t byte78{};                  // [instance+0x78]
    std::uint8_t byte79{};                  // [instance+0x79]
    std::uint8_t aim_a{};                   // [instance+0x7a]
    std::uint8_t aim_b{};                   // [instance+0x7b]
    std::uint8_t switch_flag{};             // [instance+0x66]
    std::int32_t extra_4c{};                // [instance+0x4c]
    bool write_random_selector{};           // caller wrote 0x28/0x2c
    std::uint32_t random_a{};               // [instance+0x28]
    std::uint32_t random_b{};               // [instance+0x2c]
    bool write_condition_state{};           // caller wrote 0x38
    std::uint32_t condition_state{};        // [instance+0x38]
    bool condition_carry{};                 // CF operand for condition 0x28/0x29
};

struct AnimationTickResult {
    std::uint16_t events{};  // returned in ax: frame byte +0xa | byte +0xb << 8
    bool action_changed{};   // a frame-end condition selected a new action
    std::int32_t heading{};  // 0x4399d0 after the tick
};

struct AnimationStatus {
    std::int32_t animation_slot{-1}; // 1000 when the slot is not in the table
    std::int32_t frame_index{-1};    // 100 when the frame is not reachable
    std::int32_t frame_ticks_elapsed{};
};

// Condition operands for the original opcode switch (0x43ac40-0x43b2b0).
struct AnimationConditionState {
    std::int32_t heading{};      // 0x4399d0
    std::int32_t accumulator{};  // [instance+0x34]
    std::int32_t motion_y{};     // [instance+0x20]
    std::int32_t extra_4c{};     // [instance+0x4c]
    std::uint32_t state_flags{}; // [instance+0x60]
    std::uint32_t anim_flags{};  // [instance+0x48]
    std::uint8_t stance{};       // [instance+0x64]
    std::uint8_t pitch{};        // [instance+0x65]
    std::uint8_t byte78{};       // [instance+0x78]
    std::uint8_t byte79{};       // [instance+0x79]
    std::uint8_t aim_a{};        // [instance+0x7a]
    std::uint8_t aim_b{};        // [instance+0x7b]
    std::uint8_t switch_flag{};  // [instance+0x66]
};

// Borrowed current controller data. Acquire a fresh view for each operation.
// Array views stay valid until the next controller operation that can change
// storage. Keep the controller and its prepared model alive while using a view.
template<class T> struct AnimationSourceSpan { T* data; std::size_t size; };
struct AnimationSourceState {
    const CharacterModel& model;
    const CharacterAnimation* animation;
    const CharacterFrame* frame;
    const std::size_t& bone_count;
    const std::size_t& root_count;
    std::uint32_t& remaining_ticks;
    std::array<std::int32_t,3>& motion;
    std::uint32_t& random_a;
    std::uint32_t& random_b;
    std::uint32_t& accumulator;
    std::uint32_t& condition_current;
    std::uint32_t& condition_previous;
    std::uint32_t& condition_latched;
    std::uint32_t& anim_flags;
    std::int32_t& extra_4c;
    std::uint32_t& event_bits;
    std::uint32_t& state_flags;
    std::uint8_t& stance;
    std::uint8_t& pitch;
    std::uint8_t& switch_flag;
    std::uint8_t& byte78;
    std::uint8_t& byte79;
    std::uint8_t& aim_a;
    std::uint8_t& aim_b;
    AnimationSourceSpan<CharacterBonePose> pose;
    AnimationSourceSpan<CharacterBonePose> pose_delta;
    AnimationSourceSpan<std::array<std::int32_t,3>> root_base;
    AnimationSourceSpan<std::array<std::int32_t,3>> root_delta;
};

// Original condition opcode test (carry set means failed at the call sites).
bool check_animation_condition(std::uint8_t opcode, bool carry, const AnimationConditionState& state);
// Original pseudo-random selector 0x43ac10.
std::uint8_t animation_random_selector(std::int32_t heading, std::int32_t accumulator,
    std::uint32_t random_a, std::uint32_t random_b);

// Port of one animation instance (256 x 0x9c pool at 0x42fc60). Construction
// requires the prepared CHR model used by the original loader (0x439e48).
class AnimationController {
public:
    explicit AnimationController(const CharacterModel& prepared);

    // Original instance setup: zeroed state, pose seeded from slot 0 frame 0
    // (0x43a0e0), then select_action(0, 0) (0x43a150).
    void restart();

    // Original 0x43a150. Returns false, without changes, for an absent slot.
    bool select_action(std::uint32_t animation_slot, std::uint32_t frame_index);
    // ANM lookup plus 0x43a150; false when the name or slot is absent.
    bool select_action(const AnimationDefinition& definition, std::string_view action_name,
        std::uint32_t frame_index = 0);

    // Original per-object update 0x42ce40 around the controller tick 0x43a7a0.
    AnimationTickResult tick(const AnimationInputs& inputs);

    // Original status probe 0x42cd90.
    AnimationStatus status() const;
    AnimationSourceState source_state();

    bool has_animation() const { return animation_index_ >= 0; }
    std::int32_t animation_slot() const;
    std::int32_t frame_index() const { return frame_index_; }
    std::uint32_t frame_ticks() const { return frame_ticks_; }
    std::uint32_t frame_mask() const; // current CHR frame +0x04
    void set_condition_state(std::uint32_t value) { condition_current_ = value; } // instance +0x38
    const std::vector<CharacterBonePose>& pose() const { return pose_; }
    const std::vector<CharacterBonePose>& pose_delta() const { return pose_delta_; }
    // Root tables (0x8c/0x90) with one 3-dword entry per [animation+8] bone.
    const std::vector<std::array<std::int32_t, 3>>& root_positions() const { return root_base_; }
    std::size_t root_count() const { return root_count_; } // current animation +0x08
    const std::vector<std::array<std::int32_t, 3>>& root_delta() const { return root_delta_; }
    const std::array<std::int32_t, 3>& motion() const { return motion_; } // 0x1c..0x24
    std::uint32_t event_bits() const { return event_bits_; }              // 0x50
    std::uint32_t condition_current() const { return condition_current_; } // 0x38
    std::uint32_t condition_latched() const { return condition_latched_; } // 0x40
    std::uint16_t last_events() const { return last_events_; }             // 0x4399a8
    std::int32_t heading() const { return heading_; }                      // 0x4399d0
    std::int32_t accumulator() const { return std::int32_t(accumulator_); } // 0x34
    std::int32_t last_correction() const { return last_correction_; }      // 0x4399c4
    std::int32_t move_extra() const { return move_extra_; }                // [0x4399a4]+4
    std::uint32_t random_a() const { return random_a_; }                   // 0x28
    std::uint32_t random_b() const { return random_b_; }                   // 0x2c

private:
    const CharacterAnimation* animation() const;
    const CharacterFrame* frame() const;
    std::int32_t frame_duration(const CharacterFrame& frame) const;
    std::int32_t frame_flags(const CharacterFrame& frame) const;
    std::uint8_t frame_byte(const CharacterFrame& frame, std::size_t offset) const;
    std::uint32_t frame_word(const CharacterFrame& frame, std::size_t offset) const;
    std::uint32_t raw_u32(std::uint32_t offset) const;
    void set_frame(std::size_t frame, const CharacterFrame* previous);
    void compute_pose_delta(const CharacterFrame& frame, const std::array<std::int32_t, 3>& step, bool timed);
    void advance_state();
    bool run_conditions(std::uint8_t mode);
    bool evaluate_condition(std::uint8_t opcode, bool carry) const;
    std::uint8_t random_selector_value() const;
    AnimationConditionState condition_state() const;

    const CharacterModel* model_;
    std::size_t bone_count_{};
    std::size_t root_count_{};
    std::size_t root_capacity_{};
    int animation_index_{-1};
    int frame_index_{-1};
    std::uint32_t frame_ticks_{};
    std::vector<CharacterBonePose> pose_;
    std::vector<CharacterBonePose> pose_delta_;
    std::vector<std::array<std::int32_t, 3>> root_base_;
    std::vector<std::array<std::int32_t, 3>> root_delta_;
    std::array<std::int32_t, 3> motion_{};
    std::uint32_t accumulator_{};        // 0x34
    std::uint32_t condition_current_{};  // 0x38
    std::uint32_t condition_previous_{}; // 0x3c
    std::uint32_t condition_latched_{};  // 0x40
    std::uint32_t event_bits_{};         // 0x50
    std::uint32_t random_a_{};           // 0x28
    std::uint32_t random_b_{};           // 0x2c
    std::uint8_t switch_flag_{};         // 0x66
    std::uint16_t last_events_{};        // 0x4399a8
    std::int32_t heading_{};             // 0x4399d0
    std::int32_t last_correction_{};     // 0x4399c4
    std::int32_t move_extra_{};          // [0x4399a4]+4
    std::array<std::int32_t, 3> aim_{};  // 0x4399a0 target vector
    AnimationInputs inputs_;
};

} // namespace dfre
