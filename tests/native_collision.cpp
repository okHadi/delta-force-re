// Native checks for the collision stage. Goldens come from the ported
// original-code replay in tools/check_collision_original.py.
#include "dfre/collision.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class Function> void rejects(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("Invalid collision data accepted");
}

constexpr std::uint32_t map_pixels = 1024 * 1024;

dfre::AssetBytes make_heights() {
    dfre::AssetBytes heights(map_pixels);
    for (std::uint32_t y = 0; y < 1024; ++y) for (std::uint32_t x = 0; x < 1024; ++x) {
        heights[y * 1024 + x] = static_cast<std::uint8_t>(x * 13 + y * 7 + (x ^ y));
    }
    return heights;
}

dfre::AssetBytes make_characters() {
    dfre::AssetBytes characters(map_pixels);
    for (std::uint32_t y = 0; y < 1024; ++y) for (std::uint32_t x = 0; x < 1024; ++x) {
        characters[y * 1024 + x] = static_cast<std::uint8_t>(x + y * 3);
    }
    return characters;
}

std::vector<std::uint32_t> make_detail() {
    std::vector<std::uint32_t> detail(65536);
    for (std::uint32_t tile = 0; tile < 256; ++tile) for (std::uint32_t at = 0; at < 256; ++at) {
        detail[tile * 256 + at] = ((tile * 17 + at * 3) & 255) << 16;
    }
    return detail;
}

dfre::AssetBytes make_max_height() {
    dfre::AssetBytes bounds(map_pixels);
    for (std::uint32_t y = 0; y < 1024; ++y) for (std::uint32_t x = 0; x < 1024; ++x) {
        bounds[y * 1024 + x] = static_cast<std::uint8_t>(x * 31 + y * 19 + ((x * y) & 255));
    }
    return bounds;
}

void marches() {
    const dfre::TerrainGeometry geometry(make_heights(), make_characters(), make_detail());
    const auto bounds = make_max_height();
    struct Case { std::int32_t limit, x, y, z, dx, dy, dz, lambda; bool hit; };
    const Case base[] = {
        {0x10000, 0x100000, 0x100000, 0x300000, 0x8000, 0x4000, -0x10000, 0x10000, false},
        {0x40000, 0x200000, 0x300000, 0x10000, 0x10000, 0, 0x10000, -28672, true},
        {0x10000, 0, 0, 0x4000000, 0x10000, 0x10000, 0, 0x10000, false},
        {0x200000, 0x120000, 0x340000, 0x220000, -0x20000, 0x30000, 0x2000, -262144, true},
    };
    for (const auto& item : base) {
        dfre::CollisionSegment ray{{item.x, item.y, item.z}, {item.dx, item.dy, item.dz}, item.limit};
        const auto result = dfre::terrain_ray_march(geometry, ray);
        require(result.hit == item.hit && result.lambda == item.lambda, "Base terrain march differs");
    }
    const Case detail[] = {
        {0x10000, 0x100000, 0x100000, 0x300000, 0x8000, 0x4000, -0x10000, 0x10000, false},
        {0x40000, 0x200000, 0x300000, 0x10000, 0x10000, 0, 0x10000, 61440, true},
        {0x10000, 0, 0, 0x4000000, 0x10000, 0x10000, 0, 0x10000, false},
        {0x200000, 0x120000, 0x340000, 0x220000, -0x20000, 0x30000, 0x2000, -106496, true},
    };
    for (const auto& item : detail) {
        dfre::CollisionSegment ray{{item.x, item.y, item.z}, {item.dx, item.dy, item.dz}, item.limit};
        const auto result = dfre::terrain_ray_march_detail(geometry, bounds, ray);
        require(result.hit == item.hit && result.lambda == item.lambda, "Detail terrain march differs");
    }
    // Original 0x47E4B0 returns zero, the null gate-map pointer, when the map is absent.
    const dfre::AssetBytes empty;
    dfre::CollisionSegment ray{{0, 0, 0}, {0x10000, 0x10000, 0}, 0x10000};
    require(dfre::terrain_ray_march_detail(geometry, empty, ray).lambda == 0, "Missing bound map accepted");
}

void face_tests() {
    struct Case {
        std::uint16_t flags;
        std::array<std::int32_t, 3> point;
        std::array<std::array<std::int16_t, 3>, 3> vertices;
        bool inside;
    };
    const Case cases[] = {
        {1, {0x200000, 0x100000, 0}, {{{0, 0, 0}, {0x4000, 0, 0}, {0, 0x4000, 0}}}, true},
        {1, {0x300000, 0x300000, 0}, {{{0, 0, 0}, {0x4000, 0, 0}, {0, 0x4000, 0}}}, false},
        {4, {0, 0x100000, 0x100000}, {{{0, 0, 0}, {0, 0x4000, 0}, {0, 0, 0x4000}}}, true},
        {2, {0x100000, 0, 0x100000}, {{{0, 0, 0}, {0x4000, 0, 0}, {0, 0, 0x4000}}}, true},
    };
    for (const auto& item : cases) {
        require(dfre::collision_point_in_face(item.point, item.vertices, item.flags) == item.inside,
            "Point-in-face differs");
    }
    rejects([] {
        dfre::collision_point_in_face({0, 0, 0}, {{{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}}, 0);
    });
}

void face_ray_tests() {
    struct Hit { std::int32_t x, y, z; };
    const std::array<std::array<std::int16_t, 3>, 3> yz_face{{{0, 0, 0}, {0, 16384, 0}, {0, 0, 16384}}};
    const std::array<std::int16_t, 3> normal{16384, 0, 0};
    // Accepted crossing of the x=0 face with the projected point inside.
    auto point = dfre::collision_face_ray({0x4000, 0x100000, 0x100000}, {-0x4000, 0x100000, 0x100000},
        normal, 4, 0, yz_face, 0, 0);
    require(point.has_value(), "Face crossing rejected");
    require((*point)[0] == 0 && (*point)[1] == 0x100000 && (*point)[2] == 0x100000, "Face crossing point differs");
    // Accepted two-sided crossing with a negative first distance.
    point = dfre::collision_face_ray({-0x4000, 0x80000, 0x80000}, {0x4000, 0x80000, 0x80000},
        normal, 4, 0, yz_face, 0, 2);
    require(point.has_value(), "Two-sided crossing rejected");
    require((*point)[0] == -1 && (*point)[1] == 0x80000 && (*point)[2] == 0x80000, "Two-sided point differs");
    // Rejected projected point.
    point = dfre::collision_face_ray({0x4000, 0x300000, 0x300000}, {-0x4000, 0x300000, 0x300000},
        normal, 4, 0, yz_face, 0, 0);
    require(!point.has_value(), "Outside point accepted");
    // Same-side segment keeps the two-side negation unused.
    point = dfre::collision_face_ray({0x2000, 0x100000, 0x100000}, {0x4000, 0x100000, 0x100000},
        normal, 4, 0, yz_face, 0, 2);
    require(!point.has_value(), "Same-side segment accepted");
    // The z-axis face with an x/y projection.
    const std::array<std::array<std::int16_t, 3>, 3> xy_face{{{0, 0, 0}, {16384, 0, 0}, {0, 12288, 0}}};
    point = dfre::collision_face_ray({0x100000, 0x100000, 0x4000}, {0x100000, 0x100000, -0x4000},
        {0, 0, 16384}, 1, 0, xy_face, 0, 0);
    require(point.has_value(), "Z face crossing rejected");
    require((*point)[0] == 0x100000 && (*point)[1] == 0x100000 && (*point)[2] == 0, "Z face point differs");
}

void box_tests() {
    struct Case { std::array<std::int32_t, 3> p1, p2; std::array<std::int32_t, 6> bounds; std::int32_t tolerance; bool overlap; };
    const Case cases[] = {
        {{0, 0, 0}, {0x1000, 0x1000, 0x1000}, {0, 0x2000, 0, 0x2000, 0, 0x2000}, 0x100, true},
        {{0, 0, 0}, {0x1000, 0x1000, 0x1000}, {0x3000, 0x4000, 0, 0x2000, 0, 0x2000}, 0, false},
        {{0, 0, 0}, {0x1000, 0x1000, 0x1000}, {0, 0x800, 0, 0x2000, 0, 0x2000}, 0x100, true},
        {{-0x100000, -0x100000, -0x100000}, {-0x100000, -0x100000, -0x100000},
         {-0x1000, 0x1000, -0x1000, 0x1000, -0x1000, 0x1000}, 0, false},
    };
    for (const auto& item : cases) {
        require(dfre::collision_segment_box(item.p1, item.p2, item.bounds, item.tolerance) == item.overlap,
            "Segment box overlap differs");
    }
}

dfre::ModelLod synthetic_lod() {
    dfre::ModelLod lod;
    lod.flags = 0;
    lod.vertices = {{{0, 0, 0}, 0}, {{0x4000, 0, 0}, 0}, {{0, 0x4000, 0}, 0}};
    lod.normals = {{{0, 0, 16384}, 1}};
    dfre::ModelFace face{};
    face.vertices = {0, 1, 2};
    face.normals = {0, 0, 0};
    face.material = 0;
    face.flags = 0;
    face.face_normal = 0;
    lod.faces.push_back(face);
    dfre::ModelMaterial material;
    material.flags = 0;
    lod.materials.push_back(material);
    std::array<std::uint8_t, 72> volume{};
    for (std::size_t word = 0; word < 18; ++word) {
        const std::int32_t value = word == 0 ? 1 : (word == 8 ? 0x1000 : (word == 12 ? -0x2000 : 0));
        std::memcpy(volume.data() + word * 4, &value, 4);
    }
    lod.collision_volumes.push_back(volume);
    return lod;
}

void volume_decode() {
    const std::array<std::uint8_t, 72> barrel = {
        0x03, 0, 0, 0, 0, 0, 0, 0, 0xf1, 0xff, 0xff, 0xff, 0xa1, 0xff, 0xff, 0xff,
        0x20, 0xff, 0xff, 0xff, 0xa0, 0xd4, 0x00, 0x00, 0x4b, 0x81, 0x00, 0x00,
        0x4b, 0x81, 0x00, 0x00, 0x4a, 0x81, 0x00, 0x00, 0x23, 0x7b, 0x00, 0x00,
        0xcc, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa6, 0x7e, 0xff, 0xff,
        0x3b, 0x81, 0x00, 0x00, 0x7e, 0x84, 0xff, 0xff, 0xc4, 0x7a, 0x00, 0x00,
        0x20, 0xff, 0xff, 0xff, 0xb9, 0x50, 0x01, 0x00};
    const auto volume = dfre::decode_collision_volume(barrel);
    require(volume.type == 3 && volume.flags == 0, "Volume type or flags differ");
    require(volume.center[0] == -15 && volume.center[1] == -95 && volume.center[2] == -224, "Volume center differs");
    require(volume.radius == 54432 && volume.horizontal_radius == 33099, "Volume radii differ");
    require(volume.horizontal_radius_copy == 33099, "Volume radius copy differs");
    require(volume.half_extent[0] == 33098 && volume.half_extent[1] == 31523 && volume.half_extent[2] == 43212,
        "Volume extents differ");
    require(volume.bounds[0] == -33114 && volume.bounds[1] == 33083 && volume.bounds[2] == -31618 &&
        volume.bounds[3] == 31428 && volume.bounds[4] == -224 && volume.bounds[5] == 86201, "Volume bounds differ");
    const auto lod = synthetic_lod();
    const auto volumes = dfre::decode_collision_volumes(lod);
    require(volumes.size() == 1 && volumes[0].half_extent[0] == 0x1000 && volumes[0].bounds[0] == -0x2000,
        "LOD volume decode differs");
}

std::vector<std::uint8_t> synthetic_model() {
    // Texture count zero puts the first LOD header at 128, as the decoder expects.
    std::vector<std::uint8_t> bytes(128 + 184 + 24 + 8 + 72 + 112 + 72 + 76, 0);
    std::memcpy(bytes.data(), "3DI", 3);
    bytes[3] = 5;
    std::memcpy(bytes.data() + 4, "TEST", 4);
    const auto put = [&](std::size_t at, std::int32_t value) { std::memcpy(bytes.data() + at, &value, 4); };
    put(20, 1);   // LOD count
    put(124, 0);  // texture count
    const std::size_t lod = 128;
    put(lod + 20, 364); // declared data size
    put(lod + 128, 3); // vertices
    put(lod + 136, 1); // normals
    put(lod + 144, 1); // faces
    put(lod + 152, 1); // meshes
    put(lod + 160, 0); // hardpoints
    put(lod + 168, 1); // materials
    put(lod + 176, 1); // collision volumes
    const std::size_t data = lod + 184;
    const std::size_t vertices = data, normals = vertices + 24, faces = normals + 8;
    const std::size_t meshes = faces + 72, volumes = meshes + 112, materials = volumes + 72;
    const std::array<std::int16_t, 9> positions{0, 0, 0, 0x4000, 0, 0, 0, 0x4000, 0};
    std::memcpy(bytes.data() + vertices, positions.data(), positions.size() * 2);
    const std::int16_t normal[4] = {0, 0, 16384, 1};
    std::memcpy(bytes.data() + normals, normal, sizeof(normal));
    const std::int16_t indices[3] = {0, 1, 2};
    std::memcpy(bytes.data() + faces + 0x1c, indices, sizeof(indices));
    put(faces + 0x28, 0x4000);
    put(faces + 0x2c, -0x300000);
    put(faces + 0x30, 0x300000);
    put(faces + 0x34, -0x300000);
    put(faces + 0x38, 0x300000);
    put(faces + 0x3c, -0x300000);
    put(faces + 0x40, 0x300000);
    put(meshes + 4, 3);    // vertex count
    put(meshes + 0xc, 1);  // face count
    put(meshes + 0x14, 1); // normal count
    put(meshes + 0x1c, 1); // volume count
    put(meshes + 0x40, -0x4000);
    put(meshes + 0x44, 0x4000);
    put(meshes + 0x48, -0x4000);
    put(meshes + 0x4c, 0x4000);
    put(meshes + 0x50, -0x4000);
    put(meshes + 0x54, 0x4000);
    put(volumes, 1);
    put(volumes + 32, 0x800);
    put(volumes + 36, -0x800);
    put(materials + 0x10, 0);
    return bytes;
}

void model_decode() {
    const auto bytes = synthetic_model();
    const auto ranges = dfre::collision_mesh_ranges(bytes, 0);
    require(ranges.size() == 1 && ranges[0].start == 0 && ranges[0].count == 1, "Mesh ranges differ");
    const auto boxes = dfre::collision_face_boxes(bytes, 0);
    require(boxes.size() == 1 && boxes[0].plane_constant == 0x4000, "Face plane differs");
    require(boxes[0].bounds[0] == -0x300000 && boxes[0].bounds[5] == 0x300000, "Face bounds differ");
    const auto model = dfre::decode_model(bytes);
    require(model.lods.size() == 1 && model.lods[0].collision_volumes.size() == 1, "Model decode differs");
    rejects([&] { dfre::collision_mesh_ranges(bytes, 1); });
    auto other = bytes;
    other[3] = 4;
    rejects([&] { dfre::collision_mesh_ranges(other, 0); });
    auto broken = bytes;
    const std::int32_t too_many = 2;
    std::memcpy(broken.data() + 416 + 0x1c, &too_many, 4); // mesh volume totals exceed the LOD count
    rejects([&] { dfre::collision_mesh_ranges(broken, 0); });
}

void model_ray() {
    const auto lod = synthetic_lod();
    const auto bytes = synthetic_model();
    dfre::CollisionMeshView view;
    view.bounds = {-0x4000, 0x4000, -0x4000, 0x4000, -0x4000, 0x4000};
    view.faces = &lod.faces;
    auto boxes = dfre::collision_face_boxes(bytes, 0);
    boxes[0].plane_constant = 0; // Move the stored plane onto the stored vertices.
    view.face_boxes = &boxes;
    view.vertices = &lod.vertices;
    view.normals = &lod.normals;
    view.materials = &lod.materials;
    const std::vector<dfre::CollisionMeshView> meshes{view};
    const auto hit = dfre::collision_model_ray(meshes, {0x1000, 0x1000, 0x4000},
        {0x1000, 0x1000, -0x4000}, 0, 0x20000);
    require(hit.has_value() && hit->mesh == 0 && hit->face == 0, "Model ray hit missing");
    require(hit->point[0] == 0x1000 && hit->point[1] == 0x1000 && hit->point[2] == 0,
        "Model hit point differs");
    require(!dfre::collision_model_ray(meshes, {0x900000, 0x900000, 0x4000}, {0x900000, 0x900000, -0x4000}, 0, 0),
        "Far segment accepted");
    auto hidden = lod;
    hidden.materials[0].flags = 0x1000000;
    dfre::CollisionMeshView hidden_view = view;
    hidden_view.materials = &hidden.materials;
    require(!dfre::collision_model_ray({hidden_view}, {0x1000, 0x1000, 0x4000},
        {0x1000, 0x1000, -0x4000}, 0, 0x20000), "Hidden material accepted");
    auto skipped = view;
    skipped.state_flags = 1;
    require(!dfre::collision_model_ray({skipped}, {0x1000, 0x1000, 0x4000},
        {0x1000, 0x1000, -0x4000}, 0, 0x20000), "Skipped mesh accepted");
}

} // namespace

int main() {
    marches();
    face_tests();
    face_ray_tests();
    box_tests();
    volume_decode();
    model_decode();
    model_ray();
    return 0;
}
