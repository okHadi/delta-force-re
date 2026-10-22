#include "dfre/scene_models.hpp"
#include "dfre/water.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

// Joined world model passes for the original 0x485610 frame. Original bodies
// live in the other model modules; this file wires them to loaded mission data.
// Numeric callbacks stay lookup data: 0x481060/0x480ff0 through
// model_instances, 0x481330/0x481130 through model_characters, the 14 special
// rows through model_specials. See include/dfre/scene_models.hpp.
namespace dfre {
namespace {

std::int32_t signed_bits(std::uint32_t bits) noexcept {
    std::int32_t value;std::memcpy(&value,&bits,sizeof(value));return value;
}

std::uint8_t read_u8(const std::uint8_t* data, std::size_t offset) {
    return data[offset];
}
std::uint16_t read_u16(const std::uint8_t* data, std::size_t offset) {
    return std::uint16_t(data[offset]) | std::uint16_t(data[offset + 1]) << 8;
}
std::uint32_t read_u32(const std::uint8_t* data, std::size_t offset) {
    return std::uint32_t(data[offset]) | std::uint32_t(data[offset + 1]) << 8 |
        std::uint32_t(data[offset + 2]) << 16 | std::uint32_t(data[offset + 3]) << 24;
}
std::int32_t read_i32(const std::uint8_t* data, std::size_t offset) {
    return static_cast<std::int32_t>(read_u32(data, offset));
}
std::int16_t read_i16(const std::uint8_t* data, std::size_t offset) {
    return static_cast<std::int16_t>(read_u16(data, offset));
}

// File LOD header words the draw callbacks read (+0x40 count, +0x58 duration,
// +0x5a guard). The decoder keeps the header bytes for every supported version.
std::int32_t lod_dword(const ModelLod& lod, std::size_t offset) {
    if (offset + 4 > lod.raw_header.size())
        throw std::invalid_argument("lod header is smaller than the draw callback reads");
    return read_i32(lod.raw_header.data(), offset);
}

ModelInstanceFields scene_instance_fields(const ActorImage& image) {
    ModelInstanceFields fields;
    fields.translation = {read_i32(image.data(), 0x24), read_i32(image.data(), 0x28),
        read_i32(image.data(), 0x2c)};
    fields.angles = {read_u32(image.data(), 0x30), read_u32(image.data(), 0x34),
        read_u32(image.data(), 0x38)};
    return fields;
}

// Special row object fields straight from the 0x114 runtime actor image.
ModelSpecialObject scene_special_object(const ActorImage& image, const SceneModelPassInputs& inputs) {
    ModelSpecialObject object;
    const auto* data = image.data();
    object.distance_fc = read_i32(data, 0xfc);
    object.value_98 = read_i32(data, 0x98);
    object.value_a0 = read_i32(data, 0xa0);
    object.value_e0 = read_i32(data, 0xe0);
    object.angle_9c = read_i32(data, 0x9c);
    object.angle_a4 = read_i32(data, 0xa4);
    object.angle_c4 = read_i32(data, 0xc4);
    object.angle_c8 = read_i32(data, 0xc8);
    object.value_94 = read_i32(data, 0x94);
    object.scale_7c = read_i32(data, 0x7c);
    object.z_30 = read_i32(data, 0x30);
    object.phase_ec = read_i32(data, 0xec);
    object.mode_f0 = read_i32(data, 0xf0);
    object.angle_e4 = read_i32(data, 0xe4);
    object.angle_e8 = read_i32(data, 0xe8);
    object.z_high = inputs.z_reference;       // 0x8cfa30
    object.z_low = inputs.direction_reference; // 0x8cfa34
    object.phase_33 = read_u8(data, 0x33);
    object.flag_3c = read_i32(data, 0x3c);
    return object;
}

std::uint32_t wrapped_sum(std::uint32_t left, std::uint32_t right) {
    return std::uint32_t(std::uint32_t(left) + std::uint32_t(right));
}

} // namespace

const char* scene_model_gap_name(SceneModelGapKind kind) {
    switch (kind) {
    case SceneModelGapKind::missing_model: return "missing_model";
    case SceneModelGapKind::missing_character: return "missing_character";
    case SceneModelGapKind::missing_pose: return "missing_pose";
    case SceneModelGapKind::runtime_pool_not_supplied: return "runtime_pool_not_supplied";
    case SceneModelGapKind::effect_table_not_supplied: return "effect_table_not_supplied";
    case SceneModelGapKind::row_table_not_supplied: return "row_table_not_supplied";
    case SceneModelGapKind::character_secondary_write: return "character_secondary_write";
    case SceneModelGapKind::unsupported_tag: return "unsupported_tag";
    case SceneModelGapKind::unsupported_lod: return "unsupported_lod";
    case SceneModelGapKind::effect_faces: return "effect_faces";
    case SceneModelGapKind::special_inputs: return "special_inputs";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Assets

SceneModelAssets scene_model_assets(const MissionRuntimeState& state, const MissionAssetCache& cache) {
    const auto resolve_model = [&cache](std::string_view name) -> const Model* {
        try {
            return &cache.model(name);
        } catch (const AssetError&) {
            return nullptr;
        }
    };
    const auto resolve_character = [&cache](std::string_view name) -> const CharacterModel* {
        try {
            return &cache.character(name);
        } catch (const AssetError&) {
            return nullptr;
        }
    };
    return scene_model_assets(state, resolve_model, resolve_character);
}

SceneModelAssets scene_model_assets(const MissionRuntimeState& state,
    const std::function<const Model*(std::string_view)>& resolve_model,
    const std::function<const CharacterModel*(std::string_view)>& resolve_character) {
    SceneModelAssets assets;
    if (!resolve_model || !resolve_character)
        throw std::invalid_argument("scene model assets need both resolvers");
    assets.models.reserve(state.models.size());
    for (const auto& binding : state.models) {
        const Model* model = resolve_model(binding.name);
        if (model == nullptr) {
            ++assets.unresolved_models;
            continue;
        }
        assets.models.push_back({binding.handle, binding.bank, model});
    }
    assets.characters.reserve(state.characters.size());
    for (const auto& entry : state.characters) {
        const CharacterModel* character = resolve_character(entry.first);
        if (character == nullptr) {
            ++assets.unresolved_characters;
            continue;
        }
        assets.characters.push_back({entry.second, character});
    }
    return assets;
}

// ---------------------------------------------------------------------------
// Tag dispatch

SceneModelTag scene_model_tag(std::uint32_t key) {
    SceneModelTag tag;
    if (key == 0x676e7263u) { // gnrc
        tag.kind = SceneModelTagKind::generic_crng;
        return tag;
    }
    if (key == 0x6f726730u) { // org0
        tag.kind = SceneModelTagKind::character_org0;
        return tag;
    }
    if (key == 0x6e756c6cu) { // llun
        tag.kind = SceneModelTagKind::null_row;
        return tag;
    }
    if (const auto* def = find_model_special_def(key)) {
        tag.kind = SceneModelTagKind::special;
        tag.special = def->tag;
        return tag;
    }
    // 0x4818f0 returns the first row when no key matches: row 0 is drib (bird).
    tag.kind = SceneModelTagKind::special;
    tag.special = ModelSpecialTag::bird;
    tag.default_row = true;
    return tag;
}

// ---------------------------------------------------------------------------
// Catalog

SceneModelCatalog::SceneModelCatalog(const SceneModelAssets& assets, const ModelPaletteBanks32& banks,
    const ModelGammaTable& gamma, unsigned texture_level) : banks_(&banks), texture_level_(texture_level) {
    if (texture_level > 3) throw std::invalid_argument("model texture level outside four levels");
    entries_.reserve(assets.models.size());
    std::uint32_t material_address=0x10000000u;
    for (const auto& asset : assets.models) {
        if (asset.handle == 0 || asset.model == nullptr)
            throw std::invalid_argument("scene model asset needs a nonzero handle and a decoded model");
        if (asset.bank >= banks.size())
            throw std::invalid_argument("scene model bank outside six banks");
        Entry entry;
        entry.handle = asset.handle;
        entry.bank = asset.bank;
        entry.source = asset.model;
        entry.prepared = prepare_model_bindings32(*asset.model, asset.bank, texture_level,
            banks[asset.bank], gamma);
        entry.applied.reserve(asset.model->lods.size());
        for (std::size_t lod = 0; lod < asset.model->lods.size(); ++lod)
            entry.applied.push_back(entry.prepared.apply(lod, asset.model->lods[lod]));
        for(const auto& lod:entry.prepared.lods) {
            std::vector<std::uint32_t> addresses;
            for(std::size_t i=0;i<lod.materials.size();++i) {
                if(material_address>0x1fffff80u)throw std::invalid_argument("scene material address map is full");
                addresses.push_back(material_address);
                material_address+=0x80u;
            }
            entry.material_addresses.push_back(std::move(addresses));
        }
        entries_.push_back(std::move(entry));
    }
    std::sort(entries_.begin(), entries_.end(),
        [](const Entry& left, const Entry& right) { return left.handle < right.handle; });
    for (std::size_t i = 1; i < entries_.size(); ++i)
        if (entries_[i].handle == entries_[i - 1].handle)
            throw std::invalid_argument("duplicate scene model handle");
    characters_ = assets.characters;
    std::sort(characters_.begin(), characters_.end(),
        [](const SceneCharacterAsset& left, const SceneCharacterAsset& right) {
            return left.handle < right.handle;
        });
    for (std::size_t i = 0; i < characters_.size(); ++i) {
        if (characters_[i].handle == 0 || characters_[i].character == nullptr)
            throw std::invalid_argument("scene character asset needs a nonzero handle and a decoded model");
        if (i > 0 && characters_[i].handle == characters_[i - 1].handle)
            throw std::invalid_argument("duplicate scene character handle");
    }
}

const SceneModelCatalog::Entry* SceneModelCatalog::find(std::uint32_t handle) const noexcept {
    const auto found = std::lower_bound(entries_.begin(), entries_.end(), handle,
        [](const Entry& entry, std::uint32_t value) { return entry.handle < value; });
    if (found == entries_.end() || found->handle != handle) return nullptr;
    return &*found;
}
SceneModelCatalog::Entry* SceneModelCatalog::find(std::uint32_t handle) noexcept {
    return const_cast<Entry*>(static_cast<const SceneModelCatalog*>(this)->find(handle));
}

const Model* SceneModelCatalog::model(std::uint32_t handle) const noexcept {
    const auto* entry = find(handle);
    return entry == nullptr ? nullptr : entry->source;
}
const ModelPreparedModel32* SceneModelCatalog::prepared(std::uint32_t handle) const noexcept {
    const auto* entry = find(handle);
    return entry == nullptr ? nullptr : &entry->prepared;
}
const ModelPaletteBank32* SceneModelCatalog::palette(std::uint32_t handle) const noexcept {
    const auto* entry = find(handle);
    return entry == nullptr ? nullptr : &(*banks_)[entry->bank];
}
std::size_t SceneModelCatalog::lod_count(std::uint32_t handle) const noexcept {
    const auto* entry = find(handle);
    return entry == nullptr ? 0 : entry->applied.size();
}
const ModelLod* SceneModelCatalog::applied_lod(std::uint32_t handle, std::size_t lod) const noexcept {
    const auto* entry = find(handle);
    if (entry == nullptr || lod >= entry->applied.size()) return nullptr;
    return &entry->applied[lod];
}
const CharacterModel* SceneModelCatalog::character(std::uint32_t handle) const noexcept {
    const auto found = std::lower_bound(characters_.begin(), characters_.end(), handle,
        [](const SceneCharacterAsset& entry, std::uint32_t value) { return entry.handle < value; });
    if (found == characters_.end() || found->handle != handle) return nullptr;
    return found->character;
}
std::vector<RasterMaterial32> SceneModelCatalog::raster_materials(std::uint32_t handle, std::size_t lod) const {
    const auto* entry = find(handle);
    if (entry == nullptr) throw std::invalid_argument("raster materials need a loaded model");
    if (lod >= entry->applied.size()) throw std::invalid_argument("raster materials lod outside table");
    std::vector<RasterMaterial32> materials;
    materials.reserve(entry->applied[lod].materials.size());
    for (std::size_t index = 0; index < entry->applied[lod].materials.size(); ++index)
        materials.push_back(entry->prepared.raster_material(lod, index));
    return materials;
}
std::vector<std::uint32_t> SceneModelCatalog::register_deferred_materials(ModelDeferredBridge& bridge,
    std::uint32_t handle,std::size_t lod) const {
    const auto* entry=find(handle);
    if(!entry || lod>=entry->prepared.lods.size())throw std::invalid_argument("deferred model or lod is missing");
    const auto& materials=entry->prepared.lods[lod].materials;
    const auto& addresses=entry->material_addresses[lod];
    for(std::size_t i=0;i<materials.size();++i) {
        const auto& material=materials[i];
        bridge.add_material(addresses[i],material.raw.data(),material.raw.size(),
            0x30000000u+std::uint32_t(entry->bank)*0x10000u,
            0x40000000u+std::uint32_t(entry->bank)*0x400u,entry->prepared.raster_material(lod,i));
    }
    return addresses;
}

// ---------------------------------------------------------------------------
// View and rows

SceneModelView make_scene_model_view(const std::array<std::int32_t, 3>& position,
    const std::array<std::uint32_t, 3>& angles, const RasterClip& clip, std::uint32_t horizontal_fov) {
    SceneModelView result;
    result.camera = make_model_camera(position, angles, clip, horizontal_fov);
    if (result.camera.planes.size() != 5)
        throw std::invalid_argument("scene model camera needs five clip planes");
    result.view.camera_position = position;
    result.view.world_to_camera = result.camera.matrix;
    result.view.projection.focal = result.camera.projection.focal_length;
    result.view.projection.centre_x = signed_bits(std::uint32_t(result.camera.projection.centre[0]) << 16);
    result.view.projection.centre_y = signed_bits(std::uint32_t(result.camera.projection.centre[1]) << 16);
    for (std::size_t i = 0; i < 5; ++i) result.view.projection.planes[i] = result.camera.planes[i];
    result.bounds = {clip.left, clip.top, clip.right, clip.bottom};
    return result;
}

ModelLighting make_scene_model_lighting(const SceneModelView& view) {
    // 0x485420 configures this light every frame before the large pass.
    return make_model_scene_lighting(view.camera.matrix, scene_model_light_direction,
        scene_model_light_ambient, scene_model_light_diffuse, scene_model_light_specular);
}

std::vector<const std::uint16_t*> scene_model_depth_rows(const std::vector<std::uint16_t>& depth,
    std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || std::uint64_t(width) * height != depth.size())
        throw std::invalid_argument("depth rows need a row-major image that matches the bounds");
    std::vector<const std::uint16_t*> rows(height);
    for (std::size_t row = 0; row < height; ++row)
        rows[row] = depth.data() + row * width;
    return rows;
}

ModelRowDepthTable scene_model_row_table(const std::vector<std::uint32_t>& row_keys,
    const std::vector<const std::uint16_t*>& row_words) {
    if (row_keys.size() != row_words.size())
        throw std::invalid_argument("row table keys and row words differ in size");
    ModelRowDepthTable table;
    // The original keys are dwords compared with the signed depth
    // (`0x484fb0`); TerrainFrame32::row_depths keeps the same 32-bit bits.
    table.row_keys = reinterpret_cast<const std::int32_t*>(row_keys.data());
    table.row_words = row_words.data();
    return table;
}

// ---------------------------------------------------------------------------
// Records

std::vector<SceneModelRecord> scene_model_records(const MissionRuntimeState& state) {
    std::vector<SceneModelRecord> records;
    const auto add = [&](std::size_t group, std::size_t slot) {
        SceneModelRecord record;
        record.identity = scene_model_identity(actor_handle(group, slot));
        record.image = mission_actor_image(state, group, slot);
        record.group = group;
        record.slot = slot;
        const auto index = read_u32(record.image.data(), 0x00);
        if (index < state.definitions.size()) record.character = state.definitions[index].type == 3;
        records.push_back(std::move(record));
    };
    // 0x485290 order: 0x7cca80 (group 3), 0x7dde80 (group 0), then 0x867e80
    // (group 1). The 0x822e80 pool is caller data and sits between groups 0 and 1.
    for (std::size_t slot = 0; slot < state.group3_count; ++slot) {
        if (mission_actor_created(state, slot) ||
            (slot < state.groups[3].size() && state.groups[3][slot].active)) {
            add(3, slot);
        }
    }
    for (std::size_t slot = 0; slot < state.groups[0].size(); ++slot)
        if (state.groups[0][slot].active) add(0, slot);
    for (std::size_t slot = 0; slot < state.groups[1].size(); ++slot)
        if (state.groups[1][slot].active) add(1, slot);
    return records;
}

std::vector<SceneModelRecord> scene_model_records(const MissionRuntimeState& state,
    const MissionActorImages& current) {
    std::vector<SceneModelRecord> records;
    for(const std::size_t group:{3u,0u,1u})for(std::size_t slot=0;slot<current[group].size();++slot){
        const auto& image=current[group][slot];
        const auto index=read_u32(image.data(),0);
        if(index==0)continue;
        if(index>=state.definitions.size())throw std::invalid_argument("Live actor definition exceeds the table");
        records.push_back({scene_model_identity(actor_handle(group,slot)),image,group,slot,state.definitions[index].type==3});
    }
    return records;
}

// ---------------------------------------------------------------------------
// SceneModels32

SceneModels32::SceneModels32(const SceneModelCatalog& catalog, const MissionRuntimeState& state)
    : catalog_(&catalog), state_(&state) {}

const CharacterModel* SceneModels32::character_model(std::uint32_t instance) const noexcept {
    if (instance < kCharacterInstanceBase) return nullptr;
    const auto index = instance - kCharacterInstanceBase;
    if (index >= state_->character_instances.size()) return nullptr;
    const auto item = state_->character_instances[index];
    if (item >= state_->definitions.size()) return nullptr;
    return catalog_->character(state_->definitions[item].character);
}

void SceneModels32::begin_frame() {
    material_frame_ = 0;
}

void SceneModels32::add_gap(std::vector<SceneModelGap>& gaps, SceneModelGapKind kind,
    std::uint32_t handle, std::string note) const {
    for (const auto& gap : gaps)
        if (gap.kind == kind && gap.handle == handle) return;
    gaps.push_back({kind, handle, std::move(note)});
}

const SceneModelRecord* SceneModels32::find_record(const void* identity) const noexcept {
    const auto found = record_index_.find(identity);
    return found == record_index_.end() ? nullptr : &frame_records_[found->second];
}

SceneModelCollectResult SceneModels32::collect(const SceneModelCollectInputs& inputs) {
    collected_ = SceneModelCollectResult{};
    frame_records_.clear();
    record_index_.clear();

    const auto handle_record = [&](const SceneModelRecord& record) {
        if (record_index_.count(record.identity)) throw std::invalid_argument("duplicate scene record identity");
        record_index_.emplace(record.identity, frame_records_.size());
        frame_records_.push_back(record);
        const auto handle = read_u32(record.image.data(), 0x10);
        const Model* model = nullptr;
        if (handle != 0) {
            model = catalog_->model(handle);
            if (model == nullptr) {
                add_gap(collected_.gaps, SceneModelGapKind::missing_model, handle,
                    "actor graphic handle has no decoded model");
                return;
            }
        }
        ModelCollectInput input;
        input.identity = record.identity;
        input.position = {read_i32(record.image.data(), 0x24), read_i32(record.image.data(), 0x28),
            read_i32(record.image.data(), 0x2c)};
        input.model = model;
        input.hidden = (read_u8(record.image.data(), 0x6c) & 2) != 0;
        if (record.character) {
            // 0x485040 person path: fixed 0x10000 radius into the character list.
            const auto entry = collect_character_model(input, inputs.view.view);
            if (!entry) return;
            collected_.character.push_back(*entry);
            ++collected_.character_records;
            const auto registry_slot = read_u32(record.image.data(), 0x94);
            SceneModelActorWrite write;
            write.identity = record.identity;
            write.group = record.group;
            write.slot = record.slot;
            write.offset = 0x8c;
            if (registry_slot == 0) {
                write.value = 0;
                collected_.writes.push_back(write);
            } else if (inputs.secondary_definition) {
                write.value = inputs.secondary_definition(registry_slot);
                collected_.writes.push_back(write);
            } else {
                add_gap(collected_.gaps, SceneModelGapKind::character_secondary_write, registry_slot,
                    "0x485040 needs the 0xAD3ED0 slot registry to write actor +0x8c");
            }
            return;
        }
        input.radius = model == nullptr ? 0 : model->bounding_radius;
        const auto entry = collect_scene_model(input, inputs.view.view);
        if (!entry) return;
        if (entry->queue == ModelQueue::large) collected_.large.push_back(entry->entry);
        else collected_.near.push_back(entry->entry);
    };

    // 0x485290 order: group 3, group 0, the 0x822e80 pool, group 1, effects.
    for (const auto& record : inputs.records) {
        if (record.group == 1) continue;
        ++collected_.records;
        handle_record(record);
    }
    if (inputs.runtime_records.empty()) {
        add_gap(collected_.gaps, SceneModelGapKind::runtime_pool_not_supplied, 0,
            "0x822e80 runtime actor pool is not modelled; pass its records explicitly");
    } else {
        for (const auto& record : inputs.runtime_records) {
            ++collected_.runtime_records;
            handle_record(record);
        }
    }
    for (const auto& record : inputs.records) {
        if (record.group != 1) continue;
        ++collected_.records;
        handle_record(record);
    }

    // 0x485290 effect loop over 0x9f3098: gate [+0xfc] > 0 and a nonzero record.
    if (inputs.effects.empty()) {
        add_gap(collected_.gaps, SceneModelGapKind::effect_table_not_supplied, 0,
            "0x9f3098 effect records are not supplied; pass them explicitly");
    } else {
        for (const auto& effect : inputs.effects) {
            ++collected_.effect_records;
            if (effect.counter_fc <= 0 || effect.identity == nullptr) continue;
            if (effect.model == 0) continue;
            const Model* model = catalog_->model(effect.model);
            if (model == nullptr) {
                add_gap(collected_.gaps, SceneModelGapKind::missing_model, effect.model,
                    "effect model handle has no decoded model");
                continue;
            }
            ModelCollectInput input;
            input.identity = effect.identity;
            input.position = effect.position;
            input.model = model;
            input.radius = model->bounding_radius;
            input.hidden = false;
            const auto entry = collect_effect_model(input, inputs.view.view);
            if (entry) collected_.near.push_back(*entry);
        }
    }
    return collected_;
}

// ---------------------------------------------------------------------------
// Draw pass planning.

std::optional<SceneModelDrawCall> SceneModels32::build_call(const ModelQueueEntry& entry,
    ModelQueue queue, const SceneModelPassInputs& inputs, std::vector<SceneModelGap>& gaps) {
    const auto* record = find_record(entry.object);
    if (record == nullptr)
        throw std::invalid_argument("queue object was not offered to collect");
    const auto& image = record->image;
    ModelObjectView object;
    object.identity = entry.object;
    object.primary = scene_model_identity(read_u32(image.data(), 0x10));
    object.secondary = scene_model_identity(read_u32(image.data(), 0x14));
    object.prefer_secondary = (read_u8(image.data(), 0x6c) & 8) != 0;
    const auto chosen = scene_model_handle(select_object_model(object));
    if (chosen == 0) return {}; // original returns when the model pointer is null
    const Model* model = catalog_->model(chosen);
    if (model == nullptr) {
        add_gap(gaps, SceneModelGapKind::missing_model, chosen, "draw handle has no decoded model");
        return {};
    }
    const auto level = select_model_level(model->lod_thresholds, entry.screen_radius);
    if (!level) return {}; // <= 0x8000 or below every threshold
    if (*level >= model->lods.size()) {
        add_gap(gaps, SceneModelGapKind::unsupported_lod, chosen,
            "selected level has no loaded runtime LOD record");
        return {};
    }
    const auto tag = scene_model_tag(model->render_types[*level]);
    if (tag.default_row)
        add_gap(gaps, SceneModelGapKind::unsupported_tag, chosen,
            "0x4818f0 found no row and used row 0 (drib)");
    SceneModelDrawCall call;
    call.kind = tag.kind;
    call.special_tag = tag.special;
    call.queue = queue;
    call.identity = entry.object;
    call.model_handle = chosen;
    call.lod_index = *level;
    call.clip_mask = entry.clip_mask;
    call.screen_radius = entry.screen_radius;
    call.camera = inputs.view.camera;
    std::array<ModelClipPlane, 5> stored_planes{};
    for (std::size_t i = 0; i < stored_planes.size(); ++i)
        stored_planes[i] = inputs.view.camera.planes[i];
    // 0x46a930 order: only the mask bits choose planes; a zero mask skips clipping.
    call.camera.planes = model_clip_planes_for_mask(entry.clip_mask, stored_planes);
    switch (call.kind) {
    case SceneModelTagKind::generic_crng:
        build_generic(call, image, inputs, gaps);
        break;
    case SceneModelTagKind::character_org0:
        build_character(call, image, inputs, gaps);
        if(call.items.empty())return {};
        break;
    case SceneModelTagKind::special:
        build_special(call, image, inputs, gaps);
        break;
    case SceneModelTagKind::null_row:
        break;
    }
    return call;
}

void SceneModels32::build_generic(SceneModelDrawCall& call, const ActorImage& image,
    const SceneModelPassInputs& inputs, std::vector<SceneModelGap>&) {
    // 0x481060: frame counter, object +0x24 matrix, camera product, mesh loop.
    const auto* lod = catalog_->applied_lod(call.model_handle, call.lod_index);
    if (lod == nullptr) throw std::invalid_argument("generic draw lost its applied lod");
    ModelInstanceLod info;
    info.mesh_count = lod->meshes.size();
    info.frame_count_raw = lod_dword(*lod, 0x40);
    info.frame_duration_raw = lod_dword(*lod, 0x58);
    const auto fields = scene_instance_fields(image);
    const auto draw = build_model_instance_draw(info, fields, inputs.view.view.world_to_camera,
        inputs.time);
    // A zero duration leaves 0xad3cf4 unchanged; every callback clears it after
    // its mesh loop.
    const auto frame = draw.frame ? *draw.frame : material_frame_;
    material_frame_ = 0;
    call.matrices = draw.matrices;
    call.items.reserve(lod->meshes.size());
    for (std::size_t mesh = 0; mesh < lod->meshes.size(); ++mesh) {
        SceneModelDrawItem item;
        item.lod = lod;
        item.model = call.model_handle;
        item.lod_index = call.lod_index;
        item.mesh_index = mesh;
        item.source = CharacterDrawSource::actor;
        item.table = 0;
        item.matrix = mesh;
        item.frame = frame;
        call.items.push_back(item);
    }
}

void SceneModels32::build_character(SceneModelDrawCall& call, const ActorImage& image,
    const SceneModelPassInputs& inputs, std::vector<SceneModelGap>& gaps) {
    // 0x481330 through the model_characters worker API. The actor transform,
    // flags, weapon slot, mesh count and material frame come from the image.
    const auto* lod = catalog_->applied_lod(call.model_handle, call.lod_index);
    if (lod == nullptr) throw std::invalid_argument("character draw lost its applied lod");
    const auto instance = read_u32(image.data(), 0x18);
    const CharacterModel* character = nullptr;
    const std::vector<CharacterBonePose>* pose = nullptr;
    SceneCharacterAttachments attachments;
    std::uint32_t counter = 0;
    if (instance != 0) {
        character = character_model(instance);
        if (character == nullptr) {
            add_gap(gaps, SceneModelGapKind::missing_character, instance,
                "org0 actor instance has no decoded CHR");
            return;
        }
        const auto found = characters_.find(instance);
        if (found == characters_.end() || found->second.pose == nullptr) {
            add_gap(gaps, SceneModelGapKind::missing_pose, instance,
                "org0 actor has no caller pose; set_character_pose first");
            return;
        }
        pose = found->second.pose;
        attachments = found->second.attachments;
        counter = found->second.counter;
    }
    const auto make_record = [&](const SceneCharacterAttachment& source)
        -> std::optional<CharacterAttachmentRecord> {
        if (source.model == 0) return std::nullopt; // original null record pointer
        const auto decoded = catalog_->lod_count(source.model);
        const auto count = source.lod_count == 0 ? decoded : std::min(source.lod_count, decoded);
        if (source.lod_count > decoded) {
            add_gap(gaps, SceneModelGapKind::unsupported_lod, source.model,
                "attachment lod count exceeds the loaded record");
            return std::nullopt;
        }
        CharacterAttachmentRecord record;
        record.lod_count = count;
        record.lods.resize(count, nullptr);
        for (std::size_t i = 0; i < count; ++i) record.lods[i] = catalog_->applied_lod(source.model, i);
        return record;
    };
    CharacterAttachments resolved;
    resolved.weapon_present = attachments.weapon_present;
    resolved.weapon_type = attachments.weapon_type;
    resolved.weapon = make_record(attachments.weapon);
    resolved.secondary = make_record(attachments.secondary);
    CharacterDrawFields fields;
    fields.transform = scene_instance_fields(image);
    fields.flags_6c = read_u8(image.data(), 0x6c);
    fields.flags_6d = read_u8(image.data(), 0x6d);
    fields.weapon_slot = read_u16(image.data(), 0x70);
    fields.mesh_count = lod->meshes.size();
    CharacterDrawPlan plan;
    try {
        plan = build_character_draw_plan(counter, fields, character, pose, lod, resolved,
            CharacterAttachmentMatrices{}, inputs.view.view.world_to_camera, call.lod_index,
            call.clip_mask);
    } catch (const std::invalid_argument& error) {
        add_gap(gaps, SceneModelGapKind::missing_pose, instance, error.what());
        return;
    }
    if (plan.alternate_counter) characters_[instance].counter = *plan.alternate_counter;
    call.tables = plan.matrix_tables;
    // 0x481330 writes actor +0x98 into 0xad3cf4 for every actor mesh draw; the
    // model_characters plan uses the LOD mesh count there, so actor draws take
    // the image field instead.
    const auto actor_frame = read_i32(image.data(), 0x98);
    call.items.reserve(plan.draws.size());
    for (const auto& draw : plan.draws) {
        SceneModelDrawItem item;
        item.source = draw.source;
        item.mesh_index = draw.mesh_index;
        item.table = draw.matrix_table;
        item.matrix = draw.matrix_index;
        if (draw.source == CharacterDrawSource::actor) {
            item.lod = lod;
            item.model = call.model_handle;
            item.lod_index = call.lod_index;
            item.frame = actor_frame;
        } else {
            const auto& source = draw.source == CharacterDrawSource::weapon
                ? attachments.weapon : attachments.secondary;
            const auto* attachment = catalog_->applied_lod(source.model, call.lod_index);
            if (attachment == nullptr) {
                add_gap(gaps, SceneModelGapKind::unsupported_lod, source.model,
                    "attachment lod is outside the loaded record");
                continue;
            }
            item.lod = attachment;
            item.model = source.model;
            item.lod_index = call.lod_index;
            item.frame = draw.material_frame;
        }
        if (item.table >= call.tables.size())
            throw std::invalid_argument("character matrix table outside the plan");
        call.items.push_back(item);
    }
}

void SceneModels32::build_special(SceneModelDrawCall& call, const ActorImage& image,
    const SceneModelPassInputs& inputs, std::vector<SceneModelGap>& gaps) {
    // Special rows: instance fields from the runtime LOD header and elements
    // from the decoded hardpoint array (same 0xc-byte records), object fields
    // from the actor image. model_specials owns the plan semantics.
    const auto* lod = catalog_->applied_lod(call.model_handle, call.lod_index);
    if (lod == nullptr) throw std::invalid_argument("special draw lost its applied lod");
    ModelSpecialInstance instance;
    instance.mesh_count = static_cast<std::int32_t>(lod->meshes.size());
    instance.mesh_base = 0;
    instance.element_base = 0;
    instance.divisor_58 = lod_dword(*lod, 0x58);
    if (lod->raw_header.size() < 0x5c)
        throw std::invalid_argument("lod header is smaller than the draw callback reads");
    instance.flag_5a = read_i16(lod->raw_header.data(), 0x5a);
    instance.threshold_40 = lod_dword(*lod, 0x40);
    const auto object = scene_special_object(image, inputs);
    auto plan = build_model_special_plan(call.special_tag, instance, object);
    call.special = plan;
    auto fields = scene_instance_fields(image);
    if (plan.placement_angle_offset != 0)
        fields.angles[0] = wrapped_sum(fields.angles[0], std::uint32_t(plan.placement_angle_offset));
    // tcfe/ecaf write the camera-facing yaw before the matrix call, then
    // write the saved yaw back after drawing. Use the first write here.
    for (const auto& write : plan.writes) {
        if (write.target == ModelSpecialWrite::Target::object_z) {
            fields.angles[0] = std::uint32_t(write.value);
            break;
        }
    }
    for (const auto& write : plan.writes)
        if(write.target==ModelSpecialWrite::Target::object_direction)
            fields.angles[1]=std::uint32_t(write.value);
    for (const auto& write : plan.writes) {
        if(write.target==ModelSpecialWrite::Target::object_phase && !write.restore) {
            fields.angles[0]=(fields.angles[0]&0x00ffffffu)|(std::uint32_t(std::uint8_t(write.value))<<24);
            break;
        }
    }
    const auto object_matrix = plan.scaled_base
        ? model_instance_local_matrix_scaled(fields, object.scale_7c)
        : model_instance_local_matrix(fields);
    call.matrices = model_special_step_matrices(plan, object_matrix,
        inputs.view.view.world_to_camera, lod->hardpoints);
    if (!plan.draws) return; // llun has no draw callback
    call.items.reserve(plan.steps.size());
    for (std::size_t step = 0; step < plan.steps.size(); ++step) {
        const auto mesh = std::size_t(plan.steps[step].mesh_offset / 0x70);
        if (mesh >= lod->meshes.size()) {
            add_gap(gaps, SceneModelGapKind::special_inputs, call.model_handle,
                "special step mesh is outside the decoded LOD");
            continue;
        }
        SceneModelDrawItem item;
        item.lod = lod;
        item.model = call.model_handle;
        item.lod_index = call.lod_index;
        item.mesh_index = mesh;
        item.source = CharacterDrawSource::actor;
        item.table = 0;
        item.matrix = step;
        item.frame = 0; // the verified callbacks clear 0xad3cf4 after the draws
        call.items.push_back(item);
    }
}

// The draw call stores the resolved special tag; the plan itself is built in
// build_special through model_specials.

SceneModelPassPlan SceneModels32::plan_large_pass(const SceneModelPassInputs& inputs) {
    SceneModelPassPlan plan;
    if ((inputs.frame_flags & 4) == 0) return plan;
    for (const auto& entry : collected_.large) {
        ++plan.entries;
        if (entry.object == nullptr) {
            ++plan.null_objects;
            continue;
        }
        if (auto call = build_call(entry, ModelQueue::large, inputs, plan.gaps))
            plan.calls.push_back(std::move(*call));
    }
    return plan;
}

SceneModelPassPlan SceneModels32::plan_after_terrain(const SceneModelPassInputs& inputs) {
    SceneModelPassPlan plan;
    if ((inputs.frame_flags & 4) == 0) return plan;
    if (inputs.rows.row_keys == nullptr || inputs.rows.row_words == nullptr) {
        add_gap(plan.gaps, SceneModelGapKind::row_table_not_supplied, 0,
            "0x4854d0 needs TerrainFrame32 row_depths and its depth rows");
    } else {
        const auto total = collected_.near.size() + collected_.character.size();
        const auto near_visible = cull_model_queue(collected_.near, inputs.view.bounds, inputs.rows);
        const auto character_visible = cull_model_queue(collected_.character, inputs.view.bounds,
            inputs.rows, inputs.suppressed_object, inputs.allow_suppressed);
        plan.culled = total - near_visible - character_visible;
    }
    const auto walk = [&](const ModelQueueList& list, ModelQueue queue) {
        for (const auto& entry : list) {
            ++plan.entries;
            if (entry.object == nullptr) {
                ++plan.null_objects;
                continue;
            }
            if (auto call = build_call(entry, queue, inputs, plan.gaps))
                plan.calls.push_back(std::move(*call));
        }
    };
    walk(collected_.near, ModelQueue::near);
    walk(collected_.character, ModelQueue::character);
    return plan;
}

SceneModelDrawStats SceneModels32::draw_plan(const SceneModelPassPlan& plan,
    const SceneModelDrawInputs& inputs) {
    if (inputs.colour == nullptr || inputs.depth == nullptr || inputs.width == 0 || inputs.height == 0)
        throw std::invalid_argument("scene model draw target is missing");
    if (std::uint64_t(inputs.width) * inputs.height != inputs.colour->size() ||
        inputs.colour->size() != inputs.depth->size())
        throw std::invalid_argument("scene model draw target size differs from the bounds");
    SceneModelDrawStats stats;
    for (const auto& call : plan.calls) {
        ++stats.calls;
        for (const auto& item : call.items) {
            ++stats.items;
            if (item.lod == nullptr) continue;
            std::vector<ModelMatrix> filled;
            const std::vector<ModelMatrix>* matrices = nullptr;
            switch (call.kind) {
            case SceneModelTagKind::generic_crng:
                matrices = &call.matrices;
                break;
            case SceneModelTagKind::character_org0:
                if (item.table >= call.tables.size())
                    throw std::invalid_argument("character matrix table outside the plan");
                if (item.source == CharacterDrawSource::actor) {
                    matrices = &call.tables[item.table];
                } else {
                    if (call.tables[item.table].empty())
                        throw std::invalid_argument("character attachment matrix table is empty");
                    filled.assign(item.lod->meshes.size(), call.tables[item.table][0]);
                    matrices = &filled;
                }
                break;
            case SceneModelTagKind::special:
                if (item.matrix >= call.matrices.size())
                    throw std::invalid_argument("special matrix slot outside the step list");
                filled.assign(item.lod->meshes.size(), call.matrices[item.matrix]);
                matrices = &filled;
                break;
            case SceneModelTagKind::null_row:
                continue;
            }
            const auto materials = catalog_->raster_materials(item.model, item.lod_index);
            std::array<std::uint32_t, 11> counters{};
            counters.fill(static_cast<std::uint32_t>(item.frame));
            if(inputs.deferred) {
                const auto addresses=catalog_->register_deferred_materials(*inputs.deferred,item.model,item.lod_index);
                stats.pixel_writes += draw_model_mesh32(*item.lod, item.mesh_index, *matrices,
                    call.camera, inputs.lighting, materials, inputs.width, inputs.height,
                    *inputs.colour, *inputs.depth, *inputs.deferred,addresses,counters, inputs.math);
            } else
                stats.pixel_writes += draw_model_mesh32(*item.lod,item.mesh_index,*matrices,
                    call.camera,inputs.lighting,materials,inputs.width,inputs.height,
                    *inputs.colour,*inputs.depth,counters,inputs.math);
        }
    }
    return stats;
}

void SceneModels32::set_character_pose(std::uint32_t instance,
    const std::vector<CharacterBonePose>* pose) {
    if (instance == 0) throw std::invalid_argument("character pose needs a nonzero instance");
    characters_[instance].pose = pose;
}

void SceneModels32::set_character_attachments(std::uint32_t instance,
    const SceneCharacterAttachments& attachments) {
    if (instance == 0) throw std::invalid_argument("character attachments need a nonzero instance");
    characters_[instance].attachments = attachments;
}

std::uint32_t SceneModels32::character_counter(std::uint32_t instance) const noexcept {
    const auto found = characters_.find(instance);
    return found == characters_.end() ? 0 : found->second.counter;
}

const std::vector<CharacterBonePose>* SceneModels32::character_pose(std::uint32_t instance) const noexcept {
    const auto found = characters_.find(instance);
    return found == characters_.end() ? nullptr : found->second.pose;
}


struct MissionScene32::Impl {
    const MissionRuntimeState* state{};
    SceneModelAssets resolved;
    ModelGammaTable gamma{};
    ModelPaletteBanks32 banks;
    std::unique_ptr<SceneModelCatalog> catalog;
    std::unique_ptr<SceneModels32> renderer;
    std::map<std::uint32_t,std::vector<CharacterBonePose>> poses;
    ModelDeferredBridge deferred;
    WaterScene32 water;
    Impl(const MissionRuntimeState& state,const MissionAssetCache& cache,
         const TerrainScene32& terrain,const AssetBytes& gamma_bytes):state(&state) {
        resolved=scene_model_assets(state,cache);
        if(gamma_bytes.size()!=gamma.size())throw std::runtime_error("Gamma table size differs");
        std::copy(gamma_bytes.begin(),gamma_bytes.end(),gamma.begin());
        std::vector<dfre::ModelPaletteInput> palette_inputs;
        for(const auto& entry:resolved.models)palette_inputs.push_back({entry.model,entry.bank});
        std::array<std::uint8_t,3> light{};
        for(std::size_t i=0;i<3;++i)light[i]=gamma.at(terrain.terrain.sky_palette.palette.at(48+i));
        banks=dfre::build_model_palette_banks32(palette_inputs,gamma,light);
        catalog=std::make_unique<SceneModelCatalog>(resolved,banks,gamma);
        renderer=std::make_unique<SceneModels32>(*catalog,state);
        for(std::size_t i=0;i<state.character_instances.size();++i) {
            const auto definition=state.character_instances[i];
            const auto* character=catalog->character(state.definitions.at(definition).character);
            if(!character || character->animations.empty() || character->animations[0].frames.empty())continue;
            const auto instance=dfre::kCharacterInstanceBase+std::uint32_t(i);
            poses[instance]=dfre::character_frame_pose(character->animations[0].frames[0]);
            renderer->set_character_pose(instance,&poses.at(instance));
        }

        water=prepare_water_scene32(terrain);
    }
};
MissionScene32::MissionScene32(const MissionRuntimeState& state,const MissionAssetCache& cache,
    const TerrainScene32& terrain,const AssetBytes& gamma)
    :impl_(std::make_unique<Impl>(state,cache,terrain,gamma)) {}
MissionScene32::~MissionScene32()=default;
MissionSceneFrame32 MissionScene32::render(const TerrainScene32& terrain,const VoxelCamera& camera,
    const TerrainFrameOptions32& options) {
    return render(terrain,camera,options,MissionSceneLiveInputs32{});
}
MissionSceneFrame32 MissionScene32::render(const TerrainScene32& terrain,const VoxelCamera& camera,
    const TerrainFrameOptions32& options,const MissionSceneLiveInputs32& live) {
    auto& renderer=*impl_->renderer;
    for(std::size_t slot=0;slot<impl_->state->character_instances.size();++slot){
        const auto instance=kCharacterInstanceBase+std::uint32_t(slot);
        const auto initial=impl_->poses.find(instance);
        const auto* pose=live.pose?live.pose(instance):(initial==impl_->poses.end()?nullptr:&initial->second);
        renderer.set_character_pose(instance,pose);
    }
    auto& deferred=impl_->deferred;
    const auto& state=*impl_->state;
    const auto& water=impl_->water;
    MissionSceneFrame32 result;
    result.models=impl_->resolved.models.size();result.characters=impl_->resolved.characters.size();
    const RasterClip clip{camera.left,camera.top,camera.right,camera.bottom};
    const auto view=make_scene_model_view(camera.position,{camera.yaw,camera.pitch,0},clip,std::uint32_t(camera.field_of_view));
    renderer.begin_frame();
    SceneModelCollectInputs collect;
    collect.view=view;collect.records=live.actors?scene_model_records(state,*live.actors):scene_model_records(state);
    const auto queues=renderer.collect(collect);
    SceneModelPassInputs pass;pass.view=view;pass.time=options.time;
    pass.z_reference=std::int32_t(camera.yaw);pass.direction_reference=std::int32_t(camera.pitch);
    if(state.local_player)pass.suppressed_object=scene_model_identity(actor_handle(3,state.local_player->actor));
    deferred.reset(clip,camera.width,camera.height);deferred.clear_queues();deferred.set_effect_mode(true);
    auto& frame=result.terrain;
    frame.pixels.resize(std::size_t(camera.width)*camera.height);frame.depths.resize(std::size_t(camera.width)*camera.height,0xffff);
    SceneModelDrawInputs draw;
    draw.width=camera.width;draw.height=camera.height;draw.colour=&frame.pixels;draw.depth=&frame.depths;
    draw.lighting=make_scene_model_lighting(view);draw.deferred=&deferred;
    const auto large=renderer.plan_large_pass(pass);
    const auto large_stats=renderer.draw_plan(large,draw);
    frame=render_terrain_frame32(terrain,camera,options);
    const auto depth_rows=scene_model_depth_rows(frame.depths,camera.width,camera.height);
    pass.rows=scene_model_row_table(frame.row_depths,depth_rows);
    const auto near=renderer.plan_after_terrain(pass);
    const auto mixed=deferred.draw_mixed(clip,camera.width,camera.height,frame.pixels,frame.depths);
    deferred.set_effect_mode(false);
    const auto near_stats=renderer.draw_plan(near,draw);
    WaterPass32 water_pass;water_pass.row_depths=frame.row_depths;water_pass.time=options.time;water_pass.timer=options.time;
    render_water_pass32(water,camera,water_pass,frame.pixels,frame.depths);
    const auto no_context=deferred.draw_no_context(clip,camera.width,camera.height,frame.pixels,frame.depths);
    const auto effects=deferred.draw_deferred_effect(clip,camera.width,camera.height,frame.pixels,frame.depths);
    render_water_companion32(water,camera,water_pass,frame.pixels,frame.depths);

    result.records=queues.records;result.large=queues.large.size();result.near=queues.near.size();
    result.people=queues.character.size();result.items=large_stats.items+near_stats.items;
    result.pixel_writes=large_stats.pixel_writes+near_stats.pixel_writes+mixed+no_context+effects;
    for(const auto* gaps:{&queues.gaps,&large.gaps,&near.gaps})
        result.gaps.insert(result.gaps.end(),gaps->begin(),gaps->end());
    return result;
}

} // namespace dfre
