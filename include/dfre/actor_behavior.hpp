#pragma once
#include "dfre/ai.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dfre {

// NPC and player behavior callbacks from the item `ai_function` registry.
// Rebuilt from the original callback bodies (source batch
// .local/reports/callbacks-actor-behaviors-20261016.json):
//   45B590 null    writes actor +0xFC = 0x1000000
//   45B5A0 org0    RET
//   45B5B0 org1    human person behavior
//   45BDF0 plyr    local player behavior
// The callbacks mutate the typed actor/person state in place and report every
// call into code this module does not own as an ordered action. Calls whose
// results drive the original branches consume ordered inputs: the original RNG
// is state here (exact 0x477860 LCG), while foreign query results (target
// search, animation, weapon table, sight, facing) arrive through BehaviorState.
// No behavior is invented: branches, writes, and call order mirror the original
// assembly (scratch disassembly in .local/actor_behavior-work/).

class BehaviorError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Original group-3 actor index (the callback messages carry actor-3 indexes).
struct BehaviorActorRef {
    std::uint8_t group = 3;
    std::size_t index = 0;
};

// Actor fields read/written by the behavior callbacks that stage 1 does not
// type: +0x6D, +0x74, +0x84, +0x94, +0xF8. The original's `inc word +0x2E`
// and `add word +0x32,0x80` are the high words of position Z and yaw, so they
// are modeled as +0x10000 on position[2] and +0x800000 on angle_yaw.
struct BehaviorActorExtra {
    std::uint8_t field_6d = 0;
    std::uint16_t field_74 = 0;
    std::uint32_t field_84 = 0;
    std::uint32_t field_94 = 0;
    std::uint32_t field_f8 = 0;
};

// Person fields read/written by the behavior callbacks that stage 1 does not
// type. Stage 1 typed fields keep their names in AiPersonState.
struct BehaviorPersonExtra {
    std::uint32_t field_28 = 0;  // +0x28 movement state (1/2 observed)
    std::uint32_t field_2c = 0;  // +0x2C state timer
    std::uint32_t field_30 = 0;  // +0x30 threshold
    std::uint32_t field_34 = 0;  // +0x34 threshold accumulate
    std::uint32_t field_38 = 0;  // +0x38 decision timer
    std::uint32_t field_3c = 0;  // +0x3C decision timer
    std::uint32_t field_40 = 0;  // +0x40 attack timer
    std::uint32_t field_44 = 0;  // +0x44 target actor raw pointer
    std::uint32_t field_48 = 0;  // +0x48 target position copy x
    std::uint32_t field_4c = 0;  // +0x4C target position copy y
    std::uint32_t field_50 = 0;  // +0x50 target position copy z
    std::uint32_t field_6c = 0;  // +0x6C pending combat flag (read by org1)
    std::uint32_t field_a0 = 0;  // +0xA0
    // +0xA4 is a u32 for the behavior stage (values 0/1/3). AiPersonState
    // carries the stage-1 boolean view in tail_flag; keep the two in sync.
    std::uint32_t field_a4 = 0;  // +0xA4
};

// One behavior target. The root keeps the actor, its person record, and the
// behavior extras; the callbacks mutate all four in place. raw is the original
// actor pointer value the message packets derive their group-3 index from.
struct BehaviorActor {
    BehaviorActorRef ref{};
    std::uint32_t raw = 0;
    AiActorState& actor;
    AiPersonState& person;
    BehaviorActorExtra& actor_extra;
    BehaviorPersonExtra& person_extra;
};

// Marker (group 2) fields that 0x45C6A0 and the plyr marker loop read:
// +0x08 object id, +0x24/+0x28/+0x2C position, +0x30 yaw, +0x7C radius.
struct BehaviorMarker {
    std::uint32_t object_id = 0;
    std::uint32_t position[3]{};
    std::uint32_t yaw = 0;
    std::uint32_t radius = 0;
};

// Original 0x78F280 list record: flags, count, up to 32 marker indexes.
struct BehaviorMarkerList {
    std::uint32_t flags = 0;
    std::uint32_t count = 0;
    std::array<std::uint32_t, 32> indices{};
};

// Fields the behavior callbacks read from the current target actor:
// +0x70, +0x6C, +0xF8, +0x24/+0x28/+0x2C.
struct BehaviorTargetView {
    std::uint32_t field_70 = 0;
    std::uint8_t field_6c = 0;
    std::uint32_t field_f8 = 0;
    std::uint32_t position[3]{};
};

// Original call sites. Query sites consume an input with the same site id in
// call order; effect sites only produce an action. The weapon sites are table
// reads, not calls: the action carries the read index and the input carries
// the value at that index.
namespace behavior_site {
constexpr std::uint32_t kAnimInit = 0x460410;          // query: initial animation id
constexpr std::uint32_t kCombatStep = 0x45F2C0;        // effect: combat/attack step
constexpr std::uint32_t kCombatResult = 0x45F2C1;      // query: person +0x6C after kCombatStep
constexpr std::uint32_t kTargetSearch = 0x45E280;      // query: raw target pointer + distance
constexpr std::uint32_t kSelector = 0x45F090;          // query: 0..3 selector
constexpr std::uint32_t kNearby = 0x45F010;            // query: nonzero when no live actor is near
constexpr std::uint32_t kRay = 0x45FB00;               // query: nonzero when the shot is blocked
constexpr std::uint32_t kFacing = 0x45EDD0;            // query: facing angle
constexpr std::uint32_t kSight = 0x42E2B0;             // query: line-of-sight result
constexpr std::uint32_t kAngle = 0x477770;             // query: FPU-path angle only
constexpr std::uint32_t kWeaponFlags810 = 0x45C832;    // query: weapon-table flags read by 45C810
constexpr std::uint32_t kWeaponFlags880 = 0x45C8A8;    // query: weapon-table flags read by 45C880
constexpr std::uint32_t kWeaponStep = 0x45B953;        // query: weapon-table step read by org1
constexpr std::uint32_t kSetAnimation = 0x42CEF0;      // effect: set animation
constexpr std::uint32_t kNotifyEvent11 = 0x442760;     // effect: event 0x11 [actor index, anim]
constexpr std::uint32_t kMessage = 0x445CE0;           // effect: message 0x25 [actor index, msg]
constexpr std::uint32_t kRegionBit = 0x42EDD0;         // effect: region bit set
constexpr std::uint32_t kRegionBit2 = 0x42EE00;        // effect: region bit set
constexpr std::uint32_t kRegionBit3 = 0x42EE30;        // effect: region bit set
constexpr std::uint32_t kTypeSlotMark = 0x44A3E0;      // effect: type slot +0x24 = 2
constexpr std::uint32_t kZoneStep = 0x45FD90;          // effect: zone scan step
constexpr std::uint32_t kZoneStep2 = 0x45FE30;         // effect: zone scan step
constexpr std::uint32_t kPlayerSetup = 0x442810;       // effect: player slot setup
constexpr std::uint32_t kGroundSnap = 0x4604C0;        // effect: snap actor Z to ground
} // namespace behavior_site

enum class BehaviorCallKind : std::uint8_t { query, effect };

struct BehaviorAction {
    BehaviorCallKind kind = BehaviorCallKind::effect;
    std::uint32_t site = 0;
    std::array<std::uint32_t, 3> args{};
};

struct BehaviorInput {
    std::uint32_t site = 0;
    std::uint32_t value = 0;   // query result, weapon-table value, or angle
    std::uint32_t value2 = 0;  // 45E280 only: distance
};

struct BehaviorState {
    std::uint32_t rng = 0;          // original 0xBD0060 LCG state
    bool host = true;              // original 0x67F528 != 0
    std::vector<BehaviorInput> inputs;    // scripted foreign results, in call order
    std::vector<BehaviorAction> actions;  // produced calls/reads, in order
    std::size_t cursor = 0;
    std::function<BehaviorInput(const BehaviorAction&)> on_query;
    std::function<void(const BehaviorAction&)> on_effect;

    std::uint32_t take(std::uint32_t site);  // strict site check
    void emit(BehaviorCallKind kind, std::uint32_t site,
              std::uint32_t a1 = 0, std::uint32_t a2 = 0, std::uint32_t a3 = 0);
};

// Marker/waypoint data and foreign pointer resolution for the callbacks.
struct BehaviorWorld {
    std::vector<BehaviorMarker> markers;    // indexed as the original group-2 table
    std::vector<BehaviorMarkerList> lists;  // original 0x78F280 table
    // person +0x44 raw value -> target actor view
    std::function<std::optional<BehaviorTargetView>(std::uint32_t)> resolve_target;
    // person +0xAC raw value for destination state 3 -> marker index
    std::function<std::optional<std::size_t>(std::uint32_t)> resolve_marker;
};

// Original callback bodies.
void behavior_null(BehaviorActor& actor);
void behavior_org0(BehaviorActor& actor);
void behavior_org1(BehaviorActor& actor, const BehaviorWorld& world, BehaviorState& state,
                   std::uint32_t param2);
void behavior_plyr(BehaviorActor& actor, const BehaviorWorld& world, BehaviorState& state);

// Original AI-internal helpers, exposed for focused checks.
std::uint32_t behavior_rng_next(BehaviorState& state);                  // 0x477860
std::uint32_t behavior_octagon2(std::int32_t a, std::int32_t b);        // inlined 2D octagon
std::uint32_t behavior_octagon3(std::int32_t a, std::int32_t b, std::int32_t c);  // 0x477670
std::uint32_t behavior_facing_angle(std::int32_t dx, std::int32_t dy, BehaviorState& state);  // 0x477770
bool behavior_waypoint_step(BehaviorActor& actor, const BehaviorWorld& world,
                            BehaviorState& state);                      // 0x45B400
// 0x45C6A0 marker target query; outputs are untouched when it returns false.
struct BehaviorMarkerTarget {
    std::uint32_t angle = 0;
    std::uint32_t distance = 0;
    std::uint32_t radius = 0;
    std::uint32_t yaw = 0;
};
bool behavior_marker_query(BehaviorActor& actor, const BehaviorWorld& world,
                           BehaviorState& state, BehaviorMarkerTarget& out);  // 0x45C6A0
std::uint32_t behavior_step(BehaviorActor& actor, std::uint32_t value);  // 0x45B3A0

// Record images for tests and the checker. The person image is the stage-1
// image with the behavior extras written at their original offsets.
AiActorImage behavior_actor_image(const BehaviorActor& actor);
AiPersonImage behavior_person_image(const BehaviorActor& actor);

} // namespace dfre
