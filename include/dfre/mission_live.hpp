#pragma once
#include "dfre/mission_animation.hpp"
#include "dfre/mission_geometry.hpp"
#include "dfre/mission_tick.hpp"

namespace dfre {
// Bind after init_mission_tick. Keep state, animations and geometry alive.
void bind_mission_animations(MissionTickState& state,MissionAnimations& animations);
void bind_mission_geometry(MissionTickState& state,const MissionGeometry& geometry,
    MissionGeometryMatrixSource matrices={});
// Use defined zero state at matrix callback boundaries, including copied flags.
MissionGeometryMatrixSource initialized_mission_matrices(const MissionRuntimeState& initial,
    const MissionAssetCache& cache,const MissionAnimations& animations);
ActorQueryData mission_actor_query_data(MissionTickState& state,const AiActorRef& self,
    const MissionGeometry& geometry,const AssetBytes& height_bounds,
    std::vector<ActorZoneRect> zones,bool double_score,
    MissionGeometryMatrixSource matrices);
} // namespace dfre
