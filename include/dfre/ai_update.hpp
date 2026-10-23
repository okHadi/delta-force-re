#pragma once
#include "dfre/ai.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace dfre {

// Original per-tick actor stage 0x487EA0 and the per-actor target-list rebuild
// 0x487C50 (entered through 0x487E10, thunk 0x488970). The stage mutates the
// typed actor records in place and reports every call it would make into code
// this module does not own as typed actions for the root runtime.
//
// Evidence: .local/reports/ai-next-stage.json (batch decompile of 0x487EA0,
// 0x487C50, 0x45D970, 0x404E30, 0x486AF0). Field meanings beyond the observed
// reads and writes stay unknown and are preserved.

struct AiActorRef {
    std::uint8_t group = 0;  // BMS group 0..3, or kAiExtraTableGroup for 0x822E80
    std::size_t index = 0;
};

constexpr std::uint8_t kAiExtraTableGroup = 4;
constexpr std::uint32_t kAiDefaultCallback = 0x4563A0;  // single RET; skipped by 0x487EA0

// 0x487C50 reads these from the object at actor +0x10: fields +0x64 and +0x60.
// The object registry is owned by the model/resource module, so the root
// supplies the values through this source. A zero handle skips the record.
struct AiActorBounds {
    std::int32_t radius_64 = 0;
    std::int32_t height_60 = 0;
};
using AiBoundsSource = std::function<AiActorBounds(std::uint32_t object_handle)>;

struct AiUpdateState {
    std::uint32_t call_counter = 0;          // original 0xC92A98
    bool paused = false;                     // original 0x6E4714 != 0
    bool demo = false;                       // original 0x6E4718 != 0
    std::optional<std::size_t> local_actor;  // original 0xC23820; group 3 index
    std::vector<AiActorRef> target_list;     // original 0xC8FF50 list / 0xC92A90 count
};

struct AiActorTables {
    std::array<std::vector<AiActorState>, 4> groups;  // BMS group order
    std::vector<AiActorState> extra;                  // original 0x822E80, 0x400 slots
};

enum class AiActorActionKind {
    update,     // call record +0x110 as (record, 0)
    secondary,  // call record +0x10C as (record)
};

struct AiActorAction {
    AiActorActionKind kind = AiActorActionKind::update;
    AiActorRef actor{};
    std::uint32_t target = 0;  // original function pointer field value
};

struct AiTickCall {
    enum class Kind {
        per_actor_update,      // 0x487370(index, record, tick)
        local_player_control,  // 0x488200; odd ticks only
        fixed_0045e050,        // 0x45E050; odd ticks only
        fixed_00464eb0,        // 0x464EB0
        fixed_00465bd0,        // 0x465BD0
    };
    Kind kind = Kind::per_actor_update;
    AiActorRef actor{};
};

struct AiTickReport {
    std::vector<AiTickCall> calls;              // execution order
    std::vector<AiActorAction> actions;         // execution order
    std::vector<std::uint32_t> profiler_marks;  // 0x456E30 arguments in order
};

// Run each native call at its original point, before later actor writes.
// Callers keep actor slots stable; callbacks can change records or add slots.
struct AiTickHooks {
    std::function<void(const AiTickCall&)> on_call;
    std::function<void(const AiActorAction&)> on_action;
    std::function<void(std::uint32_t)> on_mark;
    // Complete the side storage clear at488138 before the secondary callback read.
    std::function<void(const AiActorRef&)> on_clear;
};

// Original 0x487C50 for one actor: clears +0x108, stores the shared-list start
// at +0x104, then for definition types 1, 3, and 6 appends in-range records of
// group 1 and then group 0 to the shared list (cap 2000).
void rebuild_actor_targets(AiActorTables& tables, AiActorRef actor, AiUpdateState& state,
                           const AiItemDefinitionTable& definitions,
                           const AiBoundsSource& bounds);

// Original 0x487E10 (thunk 0x488970): zeroes the list count and call counter,
// then rebuilds every record of group 3 and then group 0 whose first dword is
// nonzero and whose +0x6C byte does not have bit 2 set.
void rebuild_target_lists(AiActorTables& tables, AiUpdateState& state,
                          const AiItemDefinitionTable& definitions,
                          const AiBoundsSource& bounds);

// Original 0x487EA0: one tick. tick is the original 67DBF4 counter.
AiTickReport tick_ai(AiActorTables& tables, AiUpdateState& state,
                     const AiItemDefinitionTable& definitions,
                     const AiBoundsSource& bounds, std::uint32_t tick);

AiTickReport tick_ai(AiActorTables& tables, AiUpdateState& state,
                     const AiItemDefinitionTable& definitions,
                     const AiBoundsSource& bounds, std::uint32_t tick,
                     const AiTickHooks& hooks);

} // namespace dfre
