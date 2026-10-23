#pragma once
#include "dfre/mission_projectile_trails.hpp"
#include "dfre/renderer.hpp"
#include <functional>
#include <stdexcept>
namespace dfre {
class MissionTrailError:public std::runtime_error{public:using std::runtime_error::runtime_error;};
using MissionTrailPoint=std::array<std::int32_t,3>;
struct MissionTrailLine {
 std::array<MissionTrailPoint,2> vertices{}; // 16.16 screen XY; source packed depth.
 std::uint32_t color{};
 std::uint8_t alpha{};
};
struct MissionTrailFrameBindings {
 std::function<std::uint32_t(std::uint32_t)> read;
 std::function<void(std::uint32_t,std::uint32_t)> write;
 std::function<MissionTrailPoint(std::uint32_t matrix,std::uint32_t point)> transform;
 std::function<void(const MissionTrailLine&)> draw;
};
struct MissionTrailFrameClock {std::uint32_t previous_a158a0=0;};
// 4634D0. Updates shared lifetimes and submits each reached A00 line synchronously.
void mission_trail_frame(const MissionTrailFrameBindings&,std::uint32_t camera=0x8cfa20);
// 44AF80 / 44AEE0, using current camera plane words.
bool mission_trail_clip(const std::function<std::uint32_t(std::uint32_t)>&,
 std::uint32_t camera,MissionTrailPoint& from,MissionTrailPoint& to);
void mission_trail_clip_endpoint(MissionTrailPoint& outside,const MissionTrailPoint& inside,
 std::int32_t outside_distance,std::int32_t inside_distance);
// Keep all owners alive. Bind current camera and clock lvalues.
MissionTrailFrameBindings mission_trail_frame_bindings(MissionProjectileTrailCells&,
 MissionTrailFrameClock&,std::uint32_t& tick_6e4724,std::uint32_t& demo_6e4718,
 ModelCameraFrame&,std::function<void(const MissionTrailLine&)>);
// Reached 462520 32-bit branch. Reads depth and blends color without a depth store.
std::size_t mission_trail_draw32(const MissionTrailLine&,const RasterClip&,
 std::uint32_t width,std::uint32_t height,std::vector<std::uint32_t>& color,
 const std::vector<std::uint16_t>& depth);
}
