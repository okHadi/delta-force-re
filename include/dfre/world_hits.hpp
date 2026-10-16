#pragma once
// World segment query: original 0x42E2B0 (terrain march, two model lists,
// water) and 0x42DB80 (model list hit selection). Sources use Df.exe SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
//
// Unresolved producers stay explicit inputs: the 0x114-byte list records, the
// per-mesh 0x45D170 matrices (built from a game callback result), the record
// positions and radii, and the two skip identities. The port applies the
// original arithmetic to those values only.

#include "dfre/collision.hpp"
#include "dfre/models.hpp"
#include "dfre/terrain.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dfre {

// Original 0x477620: integer square root with the final rounding step.
std::int32_t collision_sqrt(std::int32_t value);

// Original 0x477670: max + (11 * mid) / 32 + min / 4 with signed ordering.
std::int32_t collision_approximate_length(std::int32_t first, std::int32_t second, std::int32_t third);

// Original 0x45CE60: adds the translation before rotating, then shifts each
// product right by 22. The 0x45D170 output uses the ModelMatrix row-major
// layout (rotation rows at 0/4/8, translation at 3/7/11).
std::array<std::int32_t, 3> world_hit_local_point(const ModelMatrix& matrix, const std::array<std::int32_t, 3>& point);

// Original 0x45CF30: rotation without translation, each product shifted by 22.
std::array<std::int32_t, 3> world_hit_local_direction(const ModelMatrix& matrix,
    const std::array<std::int32_t, 3>& direction);

// One runtime mesh of a hit model, in mesh-array order. Vertices, normals,
// and materials are the runtime LOD tables; faces and boxes follow the stored
// face order. The matrix is the original 0x45D170 output and must map the
// query space onto the stored geometry space as the original state provider
// does; state_flags is the original parallel state record's byte at +0x3c.
struct WorldHitMesh {
    std::uint8_t state_flags = 0;
    ModelMatrix matrix{};
    std::uint32_t normal_start = 0;
    const std::vector<ModelFace>* faces = nullptr;
    const std::vector<CollisionFaceBox>* face_boxes = nullptr;
    const std::vector<ModelVertex>* vertices = nullptr;
    const std::vector<ModelNormal>* normals = nullptr;
    const std::vector<ModelMaterial>* materials = nullptr;
};

// One original 0x114-byte list record subset. identity stands in for the
// original record pointer used by the two skip checks.
struct WorldHitRecord {
    std::uint32_t active = 0;              // +0x00
    std::uint32_t identity = 0;            // original record pointer
    std::array<std::int32_t, 3> position{}; // +0x24
    std::int32_t radius = 0;               // model +0x60
    std::vector<WorldHitMesh> meshes;      // runtime slot mesh order
};

// Original query context. Origin/end/length and the bounds use the caller's
// world units (16.16 in the terrain path); direction is the normalized 16.16
// direction from 0x42E2B0; skip identities match WorldHitRecord::identity.
struct WorldHitQuery {
    std::array<std::int32_t, 3> origin{};
    std::array<std::int32_t, 3> end{};
    std::array<std::int32_t, 3> direction{};
    std::int32_t length = 0;
    std::array<std::int32_t, 6> bounds{};
    std::array<std::uint32_t, 2> skip{};
};

// Original query output fields plus the face slot the original keeps locally.
struct WorldModelHit {
    bool hit = false;
    bool divided = false;   // the original Check Divide Error fallback ran
    std::size_t record = 0; // query +0x60
    std::size_t mesh = 0;   // query +0x64
    std::size_t face = 0;   // kept locally by the original
    std::uint32_t material_flags = 0;
    std::int32_t distance = 0; // query +0x5c, 16.16 along the ray
    std::uint32_t masked = 0;  // faces that hit with material mask 0x868C set
};

// Original 0x42DB80. Returns 0 on a surface hit (fields in `hit`), otherwise
// masked faces plus one. Throws std::invalid_argument for malformed views.
std::int32_t world_model_list_hit(const std::vector<WorldHitRecord>& records, const WorldHitQuery& query,
    WorldModelHit& hit);

enum class WorldSegmentKind { zero_length, rejected, cleared, terrain, surface };

struct WorldSegmentResult {
    std::int32_t code = 0; // exact original 0x42E2B0 return value
    WorldSegmentKind kind = WorldSegmentKind::cleared;
    std::int32_t distance = 0; // surface hit distance or terrain march lambda
    std::size_t slot = 0;      // 0 for the first list, 1 for the second
    std::size_t record = 0, mesh = 0, face = 0;
    std::uint32_t material_flags = 0;
    std::uint32_t masked = 0;
    bool water_layer = false;
    bool divided = false;
};

// Original 0x42E2B0 with the two 0x42DB80 list calls and the water test.
// Throws std::invalid_argument when the original 0x100000000/length division
// would fault (length 1 or 2).
WorldSegmentResult world_segment_query(const TerrainGeometry& geometry, const AssetBytes& max_height,
    std::int32_t water_height, const std::vector<WorldHitRecord>& list_one,
    const std::vector<WorldHitRecord>& list_two, std::uint32_t skip_first, std::uint32_t skip_second,
    const std::array<std::int32_t, 3>& start, const std::array<std::int32_t, 3>& end);

} // namespace dfre
