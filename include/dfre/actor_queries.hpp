#pragma once
#include "dfre/actor_behavior.hpp"
#include "dfre/ai_update.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dfre {

// Real NPC query and attack helpers used by the human behavior callbacks,
// rebuilt from .local/reports/actor-queries-and-projectile-hits-root.json:
//   45E280 best-target search        45F090 teammate/alert selection
//   45F2C0 combat/attack step        45FD90 movement zone step
//   45FE30 second zone step          45F010 nearby predicate
//   45FB00 ray target cast           45EDD0 walkable facing scan
//   460410 initial animation id      477770 facing angle
// The helpers mutate the group-3 actor/person records in place, share the
// original 0xBD0060 RNG, and report calls into other modules as typed actions
// through BehaviorState. Foreign data (zones, terrain, line-of-sight, model
// sine/cosine tables, weapon frame factors) arrives through ActorQueryData.
// Df.exe sha256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.

class ActorQueryError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Original 0x8E48B0 record, stride 0x20, count 0x4F8D44.
struct ActorZoneRect {
    std::uint32_t flags = 0;  // +0x00; 45FE30 requires mask 2
    std::uint32_t type = 0;   // +0x04; compared with actor +0x6E >> 16
    std::int32_t x_min = 0, x_max = 0;  // +0x08 / +0x0C
    std::int32_t y_min = 0, y_max = 0;  // +0x10 / +0x14
    std::int32_t z_min = 0, z_max = 0;  // +0x18 / +0x1C
};

// Group-3 actor table view. people/…extras are indexed by actor person_slot.
struct ActorQueryTable {
    std::vector<AiActorState>* actors = nullptr;
    std::vector<AiPersonState>* people = nullptr;
    std::vector<BehaviorActorExtra>* actor_extras = nullptr;
    std::vector<BehaviorPersonExtra>* person_extras = nullptr;

    std::size_t size() const noexcept { return actors ? actors->size() : 0; }
    AiPersonState& person(std::size_t index) const;
    BehaviorPersonExtra& person_extra(std::size_t index) const;
};

// Foreign data the query ports read but do not own.
struct ActorQueryData {
    std::vector<ActorZoneRect> zones;  // 0x8E48B0; count is zones.size() (<= 0x4F8D44)
    std::int32_t water_level = 0;      // 0xBD5248
    bool double_score_67f548_bit80 = false;      // 0x67F548 bit 0x80
    bool local_player_excluded_67dbc9_bit8 = false;  // 0x67DBC9 bit 8
    // 0x477820 quantized sine/cosine pair (out_a from 0xBD0068, out_b from the
    // table behind 0x4FB2F8).
    std::function<void(std::uint32_t angle, std::int32_t& out_a, std::int32_t& out_b)> sin_cos_477820;
    // 0x4776C0 sine/cosine pair used by the ray cast.
    std::function<void(std::uint32_t angle, std::int32_t& out_sin, std::int32_t& out_cos)> sin_cos_4776c0;
    // 0x47DF40 terrain height at (x, y).
    std::function<std::int32_t(std::int32_t x, std::int32_t y)> terrain_height;
    // 0x42E2B0 line-of-sight between the actor and group-3 record `other`
    // (exact return code; 0 < code < 3 means a hit on the target).
    std::function<std::int32_t(std::size_t other)> sight_line;
    // Signed high word at frame +0x14 for 45F2C0 aim error.
    std::function<std::int32_t(std::size_t other)> weapon_frame_14;
    // Raw signed word at frame +0x10 for 45E280 target weight.
    std::function<std::int32_t(std::size_t other)> weapon_frame_10;
    std::optional<std::size_t> local_player;
    // Raw original table address for a group-3 index (0x7CCA80 + index*0x114);
    // 45E280 returns it and the behavior stores it in person +0x44.
    std::function<std::uint32_t(std::size_t index)> raw_pointer;
};

// 477770 facing angle from two signed deltas. The integer axis-aligned paths
// and general paths use the original x87 arithmetic on Intel targets.
std::uint32_t query_angle_477770(std::int32_t dx, std::int32_t dy);

// 477620 integer square root with the original final rounding step.
std::uint32_t query_isqrt_477620(std::uint32_t value);

// 45F010: 1 when no other live actor is within 0x20000 on all three axes.
std::uint32_t query_nearby_45f010(ActorQueryTable& table, std::size_t self);

// 45F090: teammate/alert selection; writes the selected teammates' person
// records and reports its 44A3E0 calls. Returns 0..3.
std::uint32_t query_selector_45f090(ActorQueryTable& table, std::size_t self,
                                    BehaviorState& state);

// 45FD90 / 45FE30: zone scans. On a hit they set person +0x9C = 2, face the
// zone centre through 477770 and (45FD90 only) report 44A3E0.
void query_zone_45fd90(ActorQueryTable& table, std::size_t self, const ActorQueryData& data,
                       BehaviorState& state);
void query_zone2_45fe30(ActorQueryTable& table, std::size_t self, const ActorQueryData& data,
                        BehaviorState& state);

// 460410: animation id for the initial pose.
std::uint32_t query_anim_id_460410(const AiActorState& actor, std::uint32_t param2,
                                   std::uint32_t param3, std::uint32_t param4,
                                   std::int32_t water_level,std::optional<std::uint32_t> incoming_eax=std::nullopt);

// 45E280: best visible enemy target. Returns the group-3 index and the
// weighted distance in out_distance; reports 44A3E0/445CE0(message 1).
std::optional<std::size_t> query_target_search_45e280(ActorQueryTable& table, std::size_t self,
                                                      const ActorQueryData& data,
                                                      BehaviorState& state,
                                                      std::uint32_t& out_distance);

// 45FB00: first same-type actor on the aim ray within (max_range, max_radius).
std::optional<std::size_t> query_ray_45fb00(ActorQueryTable& table, std::size_t self,
                                            const ActorQueryData& data,
                                            std::uint32_t max_range, std::uint32_t max_radius);

// 45EDD0: walkable facing scan (uses the shared RNG and person +0x24 bits
// 4/8). Returns the chosen full-turn yaw.
std::uint32_t query_facing_45edd0(ActorQueryTable& table, std::size_t self,
                                  const ActorQueryData& data, BehaviorState& state);

// 45F2C0: combat/attack step. Writes actor yaw/pitch and person +0x34, and
// reports the original 45E7A0 shot call as an effect.
void query_combat_step_45f2c0(ActorQueryTable& table, std::size_t self, const ActorQueryData& data,
                              BehaviorState& state);

// BehaviorQueryProvider bridge: answers the BehaviorState query sites with the
// real helpers above.
class ActorQueryProvider {
public:
    ActorQueryProvider(ActorQueryTable table, ActorQueryData data);
    // Returns an answer for a behavior query site, or nullopt when the site is
    // not owned by this provider (effects and the weapon reads).
    std::optional<BehaviorInput> answer(const AiActorRef& ref, std::uint32_t site,
                                        const BehaviorAction& request, BehaviorActor& view,
                                        BehaviorState& state);
    // Performs the effect sites this module owns (kCombatStep, kZoneStep,
    // kZoneStep2); returns true when the site was handled.
    bool perform_effect(const AiActorRef& ref, const BehaviorAction& action, BehaviorState& state);
private:
    ActorQueryTable table_;
    ActorQueryData data_;
};

} // namespace dfre
