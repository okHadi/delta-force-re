#pragma once
#include "dfre/events.hpp"
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionObjectTargetBindings {
    // Required for each reached physical group3 slot past its counted prefix.
    // Return current shared bytes. Keep this provider bound to the live pool.
    std::function<AiActorImage(std::size_t index)> physical_slot;
    // Required 42E2B0 body. Read current actors at this call boundary.
    std::function<std::int32_t(const AiActorRef& shooter,const AiActorRef& target)> sight_line;
    // Read definition +78, current actor +80, then frame +10.
    std::function<std::uint32_t(const AiActorRef& target)> frame_weight_10;
};
struct MissionObjectTarget {std::uint32_t source_actor=0,score=0;};
// 44F540. Read256 physical slots; use canonical rows for the counted prefix.
MissionObjectTarget mission_object_target(MissionTickState&,const AiActorRef& shooter,
    const MissionObjectTargetBindings&);
// 42EDD0. EventStage owns the current128-word6E0050 table.
void mission_object_region_mark(EventStage&,std::uint32_t region,std::uint32_t bit);
}
