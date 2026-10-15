#pragma once
// DF1 effect faces and effect pixel passes.
// Original references use Df.exe SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dfre {

// Effect bits of the prepared face flags (original queue key 0x700).
// 0x100 comes from material +0x10 bit 3, 0x200 from +0x11 bit 7,
// 0x400 from +0x10 bit 7.
constexpr std::uint32_t effect_face_flag_mask = 0x700;
enum class EffectPass : std::uint32_t { none = 0, transparent = 0x100, smoke = 0x200, flame = 0x400 };
EffectPass effect_pass_of(std::uint32_t face_flags);
const char* effect_pass_name(EffectPass pass);

// Original mesh dispatch 0x46b1d0 queue choice for one prepared face.
// skip: draw disabled (0xad3d7c bit 0).
// draw: 0x462df0 immediately.
// deferred_no_context: queue 0xaa8d6c, drained by 0x46bf60.
// deferred: queue 0xa1a42c, drained by 0x46be50.
// deferred_effect: queue 0xa184ec, drained by 0x46bf10.
// effect_plain: queue 0xa165ac, drained by 0x46be50 first loop.
enum class EffectRoute {
    skip,
    draw,
    deferred_no_context,
    deferred,
    deferred_effect,
    effect_plain,
};
struct EffectRouteInput {
    std::uint32_t face_flags{};
    std::uint32_t material_flags{};
    bool effect_mode{};      // original 0xad3d28
    bool draw_enabled{true}; // original 0xad3d7c bit 0
};
EffectRoute route_effect_face(const EffectRouteInput& input);
const char* effect_route_name(EffectRoute route);

// Face flags from material flags for the effect bits only. The remaining
// bits (0x4/0x8/0x10/0x20/0x40) come from face type and shading.
std::uint32_t effect_face_flags(std::uint32_t material_flags);

struct EffectQueueFace {
    std::uint32_t flags{};   // prepared face flags
    std::size_t index{};     // caller index for the face
};
enum class EffectDrainOp { plain, deferred_span };
struct EffectDrainStep {
    EffectDrainOp op{};
    const EffectQueueFace* face{};
};
// One drain plan, in original call order.
struct EffectDrainPlan {
    std::vector<EffectDrainStep> steps;
    std::size_t flushes{}; // 0x46cc30 and 0x46d190 call count
};
// Original queues. Faces enter by route; draw and skip routes stay out.
class EffectQueues {
public:
    void set_effect_mode(bool on) { effect_mode_ = on; }
    bool effect_mode() const { return effect_mode_; }
    // Routes the face and keeps it in the matching queue. Returns the
    // original route so callers can draw or skip immediately.
    EffectRoute add(std::uint32_t face_flags, std::uint32_t material_flags, std::size_t index);
    // Queue A plan: non-0x20 faces use the span builder, then the two
    // flushes run, then 0x20 faces draw with the plain triangle path.
    EffectDrainPlan deferred_plan() const;
    // Queues drained with the plain triangle path: 0x46bf60 (no context)
    // then 0x46bf10 (effect). The 0x46be50 first loop is not included.
    std::vector<const EffectQueueFace*> plain_plan() const;
    const std::vector<EffectQueueFace>& deferred() const { return deferred_; }
    const std::vector<EffectQueueFace>& deferred_effect() const { return deferred_effect_; }
    const std::vector<EffectQueueFace>& effect_plain() const { return effect_plain_; }
    const std::vector<EffectQueueFace>& no_context() const { return no_context_; }
    void clear();
private:
    bool effect_mode_{};
    std::vector<EffectQueueFace> deferred_, deferred_effect_, effect_plain_, no_context_;
};

// One internal span record, 10 int32 fields, as produced by original
// 0x461e30. pixel is the linear target offset y*width+x.
struct EffectSpan {
    std::int32_t pixel{}, length{};
    std::int32_t u{}, v{}, depth{}, shade{};
    std::int32_t du{}, dv{}, ddepth{}, dshade{};
};
// 32-bit effect pixel modes from the original kernel table.
// no_write is original 0x4672b0 (a bare ret): the 0x100/0x200 flat
// passes write no pixels in the 32-bit path.
// alpha_blend is original 0x467ff0 (0x210/0x214/0x218).
// half_blend is original 0x4682a0 (0x410/0x414/0x418/0x430/0x434/0x438).
enum class EffectSpanMode {
    no_write = 0x4672b0,
    alpha_blend = 0x467ff0,
    half_blend = 0x4682a0,
};
// Original context record at 0xa16534, set by setup 0x4672a0. Offsets
// match the original layout; unused fields stay explicit.
struct EffectSpanContext {
    std::uint32_t colour{};        // +0x00 blend colour, four bytes
    std::uint32_t field_04{};      // +0x04 first span list field +0x14 copy
    std::uint32_t field_08{};      // +0x08 0x461690 output, role not proven
    std::uint32_t column_mask{};   // +0x0c texture width mask
    std::uint32_t row_mask{};      // +0x10 texture height mask
    std::uint32_t row_shift{};     // +0x14 texture width bits
    std::uint32_t field_18{};      // +0x18 material +0x20 copy
    std::uint16_t* depth_plane{};  // +0x1c target depth plane
    std::uint32_t* colour_plane{}; // +0x20 target colour plane
    const std::uint8_t* texture{}; // +0x24 texture bytes
    const std::uint32_t* blend_table{}; // +0x28 colour table, 256 entries
    std::uint32_t field_2c{};      // +0x2c target shade table
};
// Draw one span through the original 32-bit effect kernel. Bounds are
// checked. Returns the number of target pixels the kernel could blend.
std::size_t draw_effect_span32(const EffectSpan& span, EffectSpanMode mode,
    const EffectSpanContext& context, std::uint32_t width, std::uint32_t height);

// Original 32-bit textured pixel table (init 0x4686d0 with 32 bpp and
// textured kernels). stateful_previous marks mode keys the selector does
// not handle: it keeps the previous selection, so callers must be told.
enum class EffectSpanSelection { no_write, alpha_blend, half_blend, stateful_previous };
EffectSpanSelection effect_span_selection(std::uint32_t face_flags);
const char* effect_span_selection_name(EffectSpanSelection selection);

// --- Effect sprite files (KABOOM.3DI) and short model prefixes (MC.3DI) ---
struct EffectTextureRecord {
    std::string name;
    std::uint16_t sequence{}, flags{}, width{}, height{};
    std::vector<std::uint8_t> pixels;
    std::array<std::uint8_t, 1024> palette{};
    std::size_t offset{}; // record start
};
// Named 76-byte record at the end of KABOOM.3DI. Field layout matches a
// 3DI material record. Whether the original game reads it is unproven.
struct EffectSpriteMaterial {
    std::string name;
    std::uint32_t flags{};
    std::uint8_t timer{}, frame{};
    std::size_t offset{};
};
struct EffectSpriteFile {
    std::string magic;            // "kaboom" from the shipped file
    std::uint8_t marker{};        // byte 6, 0 in the shipped file
    std::vector<std::uint8_t> header; // bytes 7..count, raw and not decoded
    std::uint32_t texture_count{};
    std::vector<EffectTextureRecord> textures;
    std::vector<EffectSpriteMaterial> materials;
    std::size_t texture_end{};    // first byte after texture data
    std::size_t unknown_begin{}, unknown_end{}; // undecoded gap
    std::size_t file_size{};
};
// KABOOM.3DI: 6-byte "kaboom" magic, a raw header, a standard texture
// list, an undecoded gap, and named trailer records. No original reader
// exists (the only .3di parser 0x486370 needs the "3DI" magic), so the
// gap and header fields stay raw.
EffectSpriteFile decode_effect_sprite(const std::vector<std::uint8_t>& bytes);

struct ModelPrefixFile {
    std::string name;
    std::uint8_t version{};
    std::uint32_t lod_count{};
    std::vector<EffectTextureRecord> textures;
    std::vector<std::uint8_t> lod_header; // first LOD header bytes
    std::uint32_t declared_lod_size{};
    std::uint32_t vertex_count{}, normal_count{}, face_count{};
    std::uint32_t mesh_count{}, hardpoint_count{}, material_count{};
    std::size_t lod_data_offset{};
    std::size_t available_lod_bytes{}; // file bytes for the first LOD data
    bool truncated{};                  // declared size exceeds the file
    std::size_t file_size{};
};
// Standard 3DI header and texture list plus the first LOD header. Accepts
// v2-v5 and reports a declared LOD size that runs past the file instead
// of failing. MC.3DI declares 16,172 bytes at 0x5150 with 3,760 present.
ModelPrefixFile decode_model_prefix(const std::vector<std::uint8_t>& bytes);

} // namespace dfre
