#pragma once
#include "dfre/mission_geometry.hpp"
#include "dfre/mission_tick.hpp"
namespace dfre {
struct MissionObjectSightBindings {
    // 47E4B0 uses the current source position at this call boundary.
    std::function<std::int32_t(const AiActorRef&,const std::array<std::int32_t,3>& direction,std::int32_t length)> terrain;
    // 42DB80 receives a cached signed count. Group1 is called before group0.
    std::function<std::int32_t(std::size_t group,std::int32_t count,const WorldHitQuery&,WorldModelHit&)> model_list;
    std::function<std::int32_t(std::size_t group)> current_count;
    std::function<void(std::uint32_t source_address)> before_read;
};
// 42E2B0. Return the exact signed source result.
std::int32_t mission_object_sight(MissionTickState&,const AiActorRef& source,const AiActorRef& target,
    const MissionObjectSightBindings&);
// Keep state, geometry, bounds and matrix providers alive. This factory binds
// real terrain and per-record world-hit bodies to the same canonical actors.
MissionObjectSightBindings mission_object_sight_bindings(MissionTickState&,const MissionGeometry&,
    const AssetBytes& height_bounds,MissionGeometryMatrixSource matrices={});
}
