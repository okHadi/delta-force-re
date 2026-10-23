#pragma once
#include "dfre/ai.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>
namespace dfre {
class ObjectBehaviorError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

using ObjectBehaviorActorImage = AiActorImage;    // 0x114, actor +0x00..+0x113
using ObjectBehaviorPersonImage = AiPersonImage;  // 0xB4, person +0x00..+0xB3

// One foreign caller-visible write. A runtime answers a foreign call with the
// original return/output words and the state the call left behind, so the
// ported callback continues from the same values the original read.
struct ObjectBehaviorPatch {
    enum class Target : std::uint8_t { actor = 0, person = 1, global = 2 };
    Target target = Target::actor;
    std::uint32_t address = 0;  // actor/person byte offset, or global address
    std::uint8_t width = 4;     // 1, 2 or 4
    std::uint32_t value = 0;
};

// Scripted result of one foreign call, consumed in call order. values[0] is
// the original return value (EAX where the call returns one), values[1..] the
// words the original stored through the call's output pointers.
struct ObjectBehaviorInput {
    std::uint32_t site = 0;
    std::array<std::uint32_t, 6> values{};
    std::vector<ObjectBehaviorPatch> patches;
    std::uint8_t output_mask=0x3e; // Bits1..5 identify output words written by the call.
};

enum class ObjectBehaviorCallKind : std::uint8_t {
    query,   // return value or outputs drive a branch
    effect,  // call only changes state
};

// One ordered foreign call. args are the scalar arguments in declaration
// order, with pointer arguments replaced by their exact data: position blocks
// and actor images travel in payload.
struct ObjectBehaviorAction {
    ObjectBehaviorCallKind kind = ObjectBehaviorCallKind::effect;
    std::uint32_t site = 0;
    std::array<std::uint32_t, 6> args{};  // declaration order; pointer slots stay 0
    std::vector<std::uint8_t> payload;    // exact bytes the pointer arguments saw
};

struct ObjectBehaviorActor {
    ObjectBehaviorActorImage image{};
    std::uint32_t token = 0;  // original actor pointer value
};

// Read-only foreign data the original callbacks dereference. Every resolver is
// caller supplied; a missing resolver or out-of-range index throws
// ObjectBehaviorError instead of returning a made-up value.
struct ObjectBehaviorWorld {
    // actor +0xF8 and person +0x44 raw values -> 0x114 actor image (read only)
    std::function<const std::uint8_t*(std::uint32_t raw)> actor_image;
    // actor +0x0C raw value -> 0xB4 person image (read/write)
    std::function<std::uint8_t*(std::uint32_t raw)> person_image;
    // definition record (0x13C stride) field read, e.g. 0x20, 0x138
    std::function<std::uint32_t(std::uint32_t definition_index, std::uint32_t offset)> definition_dword;
    // definition record count (original 0xC23814) for the btre type scan
    std::function<std::size_t()> definition_count;
    // definition record write; prnh temporarily quadruples +0xE0 around 0x44FF30
    std::function<void(std::uint32_t definition_index, std::uint32_t offset, std::uint32_t value)> definition_write;
    // live type slot record (0x793680, 0x40 stride, slot = (actor +0x70) >> 16)
    std::function<std::uint32_t(std::uint32_t slot, std::uint32_t offset)> type_slot_dword;
    // globals the callbacks read directly: 0xBD0068 table entries etc.
    std::function<std::uint32_t(std::uint32_t address)> global_dword;
    // trigger/extra actor table 0x867E80 (0x114 stride), count 0x8CF904
    std::function<std::size_t()> trigger_actor_count;
    std::function<std::uint8_t*(std::size_t index)> trigger_actor_image;
    // actor +0x10 graphics object handle, dword at the record offset (0x64/0x68)
    std::function<std::uint32_t(std::uint32_t object_handle, std::uint32_t offset)> object_dword;
};

struct ObjectBehaviorState {
    std::uint32_t rng = 0;                  // original 0xBD0060 LCG state
    bool single_player = true;              // Exact source gate: 0x67F528 == 0.
    std::uint32_t global_a158c0 = 0;        // original 0xA158C0
    std::uint32_t global_a158bc = 0;        // original 0xA158BC
    std::uint32_t ground_level_bd5248 = 0;  // original 0xBD5248
    std::vector<ObjectBehaviorInput> inputs;    // scripted foreign results, in call order
    std::vector<ObjectBehaviorAction> actions;  // produced calls, in order
    std::size_t cursor = 0;
    // Live foreign call. The provider synchronizes canonical state at this point.
    std::function<ObjectBehaviorInput(ObjectBehaviorActor&,ObjectBehaviorState&,
        const ObjectBehaviorAction&)> call;
};

// Exact 0x477860 LCG step: state lives in ObjectBehaviorState::rng.
std::uint32_t object_behavior_rng_next(ObjectBehaviorState& state);

// Hold state for the foreign call an original callback reaches through the
// action's kind/site, then return the values that continue the callback.
// Strict: the site must match the next scripted input or ObjectBehaviorError
// is thrown, so a missed, extra or reordered call can never pass silently.
ObjectBehaviorInput object_behavior_call(ObjectBehaviorActor& self,
                                         ObjectBehaviorState& state,
                                         const ObjectBehaviorWorld& world,
                                         ObjectBehaviorCallKind kind,
                                         std::uint32_t site,
                                         const std::array<std::uint32_t, 6>& args,
                                         std::vector<std::uint8_t> payload = {});

namespace object_behavior {
void veh0_ai(ObjectBehaviorActor& self, ObjectBehaviorState& state, const ObjectBehaviorWorld& world, std::uint32_t param2);
void veh0_move(ObjectBehaviorActor& self, ObjectBehaviorState& state, const ObjectBehaviorWorld& world);
void slgt_ai(ObjectBehaviorActor& self, ObjectBehaviorState& state, const ObjectBehaviorWorld& world, std::uint32_t param2);
void slgt_move(ObjectBehaviorActor& self, ObjectBehaviorState& state, const ObjectBehaviorWorld& world);
} // namespace object_behavior
} // namespace dfre
