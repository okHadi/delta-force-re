#include "dfre/mission_geometry.hpp"
#include "dfre/model_instances.hpp"
#include "dfre/model_characters.hpp"
#include "dfre/model_specials.hpp"
#include "dfre/world_hits.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dfre {
namespace {
std::int32_t signed_word(std::uint32_t value) {
    return value <= 0x7fffffffu ? std::int32_t(value) : std::int32_t(std::int64_t(value) - 0x100000000LL);
}
template<class Bytes> std::uint32_t word(const Bytes& bytes, std::size_t at) {
    if (at > bytes.size() || bytes.size() - at < 4)
        throw std::invalid_argument("mission geometry field outside stored record");
    return std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at+1]) << 8) |
        (std::uint32_t(bytes[at+2]) << 16) | (std::uint32_t(bytes[at+3]) << 24);
}
std::int32_t add(std::int32_t left, std::int32_t right) {
    return signed_word(std::uint32_t(left) + std::uint32_t(right));
}
std::int32_t negate(std::int32_t value) { return signed_word(0u - std::uint32_t(value)); }
bool in_bounds(const std::array<std::int32_t,2>& point, const std::array<std::int32_t,4>& bounds) {
    return bounds[0] <= point[0] && point[0] <= bounds[1] &&
        bounds[2] <= point[1] && point[1] <= bounds[3];
}
ModelMatrix inverse(const ModelMatrix& source) {
    auto result = source;
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column)
            result[row*4+column] = source[column*4+row];
        result[row*4+3] = negate(source[row*4+3]);
    }
    return result;
}
std::int32_t plane_height(const ModelNormal& normal, std::int32_t plane,
    const std::array<std::int32_t,3>& point) {
    auto total = plane;
    for (std::size_t axis = 0; axis < 2; ++axis) {
        const auto product = std::int64_t(normal.direction[axis]) * 256 * point[axis];
        total = add(total, signed_word(std::uint32_t(product >> 22)));
    }
    const auto numerator = std::int64_t(negate(total)) * 0x400000;
    const auto denominator = std::int32_t(normal.direction[2]) * 256;
    if (denominator == 0) throw std::invalid_argument("mission geometry plane divides by zero");
    const auto result = numerator / denominator;
    if (result < std::numeric_limits<std::int32_t>::min() ||
        result > std::numeric_limits<std::int32_t>::max())
        throw std::invalid_argument("mission geometry plane divide outside original range");
    return std::int32_t(result);
}
MissionGeometryMatrices generic_matrices(const ModelLod& lod, const AiActorImage& actor) {
    ModelInstanceFields fields;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        fields.translation[axis] = signed_word(word(actor, 0x24 + axis*4));
        fields.angles[axis] = word(actor, 0x30 + axis*4);
    }
    MissionGeometryMatrices result;
    result.matrices = model_instance_local_matrices(lod.meshes.size(), fields);
    result.flags.resize(result.matrices.size()); // 480FF0 clears the +3C word in each slot.
    return result;
}
} // namespace

MissionGeometryMatrices mission_geometry_callback_matrices(const MissionGeometryModel& binding,
    const AiActorImage& actor, const CharacterModel* character, const std::vector<CharacterBonePose>* pose,
    const std::vector<std::uint8_t>* prior_flags, std::optional<std::uint8_t> palm_stack_flag) {
    if (!binding.model || binding.model->lods.empty())
        throw std::invalid_argument("mission geometry matrix callback has no first LOD");
    const auto& model = *binding.model;
    const auto& lod = model.lods.front();
    if (model.render_types[0] == 0x676e7263u) return generic_matrices(lod, actor);
    MissionGeometryMatrices result;
    if (model.render_types[0] == 0x6f726730u) {
        if (!prior_flags || prior_flags->size() < lod.meshes.size())
            throw std::invalid_argument("mission geometry character callback requires prior scratch flags");
        if (word(actor, 0x18) && (!character || !pose))
            throw std::invalid_argument("mission geometry character callback requires the current pose");
        result.matrices = character_draw_matrices(word(actor, 0x18) ? character : nullptr,
            pose, &lod, word(actor, 0x34), lod.meshes.size());
        result.flags.assign(prior_flags->begin(), prior_flags->begin()+result.matrices.size());
        return result;
    }
    const auto* special = find_model_special_def(model.render_types[0]);
    if (!special) throw std::invalid_argument("mission geometry matrix callback tag is unknown");
    ModelSpecialInstance instance;
    instance.mesh_count = std::int32_t(lod.meshes.size());
    instance.divisor_58 = signed_word(word(lod.raw_header, 0x58));
    instance.flag_5a = instance.divisor_58 >> 16;
    instance.threshold_40 = signed_word(word(lod.raw_header, 0x40));
    ModelSpecialObject object;
    object.distance_fc = signed_word(word(actor, 0xfc));
    object.value_98 = signed_word(word(actor, 0x98));
    object.value_a0 = signed_word(word(actor, 0xa0));
    object.value_e0 = signed_word(word(actor, 0xe0));
    object.angle_9c = signed_word(word(actor, 0x9c));
    object.angle_a4 = signed_word(word(actor, 0xa4));
    object.angle_c4 = signed_word(word(actor, 0xc4));
    object.angle_c8 = signed_word(word(actor, 0xc8));
    object.value_94 = signed_word(word(actor, 0x94));
    object.scale_7c = signed_word(word(actor, 0x7c));
    object.z_30 = signed_word(word(actor, 0x30));
    object.phase_ec = signed_word(word(actor, 0xec));
    object.mode_f0 = signed_word(word(actor, 0xf0));
    object.angle_e4 = signed_word(word(actor, 0xe4));
    object.angle_e8 = signed_word(word(actor, 0xe8));
    object.phase_33 = actor[0x33];
    object.flag_3c = signed_word(word(actor, 0x3c));
    object.callback_arg3 = 0;
    const auto plan = build_model_special_plan(special->tag, instance, object);
    if (!plan.has_base_matrix) { result.present = false; return result; }
    if (special->tag == ModelSpecialTag::palm &&
        (object.value_e0 == 0x10 || object.value_e0 == 0x11 || object.value_e0 == 0x20) && !palm_stack_flag)
        throw std::invalid_argument("mission geometry palm callback requires its copied stack flag");
    ModelInstanceFields fields;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        fields.translation[axis] = signed_word(word(actor, 0x24 + axis*4));
        fields.angles[axis] = word(actor, 0x30 + axis*4);
    }
    // 480F40 uses callback argument three +7FFFFFFF for slot-base yaw.
    if (special->tag == ModelSpecialTag::face) fields.angles[0] = 0x7fffffffu;
    const auto slot_matrices = model_special_slot_matrices(plan,
        model_instance_local_matrix(fields), lod.hardpoints);
    std::size_t slots = lod.meshes.size();
    for (const auto& slot : plan.slots) slots = std::max(slots, std::size_t(slot.index) + 1);
    for (const auto index : plan.localized_slots) slots = std::max(slots, std::size_t(index) + 1);
    result.matrices.resize(slots);
    result.flags.resize(slots);
    if (special->tag == ModelSpecialTag::tire) {
        if (!prior_flags || prior_flags->size() < slots)
            throw std::invalid_argument("mission geometry tire callback requires prior scratch flags");
        result.flags.assign(prior_flags->begin(), prior_flags->begin()+slots);
    }
    if (special->tag == ModelSpecialTag::palm && palm_stack_flag)
        for (const auto& slot : plan.slots)
            if (slot.kind == ModelSpecialMatrixKind::translate) result.flags[slot.index] = *palm_stack_flag;
    for (const auto index : plan.localized_slots) {
        if (index >= slots) throw std::invalid_argument("mission geometry localized slot is outside callback state");
        result.flags[index] = 1;
    }
    std::vector<bool> written(slots);
    for (std::size_t i = 0; i < plan.slots.size(); ++i) {
        const auto index = plan.slots[i].index;
        result.matrices[index] = slot_matrices[i];
        written[index] = true;
    }
    for (std::size_t index = 0; index < lod.meshes.size(); ++index)
        if (!written[index] && !(result.flags[index] & 1))
            throw std::invalid_argument("mission geometry callback leaves an active matrix slot unwritten");
    return result;
}

MissionGeometryModel mission_geometry_model(std::uint32_t handle,
    const Model& localized, const AssetBytes& source) {
    if (source.size() < 128 || source[0] != '3' || source[1] != 'D' || source[2] != 'I' || source[3] != 5)
        throw std::invalid_argument("mission geometry requires a stored v5 model");
    if (localized.version != 5 || localized.lods.empty())
        throw std::invalid_argument("mission geometry requires a loaded first LOD");
    const auto& lod = localized.lods.front();
    if ((lod.flags & 1) && !lod.origins_localized)
        throw std::invalid_argument("mission geometry requires localized mesh origins");
    MissionGeometryModel result;
    result.handle = handle;
    result.model = &localized;
    result.bounds = {signed_word(word(source, 0x64)), signed_word(word(source, 0x60))};
    result.height_68 = signed_word(word(source,0x68));
    result.faces = collision_face_boxes(source, 0);
    if (result.faces.size() != lod.faces.size())
        throw std::invalid_argument("mission geometry source face count differs from loaded model");
    return result;
}

MissionGeometry::MissionGeometry(std::vector<MissionGeometryModel> models) : models_(std::move(models)) {
    for (std::size_t i = 0; i < models_.size(); ++i) {
        auto& value = models_[i];
        if (!value.handle || !value.model || value.model->lods.empty() ||
            value.faces.size() != value.model->lods.front().faces.size())
            throw std::invalid_argument("mission geometry model binding is incomplete");
        for (std::size_t j = 0; j < i; ++j)
            if (models_[j].handle == value.handle)
                throw std::invalid_argument("mission geometry model handle is repeated");
        const auto& lod = value.model->lods.front();
        const auto volumes = decode_collision_volumes(lod);
        std::size_t volume_start = 0;
        value.meshes.clear();
        value.meshes.reserve(lod.meshes.size());
        for (const auto& mesh : lod.meshes) {
            if (mesh.face_start > lod.faces.size() || mesh.face_count > lod.faces.size() - mesh.face_start)
                throw std::invalid_argument("mission geometry mesh face range is invalid");
            const auto start = mesh.face_start;
            const auto end = std::size_t(start) + mesh.face_count;
            const auto volume_count = word(mesh.raw,0x1c);
            if (volume_count > volumes.size() - volume_start)
                throw std::invalid_argument("mission geometry mesh collision range is invalid");
            const auto volume_end = volume_start + volume_count;
            value.meshes.push_back({{lod.faces.begin()+start, lod.faces.begin()+end},
                {value.faces.begin()+start, value.faces.begin()+end},
                {volumes.begin()+volume_start,volumes.begin()+volume_end}});
            volume_start = volume_end;
        }
    }
}
MissionGeometry::MissionGeometry(const MissionRuntimeState& state, const MissionAssetCache& cache,
    const AssetStore& assets) : MissionGeometry([&] {
        std::vector<MissionGeometryModel> models;
        for (const auto& entry : state.models)
            models.push_back(mission_geometry_model(entry.handle, cache.model(entry.name), assets.read(entry.name).bytes));
        return models;
    }()) {}

const MissionGeometryModel& MissionGeometry::require(std::uint32_t handle) const {
    for (const auto& value : models_) if (value.handle == handle) return value;
    throw std::invalid_argument("mission geometry model handle is not loaded");
}
AiActorBounds MissionGeometry::bounds(std::uint32_t handle) const {
    return handle ? require(handle).bounds : AiActorBounds{};
}
std::int32_t MissionGeometry::height_68(std::uint32_t handle) const {
    return handle?require(handle).height_68:0;
}
ActorCollisionModel MissionGeometry::collision_model(std::uint32_t handle) const {
    if (!handle) return {};
    const auto& binding = require(handle);
    return {binding.bounds.radius_64,!binding.model->lods.front().collision_volumes.empty()};
}

std::optional<std::vector<ActorCollisionMesh>> MissionGeometry::collision_state(std::uint32_t handle,
    const AiActorImage& actor, const MissionGeometryMatrixSource& matrices) const {
    if (!handle) return {};
    const auto& binding = require(handle);
    const auto& lod = binding.model->lods.front();
    if (lod.collision_volumes.empty()) return {};
    MissionGeometryMatrices state;
    if (binding.model->render_types[0] == 0x676e7263u) state = generic_matrices(lod,actor);
    else {
        if (!matrices) throw std::invalid_argument("mission geometry tag requires a matrix source");
        const std::array<std::int32_t,3> position{signed_word(word(actor,0x24)),
            signed_word(word(actor,0x28)),signed_word(word(actor,0x2c))};
        state = matrices(binding,actor,position);
    }
    if (!state.present) return {};
    if (state.matrices.size() < lod.meshes.size())
        throw std::invalid_argument("mission geometry collision matrix slots are incomplete");
    std::vector<ActorCollisionMesh> result;
    result.reserve(lod.meshes.size());
    for (std::size_t mesh = 0; mesh < lod.meshes.size(); ++mesh) {
        ActorCollisionMesh value;
        value.matrix = state.matrices[mesh];
        for (std::size_t bound = 0; bound < 6; ++bound)
            value.bounds[bound] = signed_word(word(lod.meshes[mesh].raw,0x40+bound*4));
        value.volumes = binding.meshes[mesh].volumes;
        result.push_back(std::move(value));
    }
    return result;
}

std::optional<MissionGroundEntity> MissionGeometry::ground_entity(const AiActorImage& actor,
    const std::array<std::int32_t,3>& query, const MissionGeometryMatrixSource& matrices) const {
    const auto flags = word(actor, 0x6c);
    const bool alternate = (flags & 8) && word(actor, 0x14);
    const auto handle = word(actor, alternate ? 0x14 : 0x10);
    if (!handle) return {};
    const auto& binding = require(handle);
    const std::array<std::int32_t,2> position{
        signed_word(word(actor, 0x24)), signed_word(word(actor, 0x28))};
    for (std::size_t axis = 0; axis < 2; ++axis)
        if (query[axis] < add(position[axis], negate(binding.bounds.radius_64)) ||
            add(position[axis], binding.bounds.radius_64) < query[axis]) return {};

    const auto& model = *binding.model;
    const auto& lod = model.lods.front();
    MissionGeometryMatrices state;
    if (model.render_types[0] == 0x676e7263u) state = generic_matrices(lod, actor);
    else {
        if (!matrices) throw std::invalid_argument("mission geometry tag requires a matrix source");
        state = matrices(binding, actor, query);
    }
    MissionGroundEntity result;
    result.definition = std::make_unique<GroundDefinition>();
    result.definition->radius = binding.bounds.radius_64;
    result.entity.position = position;
    result.entity.flags = flags;
    if (alternate) result.entity.alternate = result.definition.get();
    else result.entity.definition = result.definition.get();
    if (!state.present) return result;
    if (state.matrices.size() < lod.meshes.size() || state.flags.size() < lod.meshes.size())
        throw std::invalid_argument("mission geometry matrix slots are incomplete");

    result.definition->pieces.reserve(lod.meshes.size());
    for (std::size_t mesh_index = 0; mesh_index < lod.meshes.size(); ++mesh_index) {
        const auto& mesh = lod.meshes[mesh_index];
        GroundPiece piece;
        piece.flags = state.flags[mesh_index];
        for (std::size_t bound = 0; bound < 4; ++bound)
            piece.bounds[bound] = signed_word(word(mesh.raw, 0x40 + bound*4));
        if (!(piece.flags & 3)) {
            const auto point = world_hit_local_point(inverse(state.matrices[mesh_index]), query);
            piece.point = {point[0], point[1]};
            if (in_bounds(piece.point, piece.bounds)) {
                if (mesh.face_start > lod.faces.size() || mesh.face_count > lod.faces.size() - mesh.face_start)
                    throw std::invalid_argument("mission geometry mesh face range is invalid");
                for (std::size_t index = mesh.face_start; index < mesh.face_start + mesh.face_count; ++index) {
                    const auto& face = lod.faces[index];
                    const auto& box = binding.faces[index];
                    if (face.material >= lod.materials.size())
                        throw std::invalid_argument("mission geometry face material is invalid");
                    GroundVolume volume;
                    volume.skip = (lod.materials[face.material].flags & 0x01000000u) != 0;
                    for (std::size_t bound = 0; bound < 4; ++bound) volume.bounds[bound] = box.bounds[bound];
                    if (!volume.skip && in_bounds(piece.point, volume.bounds)) {
                        const auto normal_index = std::size_t(mesh.normal_start) + face.face_normal;
                        if (normal_index >= lod.normals.size())
                            throw std::invalid_argument("mission geometry face normal is invalid");
                        const auto& normal = lod.normals[normal_index];
                        if (normal.direction[2] >= 0x40 || normal.direction[2] <= -0x40) {
                            std::array<std::array<std::int16_t,3>,3> vertices{};
                            for (std::size_t vertex = 0; vertex < 3; ++vertex) {
                                if (face.vertices[vertex] >= lod.vertices.size())
                                    throw std::invalid_argument("mission geometry face vertex is invalid");
                                vertices[vertex] = lod.vertices[face.vertices[vertex]].position;
                            }
                            volume.accepted = collision_point_in_face(point, vertices, 1);
                            if (volume.accepted) {
                                auto surface = point;
                                surface[2] = plane_height(normal, box.plane_constant, point);
                                volume.height = model_transform_point(state.matrices[mesh_index], surface)[2];
                            }
                        }
                    }
                    piece.volumes.push_back(volume);
                }
            }
        }
        result.definition->pieces.push_back(std::move(piece));
    }
    return result;
}

MissionGroundEntities MissionGeometry::ground_entities(const std::vector<MissionGeometryActor>& actors,
    const std::array<std::int32_t,3>& query, const MissionGeometryMatrixSource& matrices) const {
    MissionGroundEntities result;
    result.storage.reserve(actors.size());
    result.entities.reserve(actors.size());
    result.identities.reserve(actors.size());
    for (const auto& actor : actors) {
        if (!actor.image) throw std::invalid_argument("mission geometry actor image is missing");
        auto built = ground_entity(*actor.image, query, matrices);
        if (!built) continue;
        result.storage.push_back(std::move(*built));
        result.entities.push_back(result.storage.back().entity);
        result.identities.push_back(actor.identity);
    }
    return result;
}

WorldHitRecord MissionGeometry::world_hit_record(std::uint32_t identity, const AiActorImage& actor,
    const MissionGeometryMatrixSource& matrices) const {
    WorldHitRecord result;
    result.active = word(actor, 0);
    result.identity = identity;
    for (std::size_t axis = 0; axis < 3; ++axis)
        result.position[axis] = signed_word(word(actor, 0x24 + axis*4));
    const auto handle = word(actor, 0x10); // 42DB80 reads only the primary model.
    if (!result.active || !handle) { result.active = 0; return result; }
    result.radius = require(handle).bounds.height_60;
    result.mesh_source = [this,handle,actor,matrices] {
        return world_hit_meshes(handle,actor,matrices);
    };
    return result;
}

std::vector<WorldHitMesh> MissionGeometry::world_hit_meshes(std::uint32_t handle, const AiActorImage& actor,
    const MissionGeometryMatrixSource& matrices) const {
    const auto& binding = require(handle);
    const auto& lod = binding.model->lods.front();
    MissionGeometryMatrices state;
    if (binding.model->render_types[0] == 0x676e7263u) state = generic_matrices(lod, actor);
    else {
        if (!matrices) throw std::invalid_argument("mission geometry tag requires a matrix source");
        const std::array<std::int32_t,3> position{signed_word(word(actor,0x24)),
            signed_word(word(actor,0x28)),signed_word(word(actor,0x2c))};
        state = matrices(binding, actor, position);
    }
    std::vector<WorldHitMesh> result;
    if (!state.present) return result;
    if (state.matrices.size() < lod.meshes.size() || state.flags.size() < lod.meshes.size())
        throw std::invalid_argument("mission geometry matrix slots are incomplete");
    result.reserve(lod.meshes.size());
    for (std::size_t mesh = 0; mesh < lod.meshes.size(); ++mesh) {
        WorldHitMesh value;
        value.state_flags = state.flags[mesh];
        value.matrix = inverse(state.matrices[mesh]);
        value.normal_start = lod.meshes[mesh].normal_start;
        value.faces = &binding.meshes[mesh].faces;
        value.face_boxes = &binding.meshes[mesh].boxes;
        value.vertices = &lod.vertices;
        value.normals = &lod.normals;
        value.materials = &lod.materials;
        result.push_back(value);
    }
    return result;
}

std::vector<ProjectileHitMesh> MissionGeometry::projectile_meshes(std::uint32_t handle,
    const AiActorImage& actor,const std::vector<std::uint32_t>& material_ids,
    const MissionGeometryMatrixSource& matrices) const {
    const auto& binding=require(handle);
    if(material_ids.size()!=binding.model->lods.front().materials.size())
        throw std::invalid_argument("Projectile material bindings differ from the model table");
    auto meshes=world_hit_meshes(handle,actor,matrices);
    std::vector<ProjectileHitMesh> result;
    result.reserve(meshes.size());
    for(auto& mesh:meshes)result.push_back({std::move(mesh),material_ids});
    return result;
}

std::vector<WorldHitRecord> MissionGeometry::world_hit_records(const std::vector<MissionGeometryActor>& actors,
    const MissionGeometryMatrixSource& matrices) const {
    std::vector<WorldHitRecord> result;
    result.reserve(actors.size());
    for (const auto& actor : actors) {
        if (!actor.image) throw std::invalid_argument("mission geometry actor image is missing");
        result.push_back(world_hit_record(actor.identity, *actor.image, matrices));
    }
    return result;
}
} // namespace dfre
