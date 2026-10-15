#pragma once
#include "dfre/models.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dfre {
struct RasterVertex {
    std::int32_t x{},y{},depth{},u{},v{},shade{}; // Screen and texel UV use 16.16. Depth is camera depth << 6.
};
struct RasterClip {std::int32_t left{},top{},right{},bottom{};}; // Inclusive pixels.
struct RasterClassification {
    std::array<std::uint32_t,3> x_order{},y_order{}; // Minimum, middle, maximum.
    std::uint32_t clip_flags{}; // Left 1, right 2, top 4, bottom 8.
    std::int32_t extent{}; // Zero outside; original dispatch uses 1, 2, or >=3.
};
RasterClassification classify_raster_triangle(const std::array<RasterVertex,3>& triangle,const RasterClip& clip);
struct RasterGradients {
    std::array<std::int32_t,2> u{},v{},depth{},shade{}; // X and Y changes per pixel.
};
// Gradient flags: texture 1, depth 2, shade 4.
RasterGradients raster_triangle_gradients(const std::array<RasterVertex,3>& triangle,std::uint32_t flags);
struct RasterSpan {
    std::int32_t x{},y{},length{},u{},v{},depth{},shade{};
    std::int32_t du{},dv{},ddepth{},dshade{};
};
std::vector<RasterSpan> raster_triangle_spans(const std::array<RasterVertex,3>& triangle,
    const RasterClip& clip,std::uint32_t gradient_flags);
struct RasterTexture32 {
    std::uint8_t width_bits{},height_bits{};
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint32_t> shade_colours; // 64 shade rows of 256 colours.
    std::uint32_t uniform_shade{}; // Bits 16-21 select the row.
};
enum class RasterTextureMode { uniform=0x14, varying_shade=0x18, cutout=0x34 };
// Original 32-bit indexed-texture modes; shade and depth advance per pixel pair.
std::size_t draw_textured_span32(const RasterSpan& span,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth,const RasterTexture32& texture,
    RasterTextureMode mode=RasterTextureMode::uniform);
struct ModelLighting {
    std::array<std::int32_t,3> light_direction{},view_direction{}; // Mesh-local 22-bit fixed directions.
    std::int32_t ambient{},diffuse{},specular{}; // 16.16 strengths.
};
// Original 0x46b140 context conversion for one camera-space mesh matrix.
ModelLighting localize_model_lighting(const ModelMatrix& camera_matrix,const ModelLighting& camera_lighting,
    std::int32_t scale=65536);
// Original normal lighting 0x46aff0, including material constant and specular fields.
std::int32_t shade_model_normal(const ModelNormal& normal,const ModelMaterial& material,
    const ModelLighting& lighting,bool reverse=false);
// Camera planes use 22-bit normals. Face indices address the supplied positions.

struct ModelClipPlane {std::array<std::int32_t,3> normal{};std::int32_t offset{};};
struct ClippedModelFace {ModelFace face;bool rejected{};};
struct ModelClipGeometry {std::vector<std::array<std::int32_t,3>> positions;std::vector<ClippedModelFace> faces;};
ModelClipGeometry clip_model_geometry(const std::vector<std::array<std::int32_t,3>>& positions,
    const std::vector<ModelFace>& faces,const std::vector<ModelClipPlane>& planes);
// Original one- and two-pixel triangle paths use cached flat colour and per-vertex depth.
std::size_t draw_tiny_triangle32(const std::array<RasterVertex,3>& triangle,const RasterClip& clip,
    std::uint32_t flat_colour,std::uint32_t flags,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth);

// Original camera projection, ordered clip planes, and world-to-camera matrix.

struct ModelCameraFrame {ModelMatrix matrix{};ModelProjection projection;RasterClip clip;std::vector<ModelClipPlane> planes;};
ModelCameraFrame make_model_camera(const std::array<std::int32_t,3>& position,
    const std::array<std::uint32_t,3>& angles,const RasterClip& clip,std::uint32_t horizontal_fov);
}
