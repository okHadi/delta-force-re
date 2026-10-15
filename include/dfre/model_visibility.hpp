#pragma once
#include "dfre/models.hpp"
#include "dfre/renderer.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

// Original scene model visibility, frustum bounds and LOD selection.
//
// Frame order (0x485610):
//   0x485290 scene model collection (0x485040 helper) -> 0x485420 view setup and
//   large-radius model pass -> terrain -> 0x4854d0 cull pass -> 0x485580 draw pass.
// Addresses are in Df.exe sha256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
namespace dfre {

// 0x46b780 skips objects whose projected screen radius is at or below 0x8000 (0.5 px).
inline constexpr std::int32_t model_view_minimum_radius = 0x8000;
// 0x485040 sends objects above 0x640000 (100.0 px) to the large-radius list.
inline constexpr std::int32_t model_view_large_radius = 0x640000;
// 0x485040 and 0x485290 reject objects more than 0x3fc (1020) whole units from the camera axes.
inline constexpr std::int32_t model_view_world_limit = 0x3fc;
// 0x44acf0 far test after the near offset is subtracted: 0x3fc0000 (1020.0).
inline constexpr std::int32_t model_view_far_depth = 0x3fc0000;
// Characters use a fixed 0x10000 (1.0) sphere radius in 0x485040.
inline constexpr std::int32_t model_view_character_radius = 0x10000;

// The three 0x18-byte model lists. Large: base 0xc23e70, count 0xc54478, drawn in 0x485420.
// Near: base 0xc3cd70, count 0xc54474, drawn in 0x485580. Character: base 0xc3b570,
// count 0xc54470, drawn in 0x485580 and suppressed for the local player in 0x4854d0.
enum class ModelQueue { large, near, character };

// One 0x18-byte queue record. Original fields: +0x00 object, +0x08 screen radius,
// +0x0c clip mask, +0x10 int16 screen x, +0x12 int16 screen y, +0x14 camera depth.
// The large list fills only object, screen radius and clip mask.
struct ModelQueueEntry {
    const void* object{};
    std::int32_t screen_radius{}; // 0x8d1384 (camera+0x1964), 16.16 projected pixel radius.
    std::uint32_t clip_mask{}; // 0x44acf0 result. Bit 2 near, 4/8 side, 0x10/0x20 top or bottom.
    std::int32_t screen_x{}; // Signed 16-bit value of the 16.16 screen x.
    std::int32_t screen_y{}; // Signed 16-bit value of the 16.16 screen y.
    std::int32_t camera_depth{}; // 16.16 camera-space depth.
};
using ModelQueueList = std::vector<ModelQueueEntry>;

// View clip rectangle 0x8cfa54/0x8cfa58/0x8cfa5c/0x8cfa60, camera base 0x8cfa20 +0x34.
struct ModelScreenBounds {
    std::int32_t left{};
    std::int32_t top{};
    std::int32_t right{};
    std::int32_t bottom{};
};

// Occlusion tables read by the 0x4854d0 cull pass (0x484fb0).
// 0xc93ea0: one depth key per screen row. 0x8d0688: one word-row pointer per screen row.
// The terrain and water passes own the writers; this module treats them as inputs.
struct ModelRowDepthTable {
    const std::int32_t* row_keys{};
    const std::uint16_t* const* row_words{};
};

// Original 0x484fb0. True when the record is visible in the row tables.
bool model_row_visible(const ModelQueueEntry& entry, const ModelScreenBounds& bounds,
    const ModelRowDepthTable& table);

// Projection fields of the camera struct 0x8cfa20: focal +0x54 in whole pixels (the
// original shifts it left by 16), centres +0x60/+0x64 in 16.16, five 16-byte plane
// records at +0x1880 in stored order {nx,ny,nz,offset}. The offset is only subtracted
// for plane 0 (0x44acf0 tests planes 1-4 without their offset field).
struct ModelViewProjection {
    std::int32_t focal{};
    std::int32_t centre_x{};
    std::int32_t centre_y{};
    std::array<ModelClipPlane, 5> planes{};
};

// View state used while collecting models: camera position 0x8cfa24/0x8cfa28 (16.16),
// the world-to-camera matrix 0x8d1340, and the projection fields above.
struct ModelViewState {
    std::array<std::int32_t, 3> camera_position{};
    ModelMatrix world_to_camera{};
    ModelViewProjection projection{};
};

enum class ModelProjectionState {
    outside, // 0x44acf0 returned 1; the caller drops the object.
    projected, // Point rewritten with screen x/y (16.16) and camera depth.
    camera_inside_sphere // Depth below the sphere radius; mask 0x3e, radius 0x4000000, point unchanged.
};
struct ModelProjectionResult {
    ModelProjectionState state{ModelProjectionState::outside};
    std::uint32_t clip_mask{};
    std::array<std::int32_t, 3> point{}; // Screen x/y 16.16 and camera depth when projected.
    std::int32_t screen_radius{}; // Value of 0x8d1384 after the call.
};

// Original 0x44acf0 sphere projection and clip mask for one camera-space point.
ModelProjectionResult project_model_object(const ModelViewProjection& projection,
    const std::array<std::int32_t, 3>& camera_point, std::int32_t radius);

// One object offered to 0x485040. Identity is the actor record pointer used by the
// queue and the cull pass. Position is actor +0x24 (16.16). Radius is model +0x60
// (file header bounding radius, 16.16); characters pass 0x10000.
struct ModelCollectInput {
    const void* identity{};
    std::array<std::int32_t, 3> position{};
    const void* model{};
    std::int32_t radius{};
    bool hidden{}; // Actor flag byte +0x6c bit 2.
};

struct ModelCollectResult {
    ModelQueue queue{ModelQueue::near};
    ModelQueueEntry entry{};
};

// Original 0x485040 generic model path: hidden flag, camera-axis prefilter, transform,
// sphere projection, then the 100 px split between the large and near lists.
std::optional<ModelCollectResult> collect_scene_model(const ModelCollectInput& input,
    const ModelViewState& view);

// Original 0x485290 second loop over the 0x9f3098 records: no hidden flag, no split,
// always the near list. The caller checks [record+0xfc] > 0 before calling.
std::optional<ModelQueueEntry> collect_effect_model(const ModelCollectInput& input,
    const ModelViewState& view);

// Original 0x485290/0x485040 character path. The record writes its class pointer
// back to the caller; this function only builds the queue record.
std::optional<ModelQueueEntry> collect_character_model(const ModelCollectInput& input,
    const ModelViewState& view);

// Original 0x4854d0 cull pass over one list. Entries whose 0x484fb0 result is zero
// have their object field cleared. The character list additionally clears the object
// when it equals suppressed_object and allow_suppressed is false (local player body).
std::size_t cull_model_queue(ModelQueueList& entries, const ModelScreenBounds& bounds,
    const ModelRowDepthTable& table, const void* suppressed_object = nullptr,
    bool allow_suppressed = false);

// Original 0x46b780 head: object +0x6c bit 8 selects the secondary model at +0x14,
// otherwise the primary model at +0x10. Identity is the actor record pointer that the
// level callbacks receive. Returns nullptr when no model applies.
struct ModelObjectView {
    const void* identity{};
    const void* primary{};
    const void* secondary{};
    bool prefer_secondary{};
};
const void* select_object_model(const ModelObjectView& object);

// Runtime model slot built by 0x486370 and 0x45d700. Thresholds and data come from the
// loader; callbacks come from 0x4818f0 and 0x481a00 row lookups.
using ModelDrawCallback = void (*)(const void* level_data, std::uint32_t clip_mask,
    const void* object, std::uint32_t level);
using ModelMatrixCallback = void (*)(); // Character matrix entry (0x481130); resolved only.

// One row of the callback table at 0x500c1c (stride 0x14, count 0x500d54):
// key +0x00, draw pointer +0x04, matrix pointer +0x08.
struct ModelCallbackRow {
    std::uint32_t key{};
    ModelDrawCallback draw{};
    ModelMatrixCallback matrix{};
};

struct ModelRuntimeModel {
    std::array<std::int32_t, 4> lod_thresholds{}; // +0x18 file header, 16.16 screen radii.
    std::array<std::uint32_t, 4> lod_keys{}; // +0x28 callback tags such as 'crng' or 'org0'.
    std::array<ModelDrawCallback, 4> lod_draw{}; // +0x3c resolved by 0x4818f0.
    ModelMatrixCallback lod_matrix{}; // +0x4c resolved by 0x481a00.
    std::array<const void*, 4> lod_data{}; // +0x50 runtime LOD records from the loader.
};

// Original 0x4818f0 / 0x481a00: return the pointer of the first row whose key matches,
// otherwise the first row of the table.
ModelDrawCallback resolve_model_draw_callback(const ModelCallbackRow* rows, std::size_t count,
    std::uint32_t key);
ModelMatrixCallback resolve_model_matrix_callback(const ModelCallbackRow* rows, std::size_t count,
    std::uint32_t key);

// Proven rows of table 0x500c1c (REA evidence ev_b60ed265...7499e, PE bytes).
// 16 rows, stride 0x14, count at 0x500d54. Keys are the stored 4-byte tags; read the
// four bytes right to left for the semantic name (0x6f726730 reads "org0").
// Addresses are original x86 entries, for lookup and mapping only.
struct ModelCallbackTag {
    std::uint32_t key{};
    std::uint32_t draw_address{};
    std::uint32_t matrix_address{};
};
inline constexpr std::array<ModelCallbackTag, 16> model_callback_tags{{
    {0x62697264, 0x44cef0, 0x44ce80}, // drib -> bird
    {0x65666374, 0x42a610, 0x42a600}, // tcfe -> efct
    {0x65663364, 0x480e80, 0x480e70}, // d3fe -> ef3d
    {0x66616365, 0x480f80, 0x480f40}, // ecaf -> face
    {0x66697368, 0x44c8f0, 0x44c860}, // hsif -> fish
    {0x676e7263, 0x481060, 0x480ff0}, // crng -> gnrc (generic)
    {0x68656c6f, 0x44c570, 0x44c4c0}, // oleh -> helo
    {0x6c616d70, 0x44d150, 0x44d0d0}, // pmal -> lamp
    {0x6e756c6c, 0x481120, 0x481110}, // llun -> null
    {0x6f726730, 0x481330, 0x481130}, // 0gro -> org0
    {0x70616c6d, 0x44d5e0, 0x44d2a0}, // mlap -> palm
    {0x70656e67, 0x44da20, 0x44d990}, // gnep -> peng
    {0x70777030, 0x481660, 0x481650}, // 0pwp -> pwp0
    {0x7368726b, 0x44ce00, 0x44cdf0}, // krhs -> shrk
    {0x74697265, 0x42a290, 0x42a170}, // erit -> tire
    {0x74726565, 0x4817a0, 0x4816d0}, // eert -> tree
}};

// Proven row for a key, or nullptr when the key is not one of the 16 table entries.
const ModelCallbackTag* find_model_callback_tag(std::uint32_t key);

// Typed view of the proven rows for resolve_model_draw_callback / resolve_model_matrix_callback.
// The pointers carry original x86 addresses and must not be called by the port.
std::array<ModelCallbackRow, model_callback_tags.size()> model_callback_rows();

// Original 0x46b780 level choice. The largest radius picks slot 0; a radius at or below
// 0x8000 picks nothing. Slot 0 maps to file LOD 0 (the most detailed).
std::optional<std::uint32_t> select_model_level(const std::array<std::int32_t, 4>& lod_thresholds,
    std::int32_t screen_radius);

struct ModelDrawCall {
    const void* object{};
    const void* level_data{};
    ModelDrawCallback callback{};
    std::uint32_t clip_mask{};
    std::uint32_t level{};
};

// Original 0x46b780 dispatch for one queue record.
std::optional<ModelDrawCall> select_model_draw(const ModelRuntimeModel& model,
    const ModelObjectView& object, std::uint32_t clip_mask, std::int32_t screen_radius);

// Original 0x485580/0x485420 queue walk: skip records whose object is null, select the
// object model, then its level, then report the callback the original would call.
struct ModelPassAccessors {
    std::function<ModelObjectView(const void* object)> object;
    std::function<const ModelRuntimeModel*(const void* model)> model;
};
std::vector<ModelDrawCall> run_model_draw_pass(const ModelQueueList& entries,
    const ModelPassAccessors& accessors);

// Original 0x46a930 plane choice for one clipped mesh: only planes whose mask bit is set
// are applied, in dispatch order 0, 1, 2, 4, 3. A zero mask or bit 0x40 skips clipping.
bool model_needs_face_clipping(std::uint32_t clip_mask);
std::vector<ModelClipPlane> model_clip_planes_for_mask(std::uint32_t clip_mask,
    const std::array<ModelClipPlane, 5>& planes);

} // namespace dfre
