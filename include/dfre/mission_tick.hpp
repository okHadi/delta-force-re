#pragma once
#include "dfre/actor_behavior.hpp"
#include "dfre/actor_queries.hpp"
#include "dfre/live_world.hpp"
#include "dfre/ai.hpp"
#include "dfre/ai_update.hpp"
#include "dfre/animation_driver.hpp"
#include "dfre/gameplay_dispatch.hpp"
#include "dfre/motion_clearance.hpp"
#include "dfre/motion_queries.hpp"
#include "dfre/motion_step.hpp"
#include "dfre/projectiles.hpp"
#include "dfre/weapons.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dfre {

// Mission tick join for Delta Force (1998), using the
// AIUpdate, ActorBehavior, AnimationDriver, MotionStep, MotionQueries,
// MotionClearance, Weapons, Projectiles and WorldHits stages into the original
// game loop 0x404340 and the per-tick actor stage 0x487EA0.
// Df.exe sha256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// See docs/mission-tick.md; every stage the original calls is either executed
// here (joined modules) or completed through a required synchronous binding.

class MissionTickError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct MissionTickReport;
struct MissionTickState;
struct MissionStageCall {
    std::uint32_t address = 0;
    std::array<std::uint32_t,4> args{};
    std::uint8_t argument_count = 0;
    std::vector<std::uint8_t> payload;
};
using MissionStageProvider = std::function<std::optional<std::uint32_t>(
    MissionTickState&,const MissionStageCall&)>;
enum class MissionFrameExit : std::uint8_t { running, early_stop, game_loop, shutdown, post_menu };


// ---------------------------------------------------------------------------
// Persistent state
// ---------------------------------------------------------------------------

// 0xC23824 / 0xC23828 / 0xC2382C / 0xC23A10. WeaponState owns C23830.
struct MissionMovement {
    std::int32_t mode_c23824 = 0;
    std::int32_t prone_c23828 = 0;
    std::int32_t jump_impulse_c2382c = 0x1111;
    std::uint8_t jump_pulse_c23a10 = 0;
};

// One record of the 0x9E2E40 pending-spawn list (count 0xA15898), read by the
// 0x465BD0 pass completed by the external stage binding.
struct MissionPendingSpawn {
    std::array<std::int32_t, 3> position{};
    std::int32_t type = 0;
    std::uint32_t owner = 0;
};

// Per-actor runtime for the 0x487370 join. One entry per actor slot; fields
// are the orignal actor or instance bytes stage 1/2 do not type.
struct MissionActorRuntime {
    // Bound by the world: required when the actor +0x18 handle is nonzero.
    AnimationController* animation = nullptr;
    std::optional<AnimationDriver> driver;
    // Instance fields the animation tick reads (instance +0x48/+0x64/+0x65/
    // +0x66/+0x78/+0x79/+0x7A/+0x7B/+0x4C). Cross-module inputs.
    std::uint32_t anim_flags_48 = 0;
    std::uint8_t stance_64 = 0;
    std::uint8_t pitch_65 = 0;
    std::uint8_t switch_66 = 0;
    std::uint8_t byte78 = 0;
    std::uint8_t byte79 = 0;
    std::uint8_t aim_a_7a = 0;
    std::uint8_t aim_b_7b = 0;
    std::int32_t extra_4c = 0;
    // Instance +0x38 condition state; 0x488200 also clears the live controller.
    std::uint32_t condition_state_38 = 0;
    // Actor record fields outside the stage 1 image.
    std::uint8_t byte_6e = 0;   // +0x6E (water/slope bits 2/4/8)
    std::uint32_t roll_38 = 0;  // +0x38
    std::int32_t vertical_44 = 0;  // +0x44
    std::uint32_t ground_100 = 0;  // +0x100 ground entity handle
    std::uint32_t link_90 = 0;     // +0x90 linked actor source address
    CachedStep step_cache{};       // 0xC91E90 + group-3 slot * 0xC
};

struct MissionTickConfig {
    bool host = true;            // original 0x67F528 != 0
    bool paused = false;         // 0x6E4714
    bool demo = false;           // 0x6E4718
    bool input_disabled_67dbc8_bit1 = false;  // 0x67DBC8 bit 2 gates 0x488200
    std::optional<std::size_t> local_actor;   // 0xC23820 group-3 slot
    bool double_ammo_c23a4c = false;          // C23A4C == 1 (secondary loadout id)
    bool pansy_c23dbc9_bit0 = false;          // 67DBC9 bit 0 for WeaponState
};
// Foreign query answer for one behavior site. The provider is called with the
// live actor view at the request point (see run_behavior_callback).
struct BehaviorQueryProvider {
    std::function<std::optional<BehaviorInput>(const AiActorRef&, std::uint32_t site,
                                               const BehaviorAction& request,
                                               const BehaviorActor& view)> answer;
    std::function<bool(const AiActorRef&,const BehaviorAction&,BehaviorActor&)> effect;
};

// World bindings for the stages that read data this module does not own.
struct MissionTickWorld {
    const TerrainGeometry* terrain = nullptr;  // 0xBD5284 map
    std::int32_t water_level = 0;              // 0xBD5248
    Fixed3 world_center{};                     // 0x464EB0 edge reference
    AiBoundsSource bounds;                     // 0x487C50 object fields +0x64/+0x60
    // 0x486FD0 candidate for one target actor (object +0x10 or husk +0x14
    // selected by the target's +0x6C bit 8, like the original loop).
    std::function<std::optional<GroundEntity>(const AiActorRef&,
        const std::array<std::int32_t,3>& query_position)> ground_entity;
    // Ground object for actor +0x100 (fields +0x24/+0x28/+0x2C, +0x90, +0x3C,
    // +0x40). Null leaves the ground-object transform out.
    std::function<std::optional<GroundObject>(std::uint32_t handle)> ground_object;
    // 0x4878?? epilogue: position copy into the record at actor +0x90.
    std::function<void(std::uint32_t handle, const std::array<std::int32_t, 3>&)> linked_position;
    // 0x4889C0 actor-vs-target collision query; the OR of its results drives
    // the original two-pass loop. Bind each reached collision query.
    std::function<std::uint32_t(const AiActorRef&, const AiActorRef&)> target_collision;
    // Current frame +4 mask of the actor's CHR frame, read before the
    // animation tick (0x487382/0x4876F8, 0x48829C/0x4882EE). Bind the frame
    // mask for each reached clearance scan.
    std::function<std::uint32_t(const AiActorRef&)> frame_mask;
    // 0x484740 steady predicate.
    std::function<bool()> steady;
    // Original input test 0x47A290; resolve when the weapon body asks.
    std::function<bool(std::uint32_t row_index)> trigger_held_47a290;
    // 0x464EB0 collision queries.
    ProjectileQuery projectile_query;
    // Group-2 markers, 0x78F280 lists and foreign pointer resolvers.
    BehaviorWorld behavior_world;
    // Behavior foreign query answers.
    BehaviorQueryProvider behavior_query;
    // Complete each reached foreign stage synchronously. nullopt is unbound.
    MissionStageProvider external_stage;
    // Complete each emitted weapon effect.
    std::function<bool(MissionTickState&,const WeaponEvent&)> weapon_effect;
    // Completes unowned dispatch intents; receives full semantic arguments.
    std::function<std::optional<std::uint32_t>(MissionTickState&,const DispatchCall&)> dispatch_effect;
};

struct MissionTickState {
    MissionTickConfig config;
    MissionTickWorld world;

    // Original actor storage; mutated in place and kept across ticks.
    AiActorTables tables;
    MissionActorImages stored_actor_images;
    std::array<std::vector<ActorPersonEncoding>,4> person_encoding;
    AiUpdateState ai;                     // includes the shared target list
    AiItemDefinitionTable definitions;    // +0x28/+0x9C read by the AI tick,
                                          // +0x84/+0x90 callback slots
    std::vector<AiPersonState> people;    // 0x794680 pool, persistent
    std::array<std::vector<std::optional<MissionActorRuntime>>, 5> runtimes;
    std::array<std::vector<BehaviorActorExtra>, 5> actor_extras;
    std::vector<BehaviorPersonExtra> person_extras;

    BehaviorState behavior;               // 0xBD0060 RNG + per-callback scratch
    WeaponState weapons;                  // C239B8.. and the player weapon block
    std::vector<ProjectileEntity> projectiles;
    std::vector<MissionPendingSpawn> pending_spawns;

    DispatchGlobals globals;              // command word, pulses, command tick
    DispatchActor dispatch_actor;         // live view written by 0x477D40
    MissionMovement movement;
    std::uint32_t scope_c23a24 = 0;

    std::uint32_t tick_67dbf4 = 0;        // 0x67DBF4, passed to 0x487EA0
    std::uint32_t tick_6e4720 = 0;        // 0x6E4720 (not paused)
    std::uint32_t tick_6e4724 = 0;        // 0x6E4724 (not paused, not demo)
    std::uint32_t frame_counter_4ced10 = 0;  // 0x4CED10, 0x3E second counter
    std::uint32_t mission_timer_67dbe8 = 0;  // 0x67DBE8 decrement gate
    std::uint32_t frame_event_67dbf0 = 0;
    std::uint32_t frame_work_67dbcc = 0;
    std::uint32_t previous_work_67dbd0 = 0;
    std::uint32_t last_clock_67dbd4 = 0;
    std::uint32_t clock_delta_67dbd8 = 0;
    std::uint32_t statistic_67dbdc = 0;
    std::uint32_t clock_491014 = 0;
    std::uint32_t game_type_67f53c = 0;
    std::uint32_t shutdown_503424 = 0;
    std::uint32_t message_idle_bc5d20 = 0;
    std::uint32_t send_throttle_bc5d10 = 0;
    std::uint32_t send_interval_67f538 = 0;
    std::uint32_t player_sequence_bcfc28 = 0;

    // Callback bodies for the 0x487EA0 +0x110 (update) and +0x10C (secondary)
    // slots, keyed by the original callback address. The four human records
    // are installed by install_human_behavior_callbacks.
    using ActorCallback = std::function<void(MissionTickState&, const AiActorRef&,
                                             const AiActorAction&)>;
    std::map<std::uint32_t, ActorCallback> update_callbacks;
    std::map<std::uint32_t, ActorCallback> secondary_callbacks;

    // Current report while a callback runs; restored on return or exception.
    MissionTickReport* current_report = nullptr;

    MissionActorRuntime& runtime_for(const AiActorRef& ref);
    BehaviorActorExtra& extra_for(const AiActorRef& ref);
    BehaviorPersonExtra& person_extra(std::size_t person_slot);
    AiActorState* actor_at(const AiActorRef& ref);
    const AiActorState* actor_at(const AiActorRef& ref) const;
    // Binds one actor's animation exactly like the world init does: controller
    // plus the compiled driver action list (build_animation_driver_actions).
    void bind_animation(const AiActorRef& ref, AnimationController& controller,
                        std::vector<AnimationDriverAction> actions);
};

// Builds the runtime vectors for the current table sizes. `behavior_rng` is
// the live 0xBD0060 value. Tables and people move in.
void init_mission_tick(MissionTickState& state, MissionTickConfig config,
                       AiActorTables tables, AiItemDefinitionTable definitions,
                       std::vector<AiPersonState> people, std::uint32_t behavior_rng);

// Seed from every current initial actor record, including factory-created players.
void init_mission_tick(MissionTickState& state,MissionTickConfig config,
    const MissionRuntimeState& initial,std::uint32_t behavior_rng);
AiActorImage snapshot_mission_actor(const MissionTickState& state,const AiActorRef& ref);
MissionActorImages snapshot_mission_actors(const MissionTickState& state);
// Record and execute a weapon effect at its source call site.
void mission_weapon_effect(MissionTickState& state,const WeaponEvent& event);

// Bind the NPC helpers to current records and the shared behavior RNG.
// The source supplies each actor's current geometry and sight bindings.
// The sink executes effect sites outside ActorQueries.
void bind_mission_actor_queries(MissionTickState& state,
    std::function<ActorQueryData(const AiActorRef&)> source,
    std::function<bool(const AiActorRef&,const BehaviorAction&,BehaviorActor&)> sink);

// ---------------------------------------------------------------------------
// One tick
// ---------------------------------------------------------------------------

enum class MissionTraceKind : std::uint8_t {
    stage,             // an internal stage ran (address in the trace)
    external_stage,    // synchronous foreign stage binding
    dispatch_call,     // one 0x477D40 call intent from the command pump
    mark,              // 0x456E30 profiler mark
    ai_call,           // typed call returned by tick_ai
    actor_update,      // 0x487370 join ran for an actor
    actor_callback,    // actor callback executed
    callback_missing,  // callback address has no registered body
    behavior_query,    // behavior requested a foreign query
    behavior_effect,   // behavior reported an effect
    weapon_event,      // WeaponEvent drained from WeaponState
    projectile_event,  // ProjectileEvent from tick_projectile
    unresolved,        // missing external body with the exact call
    effect,            // an executed original side effect
};

struct MissionTraceEntry {
    MissionTraceKind kind = MissionTraceKind::stage;
    std::uint32_t address = 0;
    std::uint8_t group = 0;
    std::size_t index = 0;
    std::array<std::uint32_t, 4> args{};
};

// One command event drained by the 0x479450 pump.
struct MissionCommandEvent {
    std::uint32_t id = 0;
    std::int32_t value = 0;
};

struct MissionTickInputs {
    // Command events drained by 0x479450 in queue order. `dispatch_command`
    // runs gameplay_dispatch's 0x477D40 against the live globals and actor
    // view at the pump stage (the original reads current fields).
    std::vector<MissionCommandEvent> commands;
    std::function<DispatchResult(std::uint32_t, std::int32_t)> dispatch_command;
    // Live 479450 source. Query again after each dispatch and its effects.
    // The argument is the tick captured when the command stage starts.
    std::function<std::optional<MissionCommandEvent>(std::uint32_t)> next_command;
    // Precomputed alternative: already-dispatched results in event order.
    std::vector<DispatchResult> dispatch;
};

struct MissionTickReport {
    bool paused = false;
    bool demo = false;
    std::uint32_t result = 0;             // original 404340 return value
    MissionFrameExit exit_reason = MissionFrameExit::running;
    std::vector<MissionStageCall> frame_calls;
    std::uint32_t tick = 0;                // 0x67DBF4 after the increment
    std::vector<MissionTraceEntry> trace;
    std::vector<std::uint32_t> ai_marks;   // 0x456E30 marks from tick_ai
    std::vector<AiActorAction> ai_actions; // as reported by tick_ai
    std::size_t per_actor_updates = 0;
    std::size_t callbacks_run = 0;
    std::size_t callbacks_missing = 0;
    std::size_t behavior_queries = 0;
    std::size_t projectile_ticks = 0;
};

// Original 404340 lifecycle. The provider supplies called stage bodies;
// 43F560 is owned here. The report preserves return status and call order.
MissionTickReport mission_frame(MissionTickState& state,const MissionStageProvider& stages);

// Original 0x484740 scope/aim/control gate.
std::uint32_t mission_steady(const MissionTickState& state);

// Isolated weapon fixture helper. Supply held input explicitly; this helper
// uses WeaponState burst storage and does not bind the full player prefix.
void mission_weapon_part(MissionTickState& state,bool trigger_held_fixture);

// Join the lifecycle to the command, weapon and actor stages. Bind the other
// reached addresses through world.external_stage before running the game.
MissionTickReport mission_tick(MissionTickState& state, const MissionTickInputs& inputs);

// Original per-actor update 0x487370 as one call, exposed for the checker and
// for tests. `full_pass` is (tick & 3) == 3, `odd` is (tick & 1) != 0.
void mission_actor_update(MissionTickState& state, const AiActorRef& ref, std::uint32_t tick,
                          MissionTickReport& report);

// Original local-player control 0x488200. Called by the AI tick on odd ticks.
void mission_local_control(MissionTickState& state, MissionTickReport& report);

// Original behavior callback dispatch: 0x45B590/0x45B5A0/0x45B5B0/0x45BDF0 are
// installed for the human records. Bind all reached callback addresses.
void install_human_behavior_callbacks(MissionTickState& state);
// Registers one callback body for the 0x487EA0 +0x110 (update) or +0x10C
// (secondary) slot. `secondary` selects the 0x10C slot.
void register_actor_callback(MissionTickState& state, std::uint32_t address, bool secondary,
                             std::function<void(MissionTickState&, const AiActorRef&,
                                                const AiActorAction&)> body);

// Runs one callback at the scheduler call point. The scheduler owns countdowns.
void run_actor_action(MissionTickState& state, const AiActorAction& action,
                      MissionTickReport& report);

} // namespace dfre
