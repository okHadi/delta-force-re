#pragma once
#include "dfre/models.hpp"
#include "dfre/effect_deferred.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include <optional>

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
enum class RasterTextureMode { uniform=0x14, varying_shade=0x18, cutout=0x34, varying_cutout=0x38 };
// Original 32-bit indexed-texture modes; shade and depth advance per pixel pair.
std::size_t draw_textured_span32(const RasterSpan& span,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth,const RasterTexture32& texture,
    RasterTextureMode mode=RasterTextureMode::uniform,std::optional<std::uint32_t> uniform_shade={},
    bool width_shift_ready=true);
// Original 32-bit flat material span; shade and depth advance for each pixel.
std::size_t draw_flat_span32(const RasterSpan& span,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth,
    std::uint8_t palette_index,const std::vector<std::uint32_t>& shade_colours);
struct ModelRasterTriangle {
    std::array<RasterVertex,3> vertices;
    std::uint32_t flags{};
    std::size_t material{};
};
struct RasterMaterial32 {
    const RasterTexture32* texture=nullptr;
    const std::vector<std::uint32_t>* shade_colours=nullptr;
    std::uint8_t flat_index{};
    std::uint32_t tiny_colour{};
};
// Draw one assembled face through the original tiny/flat/textured paths.
std::size_t draw_model_triangle32(const ModelRasterTriangle& face,const RasterClip& clip,
    const RasterMaterial32& material,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth);
struct ModelLighting {
    std::array<std::int32_t,3> light_direction{},view_direction{}; // Mesh-local 22-bit fixed directions.
    std::int32_t ambient{},diffuse{},specular{}; // 16.16 strengths.
};
// Original 46BB20 / 46BA50; direction starts in world-space 16.16 units.
ModelLighting make_model_scene_lighting(const ModelMatrix& camera_matrix,
    const std::array<std::int32_t,3>& direction={-46340,0,46340},
    std::int32_t ambient=0x8ccc,std::int32_t diffuse=0x8000,std::int32_t specular=0x10000);
// Original 0x46b1d0 face assembly. UV coordinates remain normalized 16.16.
std::optional<ModelRasterTriangle> prepare_model_face(const ModelFace& face,
    const std::vector<ModelProjectedVertex>& positions,const std::vector<ModelNormal>& normals,
    std::size_t normal_start,const std::vector<ModelMaterial>& materials,const ModelLighting& lighting,
    const std::array<std::uint32_t,11>& animation_counters={});
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
// Camera matrices are one world-to-camera mesh matrix for each LOD mesh.
std::size_t draw_model_mesh32(const ModelLod& lod,std::size_t mesh_index,
    const std::vector<ModelMatrix>& camera_matrices,const ModelCameraFrame& camera,
    const ModelLighting& camera_lighting,const std::vector<RasterMaterial32>& materials,
    std::uint32_t width,std::uint32_t height,std::vector<std::uint32_t>& colour,
    std::vector<std::uint16_t>& depth,const std::array<std::uint32_t,11>& animation_counters={},
    ModelVertexMath math=ModelVertexMath::scalar);

// ----- Original effect/deferred model pass bridge (0x46b1d0/0x46be50) -----
// The join of the exact DeferredPass (0x46cac0/0x46cc30/0x46d190) to the
// model face and mesh draw path. Face flags with the effect bits 0x700 used
// to throw; through the bridge they follow original 0x46b1d0 routing into
// the four queues and the queued pixel kernels.

// Complete 32-bit span kernel set of the original pixel table 0x4686d0, as
// selected by 0x4683b0 over the record flags. The function addresses are the
// original entries the kernel port replaces.
enum class DeferredSpanKernel {
    no_write,               // 0x4672b0
    uniform_fill,           // 0x467320 context colour dword, per-pixel depth
    flat_shade,             // 0x467430 context colour index and shade table
    texture_uniform,        // 0x468cda
    texture_varying,        // 0x4694df
    texture_cutout,         // 0x4690cc
    texture_varying_cutout, // 0x469945
    effect_alpha_blend,     // 0x467ff0
    effect_half_blend,      // 0x4682a0
};
const char* deferred_span_kernel_name(DeferredSpanKernel kernel);
// Selector state of original 0x4683b0: the masked key and the current span
// kernel. The 32-bit table init 0x4686d0 leaves key zero and no_write. Keys
// without a case keep the kernel and only update the key (0xa16538), so the
// state must persist across records and frames.
struct DeferredSpanSelector {
    std::uint32_t key{};
    DeferredSpanKernel kernel{DeferredSpanKernel::no_write};
    DeferredSpanKernel select(std::uint32_t face_flags);
};

// One routed queue entry with the exact prepared face fields.
struct DeferredQueuedFace {
    std::uint32_t flags{};            // prepared face record flags
    std::uint32_t material_flags{};   // runtime material dword raw[16..19]
    std::uint32_t material_address{}; // registered runtime material
    std::array<RasterVertex,3> vertices{}; // prepared record fields, UV unscaled
};

// Original 32-bit uniform fill 0x467320: one context colour for every pixel
// with a strict per-pixel depth test and depth write.
std::size_t draw_uniform_span32(const RasterSpan& span,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth,std::uint32_t fill_colour);

// Bridge: frame reset 0x46c3e0, queue reset 0x46bb20, material registration,
// routing 0x46b1d0, record add 0x46cac0, flush 0x46cc30 and the draws of
// 0x46be50/0x46bf60/0x46bf10 in the 0x485610 frame order.
class ModelDeferredBridge {
public:
    ModelDeferredBridge();
    // Original 0x46c3e0 through a supplied frame (colour rows in pixels).
    void reset(const DeferredFrame& frame);
    // Row starts are row*width. depth_mode stays 0x20 for four-byte targets.
    void reset(const RasterClip& clip,std::uint32_t width,std::uint32_t height);
    // Original 0x46bb20 queue counters and the prepared face cursor.
    void clear_queues();
    // Original 0x46bfa0 and the 0xad3d7c bit 0 draw enable.
    void set_effect_mode(bool on);
    void set_draw_enabled(bool on);
    bool effect_mode() const { return effect_mode_; }
    bool draw_enabled() const { return draw_enabled_; }
    // Original 0x4686d0 selector state for the 32-bit table.
    void reset_selector();

    // Runtime material record bytes (raw[0..75]), the 0xbfc104/0xbfc100
    // per-LOD values the frame uses, and the native span bindings. The
    // binding must stay valid while the bridge holds queued faces.
    void add_material(std::uint32_t address,const std::uint8_t* data,std::size_t size,
        std::uint32_t table_a,std::uint32_t table_b,const RasterMaterial32& binding);

    // Original 0x46b1d0 queue stage for one prepared face. Returns the
    // original route; draw and skip faces stay out of every queue.
    EffectRoute queue_face(std::uint32_t flags,std::uint32_t material_flags,
        std::uint32_t material_address,const std::array<RasterVertex,3>& vertices);
    // Original 0x46cac0. Returns the number of kept edges.
    std::size_t add_face(std::uint32_t flags,const std::array<DeferredVertex,3>& vertices,
        std::uint32_t material_address);

    // Original 0x46cc30.
    void flush();
    // Original 0x46d190 span build plus the complete selector pixel kernels.
    std::size_t draw_spans(std::uint32_t width,std::uint32_t height,
        std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth);
    // Original 0x46be50: queue C plain, queue A spans, then queue A 0x20 plain.
    std::size_t draw_mixed(const RasterClip& clip,std::uint32_t width,std::uint32_t height,
        std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth);
    // Original 0x46bf60 and 0x46bf10 plain drains.
    std::size_t draw_no_context(const RasterClip& clip,std::uint32_t width,std::uint32_t height,
        std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth);
    std::size_t draw_deferred_effect(const RasterClip& clip,std::uint32_t width,std::uint32_t height,
        std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth);
    // Drain mixed, no-context and effect queues. The frame caller sets pass order.
    std::size_t draw_all(const RasterClip& clip,std::uint32_t width,std::uint32_t height,
        std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth);
    std::size_t draw_direct(std::uint32_t flags,std::uint32_t material_address,
        const std::array<RasterVertex,3>& vertices,const RasterClip& clip,
        std::uint32_t width,std::uint32_t height,std::vector<std::uint32_t>& colour,
        std::vector<std::uint16_t>& depth);

    // Frame state for tests and the replay check.
    const DeferredPass& pass() const { return pass_; }
    const DeferredSpanSelector& selector() const { return selector_; }
    // Last computed steps for replay output. One-pixel spans use no steps.
    const std::array<std::int32_t,4>& chunk_steps() const { return chunk_steps_; }
    void set_chunk_steps(const std::array<std::int32_t,4>& steps) { chunk_steps_=steps; }
    // Span plan of the last draw_spans(). Unused one-pixel steps are zero.
    const std::vector<DeferredSpan>& plan() const { return plan_; }
    const std::vector<DeferredQueuedFace>& queue(EffectRoute route) const;

private:
    void prepare_span_kernel(DeferredSpanKernel kernel,const RasterMaterial32& material);
    std::size_t draw_plain(const DeferredQueuedFace& face,const RasterClip& clip,
        std::uint32_t width,std::uint32_t height,std::vector<std::uint32_t>& colour,
        std::vector<std::uint16_t>& depth);
    std::size_t draw_span_kernel(DeferredSpanKernel kernel,const DeferredSpan& plan_span,
        const RasterMaterial32& material,
        std::uint32_t width,std::uint32_t height,std::vector<std::uint32_t>& colour,
        std::vector<std::uint16_t>& depth);
    const RasterMaterial32& registered_binding(std::uint32_t address) const;

    DeferredPass pass_;
    DeferredSpanSelector selector_;
    // Each original textured setup caches its width at zero. The row shift
    // becomes valid on its first nonzero width (e.g. 0x469747..0x469776).
    std::array<bool,4> texture_width_ready_{};
    std::array<std::int32_t,4> chunk_steps_{};
    std::map<std::uint32_t,RasterMaterial32> bindings_;
    std::map<std::uint32_t,std::uint32_t> bound_colours_;
    std::vector<DeferredQueuedFace> deferred_queue_,deferred_effect_queue_,
        effect_plain_queue_,no_context_queue_;
    std::vector<DeferredSpan> plan_;
    bool effect_mode_{};
    bool draw_enabled_{true};
};

// Face-level join: routes through the bridge instead of throwing for the
// effect bits. Returns the pixels drawn now; queued faces draw at the drain.
std::size_t draw_model_triangle32(const ModelRasterTriangle& face,const RasterClip& clip,
    const RasterMaterial32& material,std::uint32_t width,std::uint32_t height,
    std::vector<std::uint32_t>& colour,std::vector<std::uint16_t>& depth,
    ModelDeferredBridge& deferred,std::uint32_t material_flags,std::uint32_t material_address);
// Mesh-level join: the pose material index of every face resolves through
// material_addresses before routing. Every prepared face is either drawn now
// or queued, never dropped. The address table must match the LOD materials.
std::size_t draw_model_mesh32(const ModelLod& lod,std::size_t mesh_index,
    const std::vector<ModelMatrix>& camera_matrices,const ModelCameraFrame& camera,
    const ModelLighting& camera_lighting,const std::vector<RasterMaterial32>& materials,
    std::uint32_t width,std::uint32_t height,std::vector<std::uint32_t>& colour,
    std::vector<std::uint16_t>& depth,ModelDeferredBridge& deferred,
    const std::vector<std::uint32_t>& material_addresses,
    const std::array<std::uint32_t,11>& animation_counters={},
    ModelVertexMath math=ModelVertexMath::scalar);

}
