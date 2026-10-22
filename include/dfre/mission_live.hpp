#pragma once
#include "dfre/mission_animation.hpp"
#include "dfre/mission_geometry.hpp"
#include "dfre/mission_tick.hpp"

namespace dfre {
// Bind after init_mission_tick. Keep state, animations and geometry alive.
void bind_mission_animations(MissionTickState& state,MissionAnimations& animations);
// Apply a current actor image after a raw-record helper. Keep active person
// and character bindings unchanged; use their allocation APIs to replace them.
void apply_mission_actor_image(MissionTickState& state,const AiActorRef& ref,const AiActorImage& image);
// Resolve the local actor's current person +6C for every weapon access.
std::uint32_t& mission_player_burst(MissionTickState& state);
void bind_mission_player_burst(MissionTickState& state);
void bind_mission_geometry(MissionTickState& state,const MissionGeometry& geometry,
    MissionGeometryMatrixSource matrices={});
using MissionCollisionEffect = std::function<std::optional<std::uint32_t>(
    MissionTickState&,const ActorCollisionCall&)>;
void bind_mission_actor_collision(MissionTickState& state,const MissionGeometry& geometry,
    MissionCollisionEffect effect,MissionGeometryMatrixSource matrices={});
// Use defined zero state at matrix callback boundaries, including copied flags.
MissionGeometryMatrixSource initialized_mission_matrices(const MissionRuntimeState& initial,
    const MissionAssetCache& cache,const MissionAnimations& animations);
ActorQueryData mission_actor_query_data(MissionTickState& state,const AiActorRef& self,
    const MissionGeometry& geometry,const AssetBytes& height_bounds,
    std::vector<ActorZoneRect> zones,bool double_score,
    MissionGeometryMatrixSource matrices);
} // namespace dfre
