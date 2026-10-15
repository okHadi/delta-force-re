#pragma once
#include "dfre/actor_runtime.hpp"
#include "dfre/model_bindings.hpp"
#include "dfre/model_characters.hpp"
#include "dfre/model_instances.hpp"
#include "dfre/model_palette.hpp"
#include "dfre/model_specials.hpp"
#include "dfre/model_visibility.hpp"
#include "dfre/models.hpp"
#include "dfre/renderer.hpp"
#include "dfre/resources.hpp"
#include "dfre/terrain.hpp"
#include "dfre/world_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// World model renderer for original missions. This module joins the loaded
// mission runtime (MissionRuntimeState, MissionAssetCache), the model palette
// banks and prepared bindings with the visibility/instances/specials/characters
// modules into the 0x485610 frame passes.
//
// Original frame order (Df.exe sha256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff):
//   0x485290 collect -> 0x485420 scene light + large list -> sky/ground ->
//   0x4854d0 cull -> 0x485580 near/character draw -> water.
// The three passes are gated by (0x485610 param_1 & 4). Numeric original
// callback addresses are lookup data only; drawing goes through typed paths:
//   crng 0x676e7263 -> build_model_instance_draw + draw_model_mesh32
//   org0 0x6f726730 -> build_character_draw_plan + draw_model_mesh32
//   14 special rows -> build_model_special_plan + model_special_step_matrices
//   llun 0x6e756c6c -> empty draw callback
// Unknown keys use the original default row (row 0, bird) and record a gap.

namespace dfre {

// Identities in the queue records are the deterministic actor handles from
// world_runtime.hpp, stored as opaque pointer tokens that are never dereferenced.
inline const void* scene_model_identity(std::uint32_t handle) noexcept {
    return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(handle));
}
inline std::uint32_t scene_model_handle(const void* identity) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(identity));
}

// ---------------------------------------------------------------------------
// Asset join: MissionRuntimeState handle maps -> decoded models and characters.

// One decoded runtime model bound to its deterministic handle and bank.
struct SceneModelAsset {
    std::uint32_t handle{};
    std::uint8_t bank{};
    const Model* model{};
};
struct SceneCharacterAsset {
    std::uint32_t handle{};
    const CharacterModel* character{};
};
struct SceneModelAssets {
    std::vector<SceneModelAsset> models;
    std::vector<SceneCharacterAsset> characters;
    std::size_t unresolved_models{};     // bindings with no decoded model
    std::size_t unresolved_characters{}; // character handles with no decoded file
};

// Resolves every MissionRuntimeState model binding and character handle through
// the mission cache. Model names keep the loader extension from resources.cpp.
SceneModelAssets scene_model_assets(const MissionRuntimeState& state, const MissionAssetCache& cache);
SceneModelAssets scene_model_assets(const MissionRuntimeState& state,
    const std::function<const Model*(std::string_view)>& resolve_model,
    const std::function<const CharacterModel*(std::string_view)>& resolve_character);

// ---------------------------------------------------------------------------
// Tag dispatch (0x500c1c rows through typed native paths).

enum class SceneModelTagKind : std::uint8_t {
    generic_crng,   // 0x676e7263, draw 0x481060 / matrix 0x480ff0
    character_org0, // 0x6f726730, draw 0x481330 / matrix 0x481130
    special,        // model_specials.hpp rows
    null_row,       // 0x6e756c6c, draw 0x481120 / matrix 0x481110
};
struct SceneModelTag {
    SceneModelTagKind kind{SceneModelTagKind::special};
    ModelSpecialTag special{ModelSpecialTag::bird};
    bool default_row{}; // true when 0x4818f0 fell back to row 0 (bird)
};
SceneModelTag scene_model_tag(std::uint32_t key);

// ---------------------------------------------------------------------------
// Catalog: prepared material bindings and geometry per handle.

class SceneModelCatalog {
public:
    // banks supplies the per-bank palette and shade rows; it must outlive this
    // catalog. texture_level is the original 0xC80D10 selector.
    SceneModelCatalog(const SceneModelAssets& assets, const ModelPaletteBanks32& banks,
        const ModelGammaTable& gamma, unsigned texture_level = 0);

    const Model* model(std::uint32_t handle) const noexcept;
    const CharacterModel* character(std::uint32_t handle) const noexcept;
    const ModelPreparedModel32* prepared(std::uint32_t handle) const noexcept;
    const ModelPaletteBank32* palette(std::uint32_t handle) const noexcept;
    std::size_t lod_count(std::uint32_t handle) const noexcept;
    // LOD copy with prepared 485E30/486880 material fields applied.
    const ModelLod* applied_lod(std::uint32_t handle, std::size_t lod) const noexcept;
    // One prepared raster material per LOD material, in material order.
    std::vector<RasterMaterial32> raster_materials(std::uint32_t handle, std::size_t lod) const;
    std::vector<std::uint32_t> register_deferred_materials(ModelDeferredBridge& bridge,
        std::uint32_t handle,std::size_t lod) const;
    std::size_t model_count() const noexcept { return entries_.size(); }
    std::size_t character_count() const noexcept { return characters_.size(); }
    unsigned texture_level() const noexcept { return texture_level_; }

private:
    struct Entry {
        std::uint32_t handle{};
        std::uint8_t bank{};
        const Model* source{};
        ModelPreparedModel32 prepared;
        std::vector<ModelLod> applied;
        std::vector<std::vector<std::uint32_t>> material_addresses;
    };
    const Entry* find(std::uint32_t handle) const noexcept;
    Entry* find(std::uint32_t handle) noexcept;
    const ModelPaletteBanks32* banks_{};
    unsigned texture_level_{};
    std::vector<Entry> entries_; // sorted by handle
    std::vector<SceneCharacterAsset> characters_;
};

// ---------------------------------------------------------------------------
// View state for collection and drawing (built from one player camera).

// camera is the renderer camera (0x8d1340 matrix, projection, clip, planes in
// stored order); view is the 0x8cfa20 collection state; bounds is 0x8cfa54..
struct SceneModelView {
    ModelCameraFrame camera;
    ModelViewState view;
    ModelScreenBounds bounds{};
};
SceneModelView make_scene_model_view(const std::array<std::int32_t, 3>& position,
    const std::array<std::uint32_t, 3>& angles, const RasterClip& clip, std::uint32_t horizontal_fov);

// 0x485420 scene light setup: direction {-46340, 0, 46340} and strengths
// 0x8ccc / 0x8000 / 0x10000. The light is configured every frame before the
// large pass, so both model passes share it.
inline constexpr std::array<std::int32_t, 3> scene_model_light_direction{-46340, 0, 46340};
inline constexpr std::int32_t scene_model_light_ambient = 0x8ccc;
inline constexpr std::int32_t scene_model_light_diffuse = 0x8000;
inline constexpr std::int32_t scene_model_light_specular = 0x10000;
ModelLighting make_scene_model_lighting(const SceneModelView& view);

// Row-table views for the 0x4854d0 cull. TerrainFrame32::row_depths supplies the
// keys and the uint16 depth buffer supplies one row pointer per screen row.
std::vector<const std::uint16_t*> scene_model_depth_rows(const std::vector<std::uint16_t>& depth,
    std::uint32_t width, std::uint32_t height);
ModelRowDepthTable scene_model_row_table(const std::vector<std::uint32_t>& row_keys,
    const std::vector<const std::uint16_t*>& row_words);

// ---------------------------------------------------------------------------
// Records offered to 0x485040 / 0x485290.

struct SceneModelRecord {
    const void* identity{};
    ActorImage image{};     // 0x114 runtime actor image
    std::size_t group{};    // original table: 3, 0, 1; 1 is placed after the pool
    std::size_t slot{};
    bool character{};       // definition +0x28 == 3 (person)
};
// 0x485290 collection order: group 3 (0x7cca80), group 0 (0x7dde80), the
// 0x822e80 runtime pool (caller), group 1 (0x867e80), then the 0x9f3098 effects.
// This helper returns the three BMS tables in that order; group 2 markers are
// not collected by the original.
std::vector<SceneModelRecord> scene_model_records(const MissionRuntimeState& state);

// One 0x9f3098 effect record (stride 0x114). The caller owns the effect table;
// the gate is counter_fc > 0.
struct SceneEffectRecord {
    const void* identity{};
    std::array<std::int32_t, 3> position{};
    std::uint32_t model{};   // deterministic model handle
    std::int32_t counter_fc{};
};

// ---------------------------------------------------------------------------
// Explicit unsupported inputs. Nothing is skipped silently.

enum class SceneModelGapKind : std::uint8_t {
    missing_model,             // nonzero handle with no decoded model
    missing_character,         // character instance with no decoded CHR
    missing_pose,              // org0 draw without caller pose
    runtime_pool_not_supplied, // 0x822e80 records missing
    effect_table_not_supplied, // 0x9f3098 records missing
    row_table_not_supplied,    // cull table missing
    character_secondary_write, // 0x485040 +0x8c write not resolved
    unsupported_tag,           // 0x4818f0 fell back to row 0
    unsupported_lod,           // selected level has no loaded record
    effect_faces,              // draw threw for effect face routing
    special_inputs,            // special step outside the decoded LOD
};
struct SceneModelGap {
    SceneModelGapKind kind{};
    std::uint32_t handle{};
    std::string note;
};
const char* scene_model_gap_name(SceneModelGapKind kind);

// 0x485040 character path actor +0x8c write. The caller applies these to its
// runtime actor records; value is the resolved definition index, 0 clears.
struct SceneModelActorWrite {
    const void* identity{};
    std::size_t group{};
    std::size_t slot{};
    std::uint32_t offset{}; // 0x8c
    std::uint32_t value{};
};

// ---------------------------------------------------------------------------
// Collection (0x485290).

struct SceneModelCollectInputs {
    SceneModelView view;
    std::vector<SceneModelRecord> records;         // scene_model_records order
    std::vector<SceneModelRecord> runtime_records; // 0x822e80 (position before group 1)
    std::vector<SceneEffectRecord> effects;        // 0x9f3098
    // 0x485040 +0x8c: registry slot (+0x94) -> runtime definition index
    // (original 0x4828C0 result). Null reports character_secondary_write.
    std::function<std::uint32_t(std::uint32_t)> secondary_definition;
};
struct SceneModelCollectResult {
    ModelQueueList large;     // 0xc23e70
    ModelQueueList near;      // 0xc3cd70
    ModelQueueList character; // 0xc3b570
    std::vector<SceneModelActorWrite> writes;
    std::vector<SceneModelGap> gaps;
    std::size_t records{};
    std::size_t runtime_records{};
    std::size_t effect_records{};
    std::size_t character_records{};
};

// ---------------------------------------------------------------------------
// Pass planning and drawing.

struct SceneModelPassInputs {
    SceneModelView view;
    std::uint32_t time{};          // 0x6e471c frame time for crng counters
    std::uint32_t frame_flags{4};  // 0x485610 param_1; bit 2 gates every pass
    ModelRowDepthTable rows{};     // 0x4854d0 cull tables (after terrain only)
    const void* suppressed_object{}; // 0x6ba3f0 local player
    bool allow_suppressed{};         // 0x6ba3ec
    std::int32_t z_reference{};      // 0x8cfa30 (special angle sources)
    std::int32_t direction_reference{}; // 0x8cfa34 (tcfe)
};

// One mesh draw. model/lod_index select the catalog entries for materials.
struct SceneModelDrawItem {
    const ModelLod* lod{};
    std::uint32_t model{};
    std::size_t lod_index{};
    std::size_t mesh_index{};
    CharacterDrawSource source{CharacterDrawSource::actor};
    std::size_t table{};   // index into SceneModelDrawCall::tables
    std::size_t matrix{};  // index into the table (or matrices when table is unused)
    std::int32_t frame{};  // 0xad3cf4 during this mesh draw
};

struct SceneModelDrawCall {
    SceneModelTagKind kind{};
    ModelSpecialTag special_tag{}; // valid for kind == special
    ModelQueue queue{};
    const void* identity{};
    std::uint32_t model_handle{};
    std::size_t lod_index{};
    std::uint32_t clip_mask{};
    std::int32_t screen_radius{};
    ModelCameraFrame camera{};   // mask-filtered planes, dispatch order
    std::vector<SceneModelDrawItem> items;
    std::vector<ModelMatrix> matrices;             // generic: one camera matrix per mesh
    std::vector<std::vector<ModelMatrix>> tables;  // character: table 0 actor, then attachments
    ModelSpecialPlan special{};                    // special rows: plan and its writes
};

struct SceneModelPassPlan {
    std::vector<SceneModelDrawCall> calls;
    std::vector<SceneModelGap> gaps;
    std::size_t entries{};
    std::size_t null_objects{};
    std::size_t culled{};
};

struct SceneModelDrawInputs {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint32_t>* colour{};
    std::vector<std::uint16_t>* depth{};
    ModelLighting lighting{};
    ModelVertexMath math{ModelVertexMath::scalar};
    ModelDeferredBridge* deferred{};
};
struct SceneModelDrawStats {
    std::size_t calls{};
    std::size_t items{};
    std::size_t pixel_writes{};
    std::size_t effect_gap_items{};
    std::vector<SceneModelGap> gaps;
};

// Caller-resolved character attachments. A null model is the original null
// record; the +0x90 weapon type only participates when it is 0xfc3 or 0xfc4.
struct SceneCharacterAttachment {
    std::uint32_t model{};
    std::size_t lod_count{};
};
struct SceneCharacterAttachments {
    bool weapon_present{};
    std::uint32_t weapon_type{};
    SceneCharacterAttachment weapon{};
    SceneCharacterAttachment secondary{};
};

class SceneModels32 {
public:
    SceneModels32(const SceneModelCatalog& catalog, const MissionRuntimeState& state);

    // Fixed per-frame state (call once before the passes).
    void begin_frame();

    // 0x485290. Clears and refills the three queue lists.
    SceneModelCollectResult collect(const SceneModelCollectInputs& inputs);
    const SceneModelCollectResult& queues() const noexcept { return collected_; }

    // 0x485420: scene light is set by the caller through make_scene_model_lighting;
    // the plan walks the large list in order.
    SceneModelPassPlan plan_large_pass(const SceneModelPassInputs& inputs);
    // 0x4854d0 cull then 0x485580 near and character walk.
    SceneModelPassPlan plan_after_terrain(const SceneModelPassInputs& inputs);

    SceneModelDrawStats draw_plan(const SceneModelPassPlan& plan, const SceneModelDrawInputs& inputs);

    // org0 character inputs. instance is actor image +0x18 (0x42CCE0 handle).
    void set_character_pose(std::uint32_t instance, const std::vector<CharacterBonePose>* pose);
    void set_character_attachments(std::uint32_t instance, const SceneCharacterAttachments& attachments);
    std::uint32_t character_counter(std::uint32_t instance) const noexcept;
    const std::vector<CharacterBonePose>* character_pose(std::uint32_t instance) const noexcept;

private:
    struct CharacterState {
        const std::vector<CharacterBonePose>* pose{};
        SceneCharacterAttachments attachments{};
        std::uint32_t counter{}; // 0xc23800
    };
    std::optional<SceneModelDrawCall> build_call(const ModelQueueEntry& entry, ModelQueue queue,
        const SceneModelPassInputs& inputs, std::vector<SceneModelGap>& gaps);
    void build_generic(SceneModelDrawCall& call, const ActorImage& image,
        const SceneModelPassInputs& inputs, std::vector<SceneModelGap>& gaps);
    void build_character(SceneModelDrawCall& call, const ActorImage& image,
        const SceneModelPassInputs& inputs, std::vector<SceneModelGap>& gaps);
    void build_special(SceneModelDrawCall& call, const ActorImage& image,
        const SceneModelPassInputs& inputs, std::vector<SceneModelGap>& gaps);
    void add_gap(std::vector<SceneModelGap>& gaps, SceneModelGapKind kind, std::uint32_t handle,
        std::string note) const;
    const SceneModelRecord* find_record(const void* identity) const noexcept;
    const CharacterModel* character_model(std::uint32_t instance) const noexcept;

    const SceneModelCatalog* catalog_{};
    const MissionRuntimeState* state_{};
    SceneModelCollectResult collected_;
    std::vector<SceneModelRecord> frame_records_; // offered records in collection order
    std::map<const void*, std::size_t> record_index_;
    std::map<std::uint32_t, CharacterState> characters_;
    std::int32_t material_frame_{};
};


// Loaded model palettes, initial CHR poses and the original frame pass order.
// The mission state, cache and terrain stay alive while this object is used.
struct MissionSceneFrame32 {
    TerrainFrame32 terrain;
    std::vector<SceneModelGap> gaps;
    std::size_t models{}, characters{}, records{}, large{}, near{}, people{}, items{}, pixel_writes{};
};
class MissionScene32 {
public:
    MissionScene32(const MissionRuntimeState& state,const MissionAssetCache& cache,
        const TerrainScene32& terrain,const AssetBytes& gamma);
    ~MissionScene32();
    MissionScene32(const MissionScene32&)=delete;
    MissionScene32& operator=(const MissionScene32&)=delete;
    MissionSceneFrame32 render(const TerrainScene32& terrain,const VoxelCamera& camera,
        const TerrainFrameOptions32& options={});
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dfre
