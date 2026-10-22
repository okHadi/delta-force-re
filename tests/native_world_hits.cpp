// Native checks for the world hit stage. Goldens come from the ported
// original-code replay in tools/check_world_hits_original.py.
#include "dfre/world_hits.hpp"

#include <array>
#include <iostream>
#include <cstdint>
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
    throw std::runtime_error("Invalid world hit input accepted");
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

dfre::TerrainGeometry terrain() {
    return dfre::TerrainGeometry(make_heights(), make_characters(), make_detail());
}

void math_helpers() {
    require(dfre::collision_sqrt(0) == 0 && dfre::collision_sqrt(1) == 1, "Square root low values differ");
    require(dfre::collision_sqrt(5) == 3 && dfre::collision_sqrt(65535) == 256, "Square root differs");
    require(dfre::collision_sqrt(-5) == -5, "Square root negative value differs");
    require(dfre::collision_approximate_length(3, -4, 5) == 5, "Approximate length differs");
    require(dfre::collision_approximate_length(100, 200, 300) == 393, "Approximate length differs");
    require(dfre::collision_approximate_length(0, 0, 0) == 0, "Zero length differs");
    const dfre::ModelMatrix identity{0x400000, 0, 0, 0x1000, 0, 0x400000, 0, 0x200, 0, 0, 0x400000, 0x300};
    const auto local = dfre::world_hit_local_point(identity, {0x40000, 0x500, 0x600});
    require(local[0] == 0x41000 && local[1] == 0x700 && local[2] == 0x900, "Local point differs");
    const dfre::ModelMatrix rotated{0, -0x400000, 0, 0x1000, 0x400000, 0, 0, 0x200, 0, 0, 0x400000, 0x300};
    const auto rotated_point = dfre::world_hit_local_point(rotated, {0x40000, 0x500, 0x600});
    require(rotated_point[0] == -0x700 && rotated_point[1] == 0x41000 && rotated_point[2] == 0x900,
        "Rotated local point uses the translation of each input axis");
    const auto direction = dfre::world_hit_local_direction(identity, {0x10000, 0x2000, 0x3000});
    require(direction[0] == 0x10000 && direction[1] == 0x2000 && direction[2] == 0x3000,
        "Local direction differs");
}

// One crafted record whose identity matrix maps the query ray onto a face
// crossing, matching the replay's crafted geometry.
struct CraftedRecord {
    std::vector<dfre::ModelFace> faces;
    std::vector<dfre::CollisionFaceBox> boxes;
    std::vector<dfre::ModelVertex> vertices;
    std::vector<dfre::ModelNormal> normals;
    std::vector<dfre::ModelMaterial> materials;
    dfre::WorldHitMesh mesh;
    dfre::WorldHitRecord record;
};

void crafted(CraftedRecord& fixture, std::int32_t material_flags, std::uint32_t identity,
    const std::array<std::int32_t, 3>& origin, const std::array<std::int32_t, 3>& translation) {
    dfre::ModelFace face{};
    face.vertices = {0, 1, 2};
    face.material = 0;
    face.face_normal = 0;
    fixture.faces.push_back(face);
    dfre::CollisionFaceBox box;
    box.plane_constant = 0x5000;
    box.bounds = {-0xF000, 0x19000, 0x200 - 0x10000, 0x200 + 0x10000, 0x300 - 0x10000, 0x300 + 0x10000};
    fixture.boxes.push_back(box);
    for (const auto& position : {std::array<std::int16_t, 3>{0, -598, -597}, std::array<std::int16_t, 3>{0, 602, -597},
                                 std::array<std::int16_t, 3>{0, 2, 603}}) {
        fixture.vertices.push_back(dfre::ModelVertex{position, 0});
    }
    fixture.normals.push_back(dfre::ModelNormal{{-16384, 0, 0}, 4});
    dfre::ModelMaterial material;
    material.flags = material_flags;
    fixture.materials.push_back(material);
    fixture.mesh.matrix = {0x400000, 0, 0, translation[0], 0, 0x400000, 0, translation[1], 0, 0, 0x400000,
                           translation[2]};
    fixture.mesh.faces = &fixture.faces;
    fixture.mesh.face_boxes = &fixture.boxes;
    fixture.mesh.vertices = &fixture.vertices;
    fixture.mesh.normals = &fixture.normals;
    fixture.mesh.materials = &fixture.materials;
    fixture.record.active = 1;
    fixture.record.identity = identity;
    fixture.record.position = origin;
    fixture.record.radius = 0x40000;
    fixture.record.meshes.push_back(fixture.mesh);
}

dfre::WorldHitQuery crafted_query() {
    dfre::WorldHitQuery query;
    query.origin = {0, 0, 0};
    query.end = {0x8000, 0, 0};
    query.direction = {0x10000, 0, 0};
    query.length = 0x8000;
    query.bounds = {0, 0x8000, 0, 0, 0, 0};
    query.skip = {0, 0};
    return query;
}

void model_list_hits() {
    const std::array<std::int32_t, 3> translation{0x1000, 0x200, 0x300};
    dfre::WorldModelHit hit;
    {
        CraftedRecord fixture;
        crafted(fixture, 0, 0x11, {0, 0, 0}, translation);
        fixture.record.position={0,0x20000,0x20000};fixture.record.radius=0x20000;
        unsigned callbacks=0;
        fixture.record.mesh_source=[&]{++callbacks;return fixture.record.meshes;};
        require(dfre::world_model_list_hit({fixture.record},crafted_query(),hit)==1 && !hit.hit,
            "Sphere rejection uses absolute distance components");
        require(callbacks==0,"Rejected sphere ran the model callback");
        fixture.record.position={0,0,0};
        require(dfre::world_model_list_hit({fixture.record},crafted_query(),hit)==0 && callbacks==1,
            "Accepted sphere runs the model callback at the query site");
    }
    {
        CraftedRecord fixture;
        crafted(fixture, 0, 0x11, {0, 0, 0}, translation);
        const std::vector<dfre::WorldHitRecord> records{fixture.record};
        require(dfre::world_model_list_hit(records, crafted_query(), hit) == 0, "List hit missing");
        require(hit.hit && hit.record == 0 && hit.mesh == 0 && hit.face == 0 && hit.distance == 0x4000,
            "List hit fields differ");
        require(hit.material_flags == 0 && !hit.divided, "List hit material differs");
    }
    {
        CraftedRecord fixture;
        crafted(fixture, 0x868C, 0x11, {0, 0, 0}, translation);
        const std::vector<dfre::WorldHitRecord> records{fixture.record};
        require(dfre::world_model_list_hit(records, crafted_query(), hit) == 2 && !hit.hit && hit.masked == 1,
            "Masked list hit differs");
    }
    {
        CraftedRecord fixture;
        crafted(fixture, 2, 0x11, {0, 0, 0}, translation);
        const std::vector<dfre::WorldHitRecord> records{fixture.record};
        require(dfre::world_model_list_hit(records, crafted_query(), hit) == 0 && hit.material_flags == 2,
            "Two-sided list hit differs");
    }
    {
        CraftedRecord fixture;
        crafted(fixture, 0, 0x11, {0, 0, 0}, translation);
        const std::vector<dfre::WorldHitRecord> records{fixture.record};
        auto query = crafted_query();
        query.skip = {0x11, 0};
        require(dfre::world_model_list_hit(records, query, hit) == 1 && !hit.hit, "Skipped record accepted");
    }
    {
        CraftedRecord fixture;
        crafted(fixture, 0, 0x11, {0, 0, 0}, translation);
        fixture.record.meshes[0].state_flags = 1;
        const std::vector<dfre::WorldHitRecord> records{fixture.record};
        require(dfre::world_model_list_hit(records, crafted_query(), hit) == 1 && !hit.hit, "Skipped mesh accepted");
    }
    {
        CraftedRecord fixture;
        crafted(fixture, 0, 0x11, {0, 0, 0}, translation);
        fixture.record.active = 0;
        const std::vector<dfre::WorldHitRecord> records{fixture.record};
        require(dfre::world_model_list_hit(records, crafted_query(), hit) == 1 && !hit.hit, "Inactive record accepted");
    }
    {
        CraftedRecord fixture;
        crafted(fixture, 0, 0x11, {0, 0, 0}, translation);
        fixture.record.meshes[0].faces = nullptr;
        const std::vector<dfre::WorldHitRecord> records{fixture.record};
        rejects([&] { dfre::world_model_list_hit(records, crafted_query(), hit); });
    }
}

void segment_queries() {
    const auto geometry = terrain();
    const auto bounds = make_max_height();
    const std::vector<dfre::WorldHitRecord> empty;
    auto result = dfre::world_segment_query(geometry, bounds, 0, empty, empty, 0, 0,
        {0x10000, 0x20000, 0x300000}, {0x10000, 0x20000, 0x300000});
    require(result.code == 1 && result.kind == dfre::WorldSegmentKind::zero_length, "Zero length query differs");
    result = dfre::world_segment_query(geometry, bounds, 0, empty, empty, 0, 0,
        {0x10000, 0x20000, 0x3000000}, {0x2000000, 0x20000, 0x3000000});
    require(result.code == 1 && result.kind == dfre::WorldSegmentKind::cleared, "Cleared query differs");
    // A segment that reaches the water level reports the layer through the code.
    const auto water = dfre::world_segment_query(geometry, bounds, 50331000, empty, empty, 0, 0,
        {0x10000, 0x20000, 0x3000000}, {0x2000000, 0x20000, 45331648});
    require(water.code == 2 && water.kind == dfre::WorldSegmentKind::cleared && water.water_layer,
        "Water layer query differs");
    // Terrain hit from the original detail march (negative sample index).
    result = dfre::world_segment_query(geometry, bounds, 0, empty, empty, 0, 0,
        {0x200000, 0x300000, 0x10000}, {0x204000, 0x300000, 0x10000});
    require(result.code == 61440 && result.kind == dfre::WorldSegmentKind::terrain, "Terrain query differs");
    // Stored face hit above the terrain, then the same record in the second list.
    const std::array<std::int32_t, 3> start{0, 0, 0x3000000};
    const std::array<std::int32_t, 3> translation{0x1000, 0x200, 0x300 - 0x3000000};
    CraftedRecord fixture;
    crafted(fixture, 0, 0x11, start, translation);
    const std::vector<dfre::WorldHitRecord> records{fixture.record};
    result = dfre::world_segment_query(geometry, bounds, 0, records, empty, 0, 0, start,
        {start[0] + 0x8000, start[1], start[2]});
    require(result.code == -0x4000 && result.kind == dfre::WorldSegmentKind::surface && result.slot == 0,
        "Surface query differs");
    require(result.distance == 0x4000 && result.record == 0 && result.mesh == 0 && result.face == 0,
        "Surface query fields differ");
    result = dfre::world_segment_query(geometry, bounds, 0, empty, records, 0, 0, start,
        {start[0] + 0x8000, start[1], start[2]});
    require(result.code == -0x4000 && result.slot == 1, "Second list query differs");
    // Lengths stay multiples of 0x800, so a one-unit delta collapses to zero
    // and keeps the original zero-length code.
    result = dfre::world_segment_query(geometry, bounds, 0, empty, empty, 0, 0, {0, 0, 0x3000000},
        {1, 0, 0x3000000});
    require(result.code == 1 && result.kind == dfre::WorldSegmentKind::zero_length, "Tiny delta query differs");
}

} // namespace

int main() {
    try {
    math_helpers();
    model_list_hits();
    segment_queries();
    return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
