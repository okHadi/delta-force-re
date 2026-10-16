#include "dfre/world_hits.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dfre {
namespace {

// Original x86 imul + shrd: the full 64-bit product shifted right, low 32 bits.
std::int32_t shifted_product(std::int32_t left, std::int32_t right, unsigned shift) {
    return static_cast<std::int32_t>(static_cast<std::int64_t>(left) * right >> shift);
}

std::int32_t wrap_add(std::int32_t left, std::int32_t right) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}

std::int32_t wrap_negate(std::int32_t value) {
    return static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(value));
}

std::int32_t wrap_multiply(std::int32_t left, std::int32_t right) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) * static_cast<std::uint32_t>(right));
}

// Original 0x42DB80 plane distance: each normal component shifted left by two
// is multiplied by the mesh-local point, shifted right by sixteen, and summed
// with the stored plane constant using 32-bit wrap.
std::int32_t hit_plane_distance(const ModelNormal& normal, std::int32_t plane, const std::array<std::int32_t, 3>& point) {
    std::int32_t total = plane;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        total = wrap_add(total, shifted_product(static_cast<std::int32_t>(normal.direction[axis]) * 4, point[axis], 16));
    }
    return total;
}

const std::vector<ModelFace>& require_faces(const WorldHitMesh& mesh) {
    if (!mesh.faces || !mesh.face_boxes || !mesh.vertices || !mesh.normals || !mesh.materials) {
        throw std::invalid_argument("World hit mesh lacks stored tables");
    }
    if (mesh.faces->size() != mesh.face_boxes->size()) {
        throw std::invalid_argument("World hit face boxes differ from the face count");
    }
    return *mesh.faces;
}

} // namespace

std::int32_t collision_sqrt(std::int32_t value) {
    if (value <= 1) return value;
    std::uint32_t bits = static_cast<std::uint32_t>(value);
    std::uint32_t remainder = 0, root = 0;
    for (int step = 0; step < 16; ++step) {
        remainder = (remainder << 2) | ((bits >> 30) & 3u);
        bits <<= 2;
        root <<= 1;
        const std::uint32_t trial = root * 2 + 1;
        if (remainder >= trial) {
            root += 1;
            remainder -= trial;
        }
    }
    if (static_cast<std::uint32_t>(value) - root * root >= root - 1) ++root;
    return static_cast<std::int32_t>(root);
}

std::int32_t collision_approximate_length(std::int32_t first, std::int32_t second, std::int32_t third) {
    std::int32_t largest = first, middle = second, smallest = third;
    if (largest < middle) std::swap(largest, middle);
    if (largest < smallest) std::swap(largest, smallest);
    if (middle < smallest) std::swap(middle, smallest);
    return wrap_add(wrap_add(largest, wrap_multiply(middle, 11) >> 5), smallest >> 2);
}

std::array<std::int32_t, 3> world_hit_local_point(const ModelMatrix& matrix, const std::array<std::int32_t, 3>& point) {
    std::array<std::int32_t, 3> result{};
    for (std::size_t row = 0; row < 3; ++row) {
        std::int32_t total = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto translated = wrap_add(point[axis], matrix[row * 4 + 3]);
            total = wrap_add(total, shifted_product(matrix[row * 4 + axis], translated, 22));
        }
        result[row] = total;
    }
    return result;
}

std::array<std::int32_t, 3> world_hit_local_direction(const ModelMatrix& matrix, const std::array<std::int32_t, 3>& direction) {
    std::array<std::int32_t, 3> result{};
    for (std::size_t row = 0; row < 3; ++row) {
        std::int32_t total = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            total = wrap_add(total, shifted_product(matrix[row * 4 + axis], direction[axis], 22));
        }
        result[row] = total;
    }
    return result;
}

std::int32_t world_model_list_hit(const std::vector<WorldHitRecord>& records, const WorldHitQuery& query,
    WorldModelHit& hit) {
    hit = WorldModelHit{};
    std::uint32_t masked = 0;
    for (std::size_t record_index = 0; record_index < records.size(); ++record_index) {
        const auto& record = records[record_index];
        if (record.active == 0) continue;
        if (record.identity == query.skip[0] || record.identity == query.skip[1]) continue;
        const auto radius = record.radius;
        // Query bounds against the record position expanded by the radius.
        if (!(query.bounds[0] <= wrap_add(record.position[0], radius))) continue;
        if (!(wrap_add(record.position[0], wrap_negate(radius)) <= query.bounds[1])) continue;
        if (!(query.bounds[2] <= wrap_add(record.position[1], radius))) continue;
        if (!(wrap_add(record.position[1], wrap_negate(radius)) <= query.bounds[3])) continue;
        if (!(query.bounds[4] <= wrap_add(record.position[2], radius))) continue;
        if (!(wrap_add(record.position[2], wrap_negate(radius)) <= query.bounds[5])) continue;
        // Ray against the record sphere using the original approximate length.
        std::int32_t along = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            along = wrap_add(along, shifted_product(wrap_add(record.position[axis], wrap_negate(query.origin[axis])),
                query.direction[axis], 16));
        }
        std::array<std::int32_t, 3> closest{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            closest[axis] = wrap_add(query.origin[axis], shifted_product(along, query.direction[axis], 16));
        }
        const auto distance = collision_approximate_length(
            wrap_add(closest[0], wrap_negate(record.position[0])),
            wrap_add(closest[1], wrap_negate(record.position[1])),
            wrap_add(closest[2], wrap_negate(record.position[2])));
        if (distance > radius) continue;
        for (std::size_t mesh_index = 0; mesh_index < record.meshes.size(); ++mesh_index) {
            const auto& mesh = record.meshes[mesh_index];
            if (mesh.state_flags & 1) continue;
            const auto& faces = require_faces(mesh);
            const auto local_origin = world_hit_local_point(mesh.matrix, query.origin);
            const auto local_end = world_hit_local_point(mesh.matrix, query.end);
            const auto local_direction = world_hit_local_direction(mesh.matrix, query.direction);
            std::array<std::int32_t, 3> minimum{}, maximum{};
            for (std::size_t axis = 0; axis < 3; ++axis) {
                minimum[axis] = std::min(local_origin[axis], local_end[axis]);
                maximum[axis] = std::max(local_origin[axis], local_end[axis]);
            }
            for (std::size_t face_index = 0; face_index < faces.size(); ++face_index) {
                const auto& face = faces[face_index];
                const auto& box = (*mesh.face_boxes)[face_index];
                if (!(minimum[0] <= box.bounds[1] && box.bounds[0] <= maximum[0])) continue;
                if (!(minimum[1] <= box.bounds[3] && box.bounds[2] <= maximum[1])) continue;
                if (!(minimum[2] <= box.bounds[5] && box.bounds[4] <= maximum[2])) continue;
                if (face.material >= mesh.materials->size()) throw std::invalid_argument("World hit material outside the table");
                const auto& material = (*mesh.materials)[face.material];
                if (material.flags & 0x800u) continue;
                const auto normal_index = std::size_t(mesh.normal_start) + face.face_normal;
                if (normal_index >= mesh.normals->size()) throw std::invalid_argument("World hit normal outside the table");
                const auto& normal = (*mesh.normals)[normal_index];
                const auto dot_origin = hit_plane_distance(normal, box.plane_constant, local_origin);
                const auto dot_end = hit_plane_distance(normal, box.plane_constant, local_end);
                bool crosses = false;
                if ((material.flags & 2) == 0) {
                    crosses = dot_origin > 0 && dot_end < 1;
                } else {
                    crosses = dot_origin > 0 ? dot_end < 1 : dot_end > 0;
                }
                if (!crosses) continue;
                const auto magnitude_origin = dot_origin < 0 ? wrap_negate(dot_origin) : dot_origin;
                const auto magnitude_end = dot_end < 0 ? wrap_negate(dot_end) : dot_end;
                const auto magnitude_total = wrap_add(magnitude_origin, magnitude_end);
                std::int32_t distance_along = 0;
                bool divided = false;
                if (magnitude_total < magnitude_origin) {
                    // Original Check Divide Error path keeps 0x40000000 and prints.
                    distance_along = 0x40000000;
                    divided = true;
                } else {
                    const auto fraction = static_cast<std::int32_t>(
                        (static_cast<std::int64_t>(magnitude_origin) * 0x10000) / magnitude_total);
                    distance_along = shifted_product(query.length, fraction, 16);
                }
                std::array<std::int32_t, 3> point{};
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    point[axis] = wrap_add(local_origin[axis], shifted_product(distance_along, local_direction[axis], 16));
                }
                std::array<std::array<std::int16_t, 3>, 3> positions{};
                for (std::size_t corner = 0; corner < 3; ++corner) {
                    if (face.vertices[corner] >= mesh.vertices->size()) {
                        throw std::invalid_argument("World hit vertex outside the table");
                    }
                    positions[corner] = (*mesh.vertices)[face.vertices[corner]].position;
                }
                if (!collision_point_in_face(point, positions, normal.flags)) continue;
                if ((material.flags & 0x868cu) == 0) {
                    hit.hit = true;
                    hit.divided = divided;
                    hit.record = record_index;
                    hit.mesh = mesh_index;
                    hit.face = face_index;
                    hit.material_flags = material.flags;
                    hit.distance = distance_along;
                    hit.masked = masked;
                    return 0;
                }
                masked += 1;
            }
        }
    }
    hit.masked = masked;
    return static_cast<std::int32_t>(masked + 1);
}

WorldSegmentResult world_segment_query(const TerrainGeometry& geometry, const AssetBytes& max_height,
    std::int32_t water_height, const std::vector<WorldHitRecord>& list_one,
    const std::vector<WorldHitRecord>& list_two, std::uint32_t skip_first, std::uint32_t skip_second,
    const std::array<std::int32_t, 3>& start, const std::array<std::int32_t, 3>& end) {
    WorldSegmentResult result;
    std::array<std::int32_t, 3> delta{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        delta[axis] = wrap_add(end[axis], wrap_negate(start[axis]));
    }
    std::int32_t sum = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        sum = wrap_add(sum, shifted_product(delta[axis], delta[axis], 22));
    }
    const auto length = wrap_multiply(collision_sqrt(sum), 0x800);
    if (length == 0) {
        result.code = 1;
        result.kind = WorldSegmentKind::zero_length;
        return result;
    }
    if (length >= 0x3200000) {
        result.code = 0;
        result.kind = WorldSegmentKind::rejected;
        return result;
    }
    constexpr std::int64_t reciprocal_dividend = 0x100000000;
    if (reciprocal_dividend / length > 2147483647) {
        throw std::invalid_argument("World segment length divides outside the original range");
    }
    const auto reciprocal = static_cast<std::int32_t>(reciprocal_dividend / length);
    WorldHitQuery query;
    query.origin = start;
    query.end = end;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        query.direction[axis] = shifted_product(delta[axis], reciprocal, 16);
    }
    query.length = length;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        query.bounds[axis * 2] = std::min(start[axis], end[axis]);
        query.bounds[axis * 2 + 1] = std::max(start[axis], end[axis]);
    }
    query.skip = {skip_first, skip_second};
    const auto march = terrain_ray_march_detail(geometry, max_height, CollisionSegment{start, query.direction, length});
    if (march.hit || march.lambda != 0x10000) {
        result.code = march.lambda;
        result.kind = march.hit ? WorldSegmentKind::terrain : WorldSegmentKind::rejected;
        result.distance = march.lambda;
        return result;
    }
    WorldModelHit first;
    const auto first_count = world_model_list_hit(list_one, query, first);
    if (first_count == 0) {
        result.code = wrap_negate(first.distance);
        result.kind = WorldSegmentKind::surface;
        result.distance = first.distance;
        result.slot = 0;
        result.record = first.record;
        result.mesh = first.mesh;
        result.face = first.face;
        result.material_flags = first.material_flags;
        result.divided = first.divided;
        return result;
    }
    WorldModelHit second;
    const auto second_count = world_model_list_hit(list_two, query, second);
    if (second_count == 0) {
        result.code = wrap_negate(second.distance);
        result.kind = WorldSegmentKind::surface;
        result.distance = second.distance;
        result.slot = 1;
        result.record = second.record;
        result.mesh = second.mesh;
        result.face = second.face;
        result.material_flags = second.material_flags;
        result.divided = second.divided;
        return result;
    }
    std::uint32_t masked = 0;
    masked += static_cast<std::uint32_t>(first_count - 1);
    masked += static_cast<std::uint32_t>(second_count - 1);
    result.masked = masked;
    result.divided = first.divided || second.divided;
    if (water_height != 0 && !(water_height < start[2] && water_height < end[2])) {
        result.water_layer = true;
        masked += 1;
    }
    result.code = static_cast<std::int32_t>(masked + 1);
    result.kind = WorldSegmentKind::cleared;
    return result;
}

} // namespace dfre
