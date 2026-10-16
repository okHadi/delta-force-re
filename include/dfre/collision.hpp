#pragma once
// Original DF1 collision data and geometric ray stage.
//
// Sources (Df.exe SHA-256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff):
//   0x485E30 LOD loader: per-mesh collision ranges, 72-byte volume records.
//   0x47E390 terrain ray march (base height).
//   0x47E4B0 terrain ray march (detail height, max-height gate).
//   0x42D460 model segment hit stage: mesh box gate, face box gate, face plane
//           crossing, and 0x42D2A0 projected point-in-face test.
// Field units follow stored model data; see docs/collision-runtime.md.

#include "dfre/assets.hpp"
#include "dfre/models.hpp"
#include "dfre/terrain.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace dfre {

// One stored v5 collision volume record. All numbers are stored little-endian
// words of the 72-byte record; positions and bounds use 24.8 units.
struct CollisionVolume {
    std::uint32_t type = 0;  // word 0: 1 box, 3 cylinder in every checked asset
    std::uint32_t flags = 0; // word 1: zero in every checked asset
    std::array<std::int32_t, 3> center{}; // 24.8; cylinder stores bounds min z in z
    std::int32_t radius = 0;              // 24.8, bounding radius about center
    std::int32_t horizontal_radius = 0;   // 24.8, horizontal (x/y) radius
    std::int32_t horizontal_radius_copy = 0; // stored duplicate of the same value
    std::array<std::int32_t, 3> half_extent{}; // 24.8, (max - min) >> 1 per axis
    std::array<std::int32_t, 6> bounds{}; // min x, max x, min y, max y, min z, max z
};

// Decodes one stored record. Every 72-byte value is accepted; the caller keeps
// the source bytes. Throws std::invalid_argument only for a null record.
CollisionVolume decode_collision_volume(const std::array<std::uint8_t, 72>& raw);
std::vector<CollisionVolume> decode_collision_volumes(const ModelLod& lod);

// Per-mesh volume slices decoded from a stored v5 model. The v5 mesh record
// keeps the volume count at +0x1c and the first volume index at +0x20; the
// original loader replaces the first index with a runtime pointer.
struct CollisionMeshRange {
    std::uint32_t start = 0;
    std::uint32_t count = 0;
};
std::vector<CollisionMeshRange> collision_mesh_ranges(const std::vector<std::uint8_t>& model, std::size_t lod_index);

// Face plane and bounds stored after the decoded face fields. Both use 24.8
// units; the plane value is the stored constant added after the normal
// products. Original instructions read the plane at face +0x28 and the six
// bounds words at +0x2c through +0x43.
struct CollisionFaceBox {
    std::int32_t plane_constant = 0;
    std::array<std::int32_t, 6> bounds{};
};
std::vector<CollisionFaceBox> collision_face_boxes(const std::vector<std::uint8_t>& model, std::size_t lod_index);

// Mesh-local segment for the terrain and model stages. Start and delta use
// 16.16 terrain coordinates for the terrain marches (z uses the fixed height
// units of TerrainGeometry) and 24.8 units for the model stage. Limit keeps
// its original meaning: for the terrain marches it is the segment length in
// 16.16 units, so limit >> 12 is the sample index bound.
struct CollisionSegment {
    std::array<std::int32_t, 3> start{};
    std::array<std::int32_t, 3> delta{};
    std::int32_t limit = 0;
};

struct CollisionRayResult {
    bool hit = false;
    // Original return: no hit keeps lambda = 1.0 (0x10000). A hit returns
    // -(index * 0x1000) in the original, which is -(lambda * 0x10000) with
    // lambda counted over 16 sub-steps of delta. Zero means the map was absent.
    std::int32_t lambda = 0x10000;
};

// Original 0x47E390. Base-height walk over the height map; the coarse gate
// samples the nearest height byte times 0x4000.
CollisionRayResult terrain_ray_march(const TerrainGeometry& geometry, const CollisionSegment& ray);

// Original 0x47E4B0. Detail walk against TerrainGeometry::surface_height with
// an 18-sample inner limit. The coarse gate samples the stored max-height map
// (original global 0x00BD52D4, built by 0x47D9E0); pass
// build_terrain_height_bounds(geometry).front() for the original data.
CollisionRayResult terrain_ray_march_detail(const TerrainGeometry& geometry, const AssetBytes& max_height,
    const CollisionSegment& ray);

// Original 0x42D2A0. Point-in-face test with the projection plane selected by
// the face normal's flags: bit 0 uses x/y, bit 1 uses x/z, bit 2 uses y/z.
// Vertices are mesh-local i16 xyz; the point uses 24.8 units (vertices shift
// left by 8 during the test). Exact signed division and shift rules are kept.
// Throws std::invalid_argument when the original division would fault.
bool collision_point_in_face(const std::array<std::int32_t, 3>& point,
    const std::array<std::array<std::int16_t, 3>, 3>& vertices, std::uint16_t normal_flags);

// Original 0x42D460 face stage. p1 and p2 are mesh-local 24.8 segment ends.
// normal uses the stored i16 components; plane_constant is CollisionFaceBox
// ::plane_constant; threshold is the caller's plane threshold (original
// context +0x20, stored in ebx before the test); material_flags selects the
// two-sided negation at material +0x10 bit 1. Returns the accepted point.
std::optional<std::array<std::int32_t, 3>> collision_face_ray(
    const std::array<std::int32_t, 3>& p1, const std::array<std::int32_t, 3>& p2,
    const std::array<std::int16_t, 3>& normal, std::uint16_t normal_flags, std::int32_t plane_constant,
    const std::array<std::array<std::int16_t, 3>, 3>& vertices, std::int32_t threshold,
    std::uint32_t material_flags);

// Original segment box overlap used by the mesh and face gates. The segment
// axis range is expanded by tolerance on both ends, then compared with the
// stored bounds with strict comparisons. Works on 24.8 values.
bool collision_segment_box(const std::array<std::int32_t, 3>& p1, const std::array<std::int32_t, 3>& p2,
    const std::array<std::int32_t, 6>& bounds, std::int32_t tolerance) noexcept;

// One mesh of the model ray stage. Faces are tested in stored order.
struct CollisionMeshView {
    std::array<std::int32_t, 6> bounds{};
    // Per-mesh state flags from the original parallel 0x40-byte record
    // (bit 0 and bit 1 skip the mesh). Pass 0 when the caller has none.
    std::uint8_t state_flags = 0;
    // Face records are LOD-global; these starts map a face normal to the LOD
    // normal vector. Face vertex indexes are LOD-global into `vertices`.
    std::uint32_t normal_start = 0;
    const std::vector<ModelFace>* faces = nullptr;     // stored face fields
    const std::vector<CollisionFaceBox>* face_boxes = nullptr;
    const std::vector<ModelVertex>* vertices = nullptr; // LOD positions
    const std::vector<ModelNormal>* normals = nullptr;  // LOD normals
    const std::vector<ModelMaterial>* materials = nullptr;
};

struct CollisionModelHit {
    std::size_t mesh = 0;
    std::size_t face = 0;
    std::array<std::int32_t, 3> point{}; // mesh-local 24.8 hit point
};

// Walks meshes in order with the original mesh gate, face gate, plane
// crossing, and point test. The segment and box values use 24.8 units.
// `vertices` and `normals` are the LOD-level tables: face.vertices indexes
// `vertices` directly and the face normal is `normal_start + face_normal`;
// the materials vector is the LOD material table. threshold and tolerance
// carry the caller's original context values.
std::optional<CollisionModelHit> collision_model_ray(const std::vector<CollisionMeshView>& meshes,
    const std::array<std::int32_t, 3>& p1, const std::array<std::int32_t, 3>& p2,
    std::int32_t threshold, std::int32_t tolerance);

} // namespace dfre
