#include "dfre/motion_queries.hpp"

namespace dfre {
std::int32_t ground_height_query(const TerrainGeometry& terrain,std::int32_t x,std::int32_t y) noexcept {
    return terrain.base_height(x,y);
}
GroundSlope ground_slope_query(const TerrainGeometry& terrain,std::int32_t x,std::int32_t y) noexcept {
    const auto samples=terrain.slope(x,y);
    return {samples[0],samples[1]};
}
std::int32_t ground_height_for_actor(const TerrainGeometry& terrain,const ActorGroundQuery& query,
    std::int32_t x,std::int32_t y) noexcept {
    return query.has_ground_entity?query.entity_height:terrain.base_height(x,y); // 48775F
}
} // namespace dfre
