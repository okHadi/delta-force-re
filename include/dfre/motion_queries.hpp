#pragma once
#include "dfre/terrain.hpp"
#include <cstdint>

// Explicit terrain and actor ground queries for the motion step.
// Df.exe SHA256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// See docs/motion-queries.md.

namespace dfre {
// Original 0x47DF40 base height, which is TerrainGeometry::base_height: bilinear
// elevation samples with 16-bit world fractions, times 0x40.
std::int32_t ground_height_query(const TerrainGeometry& terrain,std::int32_t x,std::int32_t y) noexcept;

// Original 0x47E5D0 slope samples, which are TerrainGeometry::slope: elevation
// byte differences at +/-2 cells, each times 0x1000.
struct GroundSlope {
    std::int32_t x=0;
    std::int32_t y=0;
    bool operator==(const GroundSlope& other) const noexcept {return x==other.x&&y==other.y;}
};
GroundSlope ground_slope_query(const TerrainGeometry& terrain,std::int32_t x,std::int32_t y) noexcept;

// Original 0x48775F chooses the height source: actors without a ground entity
// (actor +0x108 == 0) use 0x47DF40, actors with one use 0x486FD0. The 0x486FD0
// result is not reconstructed here and stays an explicit input.
struct ActorGroundQuery {
    bool has_ground_entity=false; // actor +0x108 != 0
    std::int32_t entity_height=0; // 0x486FD0(actor,position,0x8000) result
    bool operator==(const ActorGroundQuery& other) const noexcept {
        return has_ground_entity==other.has_ground_entity&&entity_height==other.entity_height;
    }
};
std::int32_t ground_height_for_actor(const TerrainGeometry& terrain,const ActorGroundQuery& query,
    std::int32_t x,std::int32_t y) noexcept;

// Original 0x4876E0 minimum clearance scan over the definition lists at
// [def+0xC] and [def+0x8C]. The scan itself is not reconstructed; callers pass
// its minimum as GroundQuery::clearance (0xFFFFFFF when the list is empty).
inline constexpr std::int32_t empty_clearance=0xfffffff;
} // namespace dfre
