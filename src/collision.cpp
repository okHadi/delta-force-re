#include "dfre/collision.hpp"

#include <cstring>
#include <stdexcept>

namespace dfre {
namespace {

std::uint32_t read_word(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    if (at + 4 > bytes.size()) throw std::invalid_argument("Collision data outside model");
    std::uint32_t value;
    std::memcpy(&value, bytes.data() + at, 4);
    return value;
}

std::int32_t read_signed(const std::vector<std::uint8_t>& bytes, std::size_t at) {
    return static_cast<std::int32_t>(read_word(bytes, at));
}

// Original x86 imul + shrd: the full 64-bit product shifted right, low 32 bits.
std::int32_t scaled_product(std::int32_t left, std::int32_t right, unsigned shift) {
    return static_cast<std::int32_t>(static_cast<std::int64_t>(left) * right >> shift);
}

std::int32_t wrap_add(std::int32_t left, std::int32_t right) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}

std::int32_t wrap_negate(std::int32_t value) {
    return static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(value));
}

struct ModelLayout {
    std::size_t data = 0;
    std::uint32_t vertices = 0, normals = 0, faces = 0, meshes = 0, materials = 0, volumes = 0;
    std::size_t vertex_offset = 0, normal_offset = 0, face_offset = 0, mesh_offset = 0, volume_offset = 0;
};

// Reads one stored v5 LOD block. The caller keeps the model bytes unchanged.
ModelLayout read_layout(const std::vector<std::uint8_t>& model, std::size_t lod_index) {
    if (model.size() < 128) throw std::invalid_argument("Collision model shorter than its header");
    if (std::memcmp(model.data(), "3DI", 3) != 0) throw std::invalid_argument("Collision model lacks the 3DI tag");
    if (model[3] != 5) throw std::invalid_argument("Collision data supports the stored v5 models");
    const auto lod_count = read_word(model, 20);
    if (lod_index >= lod_count) throw std::invalid_argument("Collision LOD outside the model");
    std::size_t at = 128;
    const auto textures = read_word(model, 124);
    for (std::uint32_t texture = 0; texture < textures; ++texture) {
        if (at + 36 > model.size()) throw std::invalid_argument("Collision texture outside the model");
        at += 36 + read_word(model, at + 16) + 1024;
    }
    ModelLayout layout;
    for (std::uint32_t lod = 0; lod <= lod_index; ++lod) {
        if (at + 184 > model.size()) throw std::invalid_argument("Collision LOD header outside the model");
        layout.vertices = read_word(model, at + 128);
        layout.normals = read_word(model, at + 136);
        layout.faces = read_word(model, at + 144);
        layout.meshes = read_word(model, at + 152);
        const auto hardpoints = read_word(model, at + 160);
        layout.materials = read_word(model, at + 168);
        layout.volumes = read_word(model, at + 176);
        layout.data = at + 184;
        layout.vertex_offset = layout.data;
        layout.normal_offset = layout.vertex_offset + std::size_t(layout.vertices) * 8;
        layout.face_offset = layout.normal_offset + std::size_t(layout.normals) * 8;
        layout.mesh_offset = layout.face_offset + std::size_t(layout.faces) * 72;
        const auto hardpoint_offset = layout.mesh_offset + std::size_t(layout.meshes) * 112;
        layout.volume_offset = hardpoint_offset + std::size_t(hardpoints) * 12;
        const auto total = std::size_t(layout.volumes) * 72 + std::size_t(layout.materials) * 76;
        if (layout.volume_offset + total > model.size()) throw std::invalid_argument("Collision LOD data outside the model");
        at = layout.volume_offset + total;
    }
    return layout;
}

} // namespace

CollisionVolume decode_collision_volume(const std::array<std::uint8_t, 72>& raw) {
    auto word = [&](std::size_t index) {
        std::int32_t value;
        std::memcpy(&value, raw.data() + index * 4, 4);
        return value;
    };
    CollisionVolume volume;
    volume.type = static_cast<std::uint32_t>(word(0));
    volume.flags = static_cast<std::uint32_t>(word(1));
    for (std::size_t axis = 0; axis < 3; ++axis) volume.center[axis] = word(2 + axis);
    volume.radius = word(5);
    volume.horizontal_radius = word(6);
    volume.horizontal_radius_copy = word(7);
    for (std::size_t axis = 0; axis < 3; ++axis) volume.half_extent[axis] = word(8 + axis);
    for (std::size_t bound = 0; bound < 6; ++bound) volume.bounds[bound] = word(12 + bound);
    return volume;
}

std::vector<CollisionVolume> decode_collision_volumes(const ModelLod& lod) {
    std::vector<CollisionVolume> volumes;
    volumes.reserve(lod.collision_volumes.size());
    for (const auto& raw : lod.collision_volumes) volumes.push_back(decode_collision_volume(raw));
    return volumes;
}

std::vector<CollisionMeshRange> collision_mesh_ranges(const std::vector<std::uint8_t>& model, std::size_t lod_index) {
    const auto layout = read_layout(model, lod_index);
    std::vector<CollisionMeshRange> ranges;
    ranges.reserve(layout.meshes);
    std::uint32_t start = 0;
    for (std::uint32_t mesh = 0; mesh < layout.meshes; ++mesh) {
        const auto at = layout.mesh_offset + std::size_t(mesh) * 112;
        const auto count = read_word(model, at + 0x1c);
        if (count > layout.volumes - start) throw std::invalid_argument("Mesh volume totals outside the LOD");
        ranges.push_back({start, count});
        start += count;
    }
    if (start != layout.volumes) throw std::invalid_argument("Mesh volume totals differ from the LOD count");
    return ranges;
}

std::vector<CollisionFaceBox> collision_face_boxes(const std::vector<std::uint8_t>& model, std::size_t lod_index) {
    const auto layout = read_layout(model, lod_index);
    std::vector<CollisionFaceBox> boxes;
    boxes.reserve(layout.faces);
    for (std::uint32_t face = 0; face < layout.faces; ++face) {
        const auto at = layout.face_offset + std::size_t(face) * 72;
        CollisionFaceBox box;
        box.plane_constant = read_signed(model, at + 0x28);
        for (std::size_t bound = 0; bound < 6; ++bound) box.bounds[bound] = read_signed(model, at + 0x2c + bound * 4);
        boxes.push_back(box);
    }
    return boxes;
}

CollisionRayResult terrain_ray_march(const TerrainGeometry& geometry, const CollisionSegment& ray) {
    if (geometry.height_bytes().size() != 1024 * 1024) return {false, 0};
    const std::int32_t limit = ray.limit >> 12;
    std::int32_t x = ray.start[0], z = ray.start[2];
    std::int32_t y = ray.start[1];
    const std::int32_t step_x = ray.delta[0], step_y = ray.delta[1], step_z = ray.delta[2];
    const std::int32_t sub_x = step_x >> 4, sub_y = step_y >> 4, sub_z = step_z >> 4;
    std::int32_t index = 0, samples = 0;
    auto inner = [&]() -> std::optional<CollisionRayResult> {
        do {
            index += 1;
            x = wrap_add(x, sub_x);
            y = wrap_add(y, sub_y);
            z = wrap_add(z, sub_z);
            if (index > limit) return CollisionRayResult{false, 0x10000};
            if (z < geometry.base_height(x, y)) return CollisionRayResult{true, wrap_add(0, index * -0x1000)};
            ++samples;
        } while (samples < 0x13);
        return std::nullopt;
    };
    for (;;) {
        if (limit < index) {
            if (auto result = inner()) return *result;
        }
        if (z > geometry.nearest_height(x, y)) {
            index += 0x10;
            x = wrap_add(x, step_x);
            y = wrap_add(y, step_y);
            z = wrap_add(z, step_z);
            continue;
        }
        index -= 0x10;
        x = wrap_add(x, wrap_negate(step_x));
        y = wrap_add(y, wrap_negate(step_y));
        z = wrap_add(z, wrap_negate(step_z));
        samples = 0;
        if (auto result = inner()) return *result;
    }
}

CollisionRayResult terrain_ray_march_detail(const TerrainGeometry& geometry, const AssetBytes& max_height,
    const CollisionSegment& ray) {
    // Original 0x47E4B0 returns the null gate-map pointer (zero) when the map
    // is absent, which the caller reads as the not-cleared code.
    if (max_height.size() != 1024 * 1024) return {false, 0};
    const std::int32_t limit = ray.limit >> 12;
    std::int32_t x = ray.start[0], z = ray.start[2];
    std::int32_t y = ray.start[1];
    const std::int32_t step_x = ray.delta[0], step_y = ray.delta[1], step_z = ray.delta[2];
    const std::int32_t sub_x = step_x >> 4, sub_y = step_y >> 4, sub_z = step_z >> 4;
    std::int32_t index = 0, samples = 0;
    for (;;) {
        if (index <= limit) {
            const auto index_at = (static_cast<std::uint32_t>(y) >> 16 & 1023) * 1024 +
                (static_cast<std::uint32_t>(x) >> 16 & 1023);
            if (z > static_cast<std::int32_t>(max_height[index_at]) * 0x4000) {
                index += 0x10;
                x = wrap_add(x, step_x);
                y = wrap_add(y, step_y);
                z = wrap_add(z, step_z);
                continue;
            }
            index -= 0x10;
            x = wrap_add(x, wrap_negate(step_x));
            y = wrap_add(y, wrap_negate(step_y));
            z = wrap_add(z, wrap_negate(step_z));
            samples = 0;
        }
        // Original 0x47E51B runs single detail samples until nineteen pass.
        for (;;) {
            index += 1;
            x = wrap_add(x, sub_x);
            y = wrap_add(y, sub_y);
            z = wrap_add(z, sub_z);
            if (index > limit) return {false, 0x10000};
            if (geometry.surface_height(x, y) > z) return {true, wrap_add(0, index * -0x1000)};
            ++samples;
            if (samples > 0x12) break;
        }
    }
}

bool collision_point_in_face(const std::array<std::int32_t, 3>& point,
    const std::array<std::array<std::int16_t, 3>, 3>& vertices, std::uint16_t normal_flags) {
    std::int32_t first = 0, second = 0, axis_first = 0, axis_second = 0;
    if (normal_flags & 1) {
        first = point[0]; second = point[1]; axis_first = 0; axis_second = 1;
    } else if (normal_flags & 2) {
        first = point[0]; second = point[2]; axis_first = 0; axis_second = 2;
    } else if (normal_flags & 4) {
        first = point[1]; second = point[2]; axis_first = 1; axis_second = 2;
    } else {
        throw std::invalid_argument("Face normal flags select no projection plane");
    }
    std::array<std::int32_t, 4> projected_first{}, projected_second{};
    for (std::size_t index = 0; index < 3; ++index) {
        projected_first[index] = static_cast<std::int32_t>(vertices[index][axis_first]) * 256;
        projected_second[index] = static_cast<std::int32_t>(vertices[index][axis_second]) * 256;
    }
    projected_first[3] = projected_first[0];
    projected_second[3] = projected_second[0];
    std::int32_t crossings = 0;
    std::int32_t side = projected_second[0] >= second ? 1 : 0;
    for (std::int32_t index = 0; index < 3; ++index) {
        const std::int32_t next_side = projected_second[index + 1] >= second ? 1 : 0;
        if (side != next_side) {
            std::int32_t bits = first > projected_first[index] ? 1 : 0;
            if (first > projected_first[index + 1]) bits |= 2;
            if (bits != 3) {
                bool counted = bits == 0;
                if (!counted) {
                    const std::int32_t rise = wrap_add(projected_first[index + 1], wrap_negate(projected_first[index]));
                    const std::int32_t run = wrap_add(projected_second[index + 1], wrap_negate(projected_second[index]));
                    const std::int64_t scaled = static_cast<std::int64_t>(rise) * 65536;
                    if (scaled / run > 2147483647 || scaled / run < -2147483648) {
                        throw std::invalid_argument("Face edge division outside the original range");
                    }
                    const auto slope = static_cast<std::int32_t>(scaled / run);
                    const auto edge = wrap_add(projected_first[index],
                        scaled_product(wrap_add(second, wrap_negate(projected_second[index])), slope, 16));
                    counted = edge >= first;
                }
                if (counted && ++crossings >= 2) return false;
            }
        }
        side = next_side;
    }
    return crossings == 1;
}

bool collision_segment_box(const std::array<std::int32_t, 3>& p1, const std::array<std::int32_t, 3>& p2,
    const std::array<std::int32_t, 6>& bounds, std::int32_t tolerance) noexcept {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        std::int32_t low = 0, high = 0;
        if (p1[axis] < p2[axis]) {
            low = wrap_add(p1[axis], wrap_negate(tolerance));
            high = wrap_add(p2[axis], tolerance);
        } else {
            low = wrap_add(p2[axis], wrap_negate(tolerance));
            high = wrap_add(p1[axis], tolerance);
        }
        if (high < bounds[axis * 2]) return false;
        if (low > bounds[axis * 2 + 1]) return false;
    }
    return true;
}

std::optional<std::array<std::int32_t, 3>> collision_face_ray(
    const std::array<std::int32_t, 3>& p1, const std::array<std::int32_t, 3>& p2,
    const std::array<std::int16_t, 3>& normal, std::uint16_t normal_flags, std::int32_t plane_constant,
    const std::array<std::array<std::int16_t, 3>, 3>& vertices, std::int32_t threshold,
    std::uint32_t material_flags) {
    auto plane_distance = [&](const std::array<std::int32_t, 3>& point) {
        std::int32_t total = plane_constant;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            total = wrap_add(total, scaled_product(static_cast<std::int32_t>(normal[axis]) * 256, point[axis], 22));
        }
        return total;
    };
    std::int32_t first = plane_distance(p1), second = plane_distance(p2);
    // Original 0x42D808 tests the first endpoint before the two-sided
    // negation at 0x42DAC6; a non-negative first distance keeps its sign.
    const bool reverse = first < 0 && (material_flags & 2) != 0;
    if (reverse) {
        first = wrap_negate(first);
        second = wrap_negate(second);
    }
    if (second >= threshold) return std::nullopt;
    if (second >= first) return std::nullopt;
    const std::int32_t spread = wrap_add(first, wrap_negate(second));
    const std::int32_t magnitude = spread < 0 ? wrap_negate(spread) : spread;
    if (magnitude < 0x10) return std::nullopt;
    constexpr std::int64_t original_numerator = 0xffff0000;
    const auto slope = static_cast<std::int32_t>(original_numerator / (second - first));
    std::array<std::int32_t, 3> base{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto along = scaled_product(wrap_add(p2[axis], wrap_negate(p1[axis])), slope, 16);
        base[axis] = wrap_add(p1[axis], scaled_product(along, wrap_add(threshold, wrap_negate(first)), 16));
    }
    std::array<std::int32_t, 3> point{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto offset = scaled_product(static_cast<std::int32_t>(normal[axis]) * 256, threshold, 22);
        point[axis] = reverse ? wrap_add(base[axis], offset) : wrap_add(base[axis], wrap_negate(offset));
    }
    if (!collision_point_in_face(point, vertices, normal_flags)) return std::nullopt;
    return point;
}

std::optional<CollisionModelHit> collision_model_ray(const std::vector<CollisionMeshView>& meshes,
    const std::array<std::int32_t, 3>& p1, const std::array<std::int32_t, 3>& p2,
    std::int32_t threshold, std::int32_t tolerance) {
    for (std::size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index) {
        const auto& mesh = meshes[mesh_index];
        if (mesh.state_flags & 3) continue;
        if (!collision_segment_box(p1, p2, mesh.bounds, tolerance)) continue;
        if (!mesh.faces || !mesh.face_boxes || !mesh.vertices || !mesh.normals || !mesh.materials) {
            throw std::invalid_argument("Collision mesh view lacks stored data");
        }
        if (mesh.faces->size() != mesh.face_boxes->size()) {
            throw std::invalid_argument("Collision face boxes differ from face count");
        }
        for (std::size_t face_index = 0; face_index < mesh.faces->size(); ++face_index) {
            const auto& face = (*mesh.faces)[face_index];
            const auto& box = (*mesh.face_boxes)[face_index];
            if (!collision_segment_box(p1, p2, box.bounds, tolerance)) continue;
            if (face.material >= mesh.materials->size()) throw std::invalid_argument("Collision material outside the LOD");
            const auto material_flags = (*mesh.materials)[face.material].flags;
            if (material_flags & 0x1000000u) continue;
            const auto normal_index = std::size_t(mesh.normal_start) + face.face_normal;
            if (normal_index >= mesh.normals->size()) throw std::invalid_argument("Collision normal outside the LOD");
            const auto& normal = (*mesh.normals)[normal_index];
            std::array<std::array<std::int16_t, 3>, 3> positions{};
            for (std::size_t corner = 0; corner < 3; ++corner) {
                if (face.vertices[corner] >= mesh.vertices->size()) {
                    throw std::invalid_argument("Collision vertex outside the LOD");
                }
                positions[corner] = (*mesh.vertices)[face.vertices[corner]].position;
            }
            if (auto point = collision_face_ray(p1, p2, normal.direction, normal.flags, box.plane_constant,
                    positions, threshold, material_flags)) {
                return CollisionModelHit{mesh_index, face_index, *point};
            }
        }
    }
    return std::nullopt;
}

} // namespace dfre
