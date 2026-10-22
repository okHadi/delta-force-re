#pragma once
#include "dfre/mission_live.hpp"
#include "dfre/mission_projectile_stage.hpp"

namespace dfre {
// External cells not held by MissionTickState. Read them from their live owner.
struct MissionAreaLiveGlobals {
    std::uint8_t rules_67f549 = 0;
    std::uint32_t alternate_listener_6ba3ec = 0;
    MissionListenerPose alternate_pose;
    std::optional<MissionAreaDispatch> incoming_dispatch;
};
struct MissionAreaLiveBindings {
    std::function<MissionAreaLiveGlobals(const MissionTickState&)> globals;
    std::function<std::int32_t(MissionTickState&,const AiActorRef&,const Fixed3&,const Fixed3&)> segment;
    std::function<void(MissionTickState&,const AiActorRef&,std::uint32_t callback,std::uint32_t message)> update;
    std::function<void(MissionTickState&)> local_hit;
    std::function<void(MissionTickState&,const MissionListenerPose&)> listener;
};
// Stage-provider adapter for 45E050 and 465BD0. Other addresses return nullopt.
// Raw helper writes reach canonical state before each foreign call and on errors.
// Keep geometry and callbacks alive through the synchronous call.
std::optional<std::uint32_t> mission_area_live_stage(MissionTickState& state,
    const MissionGeometry& geometry,const MissionStageCall& call,const MissionAreaLiveBindings& bindings);
} // namespace dfre
