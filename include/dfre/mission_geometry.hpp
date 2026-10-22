#pragma once
#include "dfre/ai_update.hpp"
#include "dfre/collision.hpp"
#include "dfre/motion_clearance.hpp"
#include "dfre/resources.hpp"
#include "dfre/world_runtime.hpp"
#include "dfre/world_hits.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace dfre {
struct MissionGeometryMesh {
    std::vector<ModelFace> faces;
    std::vector<CollisionFaceBox> boxes;
};
struct MissionGeometryModel {
    std::uint32_t handle{};
    const Model* model{}; // Localized cache model; must outlive this view.
    AiActorBounds bounds{}; // Stored model header +64/+60.
    std::vector<CollisionFaceBox> faces; // First LOD, stored face order.
    std::vector<MissionGeometryMesh> meshes; // Stable per-mesh views for world hits.
};
MissionGeometryModel mission_geometry_model(std::uint32_t handle,
    const Model& localized, const AssetBytes& source);

struct MissionGeometryMatrices {
    bool present{true}; // False is the original matrix callback's null result.
    std::vector<ModelMatrix> matrices;
    std::vector<std::uint8_t> flags; // Parallel instance byte +3C.
};
// Supply the first-LOD matrix callback result for non-gnrc tags.
using MissionGeometryMatrixSource = std::function<MissionGeometryMatrices(
    const MissionGeometryModel&, const AiActorImage&, const std::array<std::int32_t,3>&)>;
// Original first-LOD matrix callbacks, with third argument zero. Supply CHR
// and current pose when org0 actor +18 is nonzero. Other known tags use the
// current actor fields and model hardpoints. Unknown tags raise an error.
// org0 and tire preserve scratch +3C flags, so prior_flags is required for them.
// Palm selectors 10/11/20 copy a stack flag; supply palm_stack_flag for them.
MissionGeometryMatrices mission_geometry_callback_matrices(const MissionGeometryModel& binding,
    const AiActorImage& actor, const CharacterModel* character = nullptr,
    const std::vector<CharacterBonePose>* pose = nullptr,
    const std::vector<std::uint8_t>* prior_flags = nullptr,
    std::optional<std::uint8_t> palm_stack_flag = std::nullopt);

struct MissionGroundEntity {
    std::unique_ptr<GroundDefinition> definition;
    GroundEntity entity;
    MissionGroundEntity() = default;
    MissionGroundEntity(MissionGroundEntity&&) noexcept = default;
    MissionGroundEntity& operator=(MissionGroundEntity&&) noexcept = default;
    MissionGroundEntity(const MissionGroundEntity&) = delete;
    MissionGroundEntity& operator=(const MissionGroundEntity&) = delete;
};
struct MissionGeometryActor {
    std::uint32_t identity{};
    const AiActorImage* image{};
};
struct MissionGroundEntities {
    std::vector<MissionGroundEntity> storage;
    std::vector<GroundEntity> entities;
    std::vector<std::uint32_t> identities;
};

class MissionGeometry {
public:
    explicit MissionGeometry(std::vector<MissionGeometryModel> models);
    MissionGeometry(const MissionRuntimeState& state, const MissionAssetCache& cache,
        const AssetStore& assets);
    AiActorBounds bounds(std::uint32_t handle) const;
    std::optional<MissionGroundEntity> ground_entity(const AiActorImage& actor,
        const std::array<std::int32_t,3>& query,
        const MissionGeometryMatrixSource& matrices = {}) const;
    MissionGroundEntities ground_entities(const std::vector<MissionGeometryActor>& actors,
        const std::array<std::int32_t,3>& query,
        const MissionGeometryMatrixSource& matrices = {}) const;
    // 42DB80 uses primary +10, radius +60 and one inverse matrix per mesh.
    // Matrix work is deferred until the query passes bounds and sphere gates.
    // The callback owns its actor snapshot; mesh views use this object's storage.
    WorldHitRecord world_hit_record(std::uint32_t identity, const AiActorImage& actor,
        const MissionGeometryMatrixSource& matrices = {}) const;
    std::vector<WorldHitRecord> world_hit_records(const std::vector<MissionGeometryActor>& actors,
        const MissionGeometryMatrixSource& matrices = {}) const;
private:
    std::vector<WorldHitMesh> world_hit_meshes(std::uint32_t handle, const AiActorImage& actor,
        const MissionGeometryMatrixSource& matrices) const;
    const MissionGeometryModel& require(std::uint32_t handle) const;
    std::vector<MissionGeometryModel> models_;
};
} // namespace dfre
