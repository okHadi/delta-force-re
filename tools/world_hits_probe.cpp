// World hit probe: runs the ported 0x42E2B0 / 0x42DB80 stage on a described
// scenario. One scenario per header line; record, mesh, vertex, and face
// lines follow until the next header or end of input. Used by the native
// tests and by tools/check_world_hits_original.py for the original x86 replay.
#include "dfre/world_hits.hpp"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using dfre::AssetBytes;
using dfre::ModelFace;
using dfre::ModelMaterial;
using dfre::ModelNormal;
using dfre::ModelVertex;
using dfre::WorldHitMesh;
using dfre::WorldHitRecord;

constexpr std::uint32_t map_pixels = 1024 * 1024;

AssetBytes make_heights() {
    AssetBytes heights(map_pixels);
    for (std::uint32_t y = 0; y < 1024; ++y) for (std::uint32_t x = 0; x < 1024; ++x) {
        heights[y * 1024 + x] = static_cast<std::uint8_t>(x * 13 + y * 7 + (x ^ y));
    }
    return heights;
}

AssetBytes make_characters() {
    AssetBytes characters(map_pixels);
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

AssetBytes make_max_height() {
    AssetBytes bounds(map_pixels);
    for (std::uint32_t y = 0; y < 1024; ++y) for (std::uint32_t x = 0; x < 1024; ++x) {
        bounds[y * 1024 + x] = static_cast<std::uint8_t>(x * 31 + y * 19 + ((x * y) & 255));
    }
    return bounds;
}

struct ScenarioMesh {
    WorldHitMesh view;
    std::vector<ModelFace> faces;
    std::vector<dfre::CollisionFaceBox> boxes;
    std::vector<ModelVertex> vertices;
    std::vector<ModelNormal> normals;
    std::vector<ModelMaterial> materials;
};

struct ScenarioRecord {
    WorldHitRecord record;
    std::vector<ScenarioMesh> meshes;
};

struct Scenario {
    bool segment = false;
    std::array<std::int32_t, 3> origin{}, end{}, direction{};
    std::array<std::int32_t, 6> bounds{};
    std::int32_t length = 0;
    std::array<std::uint32_t, 2> skip{};
    std::int32_t water = 0;
    std::array<std::vector<ScenarioRecord>, 2> lists;
};

std::int64_t read_int(std::istringstream& input, const char* message) {
    std::int64_t value = 0;
    if (!(input >> value)) throw std::invalid_argument(message);
    return value;
}

std::int32_t to_int(std::int64_t value, const char* message) {
    if (value < INT32_MIN || value > INT32_MAX) throw std::invalid_argument(message);
    return static_cast<std::int32_t>(value);
}

std::int16_t to_short(std::int64_t value, const char* message) {
    if (value < INT16_MIN || value > INT16_MAX) throw std::invalid_argument(message);
    return static_cast<std::int16_t>(value);
}

Scenario parse_header(const std::string& keyword, std::istringstream& input) {
    Scenario scenario;
    if (keyword == "segment") {
        scenario.segment = true;
        scenario.water = to_int(read_int(input, "Expected water height"), "Water height outside 32 bits");
        for (auto* value : {&scenario.origin[0], &scenario.origin[1], &scenario.origin[2],
                            &scenario.end[0], &scenario.end[1], &scenario.end[2]}) {
            *value = to_int(read_int(input, "Expected start and end"), "Point outside 32 bits");
        }
        scenario.skip[0] = static_cast<std::uint32_t>(read_int(input, "Expected skip identities"));
        scenario.skip[1] = static_cast<std::uint32_t>(read_int(input, "Expected skip identities"));
        return scenario;
    }
    for (auto* value : {&scenario.origin[0], &scenario.origin[1], &scenario.origin[2],
                        &scenario.end[0], &scenario.end[1], &scenario.end[2],
                        &scenario.direction[0], &scenario.direction[1], &scenario.direction[2]}) {
        *value = to_int(read_int(input, "Expected query points"), "Query value outside 32 bits");
    }
    scenario.length = to_int(read_int(input, "Expected length"), "Length outside 32 bits");
    for (auto& bound : scenario.bounds) {
        bound = to_int(read_int(input, "Expected bounds"), "Bound outside 32 bits");
    }
    scenario.skip[0] = static_cast<std::uint32_t>(read_int(input, "Expected skip identities"));
    scenario.skip[1] = static_cast<std::uint32_t>(read_int(input, "Expected skip identities"));
    return scenario;
}

void parse_record(Scenario& scenario, std::istringstream& input) {
    const auto list = read_int(input, "Expected list index");
    if (list < 0 || list > 1 || (!scenario.segment && list != 0)) throw std::invalid_argument("Bad list index");
    auto& records = scenario.lists[static_cast<std::size_t>(list)];
    records.emplace_back();
    auto& record = records.back().record;
    record.active = static_cast<std::uint32_t>(read_int(input, "Expected active flag"));
    record.identity = static_cast<std::uint32_t>(read_int(input, "Expected identity"));
    for (auto& value : record.position) {
        value = to_int(read_int(input, "Expected record position"), "Position outside 32 bits");
    }
    record.radius = to_int(read_int(input, "Expected radius"), "Radius outside 32 bits");
    static_cast<void>(read_int(input, "Expected mesh count"));
}

void parse_mesh(Scenario& scenario, std::istringstream& input) {
    const auto list = read_int(input, "Expected list index");
    const auto record_index = read_int(input, "Expected record index");
    if (list < 0 || list > 1 || (!scenario.segment && list != 0)) throw std::invalid_argument("Bad list index");
    if (record_index < 0) throw std::invalid_argument("Negative record index");
    auto& records = scenario.lists[static_cast<std::size_t>(list)];
    if (static_cast<std::size_t>(record_index) >= records.size()) throw std::invalid_argument("Mesh before its record");
    auto& meshes = records[static_cast<std::size_t>(record_index)].meshes;
    meshes.emplace_back();
    auto& mesh = meshes.back();
    mesh.view.state_flags = static_cast<std::uint8_t>(read_int(input, "Expected state flags"));
    for (auto& value : mesh.view.matrix) {
        value = to_int(read_int(input, "Expected matrix"), "Matrix value outside 32 bits");
    }
    mesh.view.normal_start = static_cast<std::uint32_t>(read_int(input, "Expected normal start"));
    static_cast<void>(read_int(input, "Expected vertex count"));
}

void parse_vertex(Scenario& scenario, std::istringstream& input) {
    const auto list = read_int(input, "Expected list index");
    const auto record_index = read_int(input, "Expected record index");
    const auto mesh_index = read_int(input, "Expected mesh index");
    if (list < 0 || list > 1 || (!scenario.segment && list != 0)) throw std::invalid_argument("Bad list index");
    if (record_index < 0 || mesh_index < 0) throw std::invalid_argument("Negative index");
    auto& records = scenario.lists[static_cast<std::size_t>(list)];
    if (static_cast<std::size_t>(record_index) >= records.size()) throw std::invalid_argument("Vertex before its record");
    auto& meshes = records[static_cast<std::size_t>(record_index)].meshes;
    if (static_cast<std::size_t>(mesh_index) >= meshes.size()) throw std::invalid_argument("Vertex before its mesh");
    auto& mesh = meshes[static_cast<std::size_t>(mesh_index)];
    ModelVertex vertex{};
    for (auto& value : vertex.position) {
        value = to_short(read_int(input, "Expected vertex position"), "Vertex outside 16 bits");
    }
    mesh.vertices.push_back(vertex);
}

void parse_face(Scenario& scenario, std::istringstream& input) {
    const auto list = read_int(input, "Expected list index");
    const auto record_index = read_int(input, "Expected record index");
    const auto mesh_index = read_int(input, "Expected mesh index");
    if (list < 0 || list > 1 || (!scenario.segment && list != 0)) throw std::invalid_argument("Bad list index");
    if (record_index < 0 || mesh_index < 0) throw std::invalid_argument("Negative index");
    auto& records = scenario.lists[static_cast<std::size_t>(list)];
    if (static_cast<std::size_t>(record_index) >= records.size()) throw std::invalid_argument("Face before its record");
    auto& meshes = records[static_cast<std::size_t>(record_index)].meshes;
    if (static_cast<std::size_t>(mesh_index) >= meshes.size()) throw std::invalid_argument("Face before its mesh");
    auto& mesh = meshes[static_cast<std::size_t>(mesh_index)];
    const auto material_flags = static_cast<std::uint32_t>(read_int(input, "Expected material flags"));
    dfre::CollisionFaceBox box;
    box.plane_constant = to_int(read_int(input, "Expected plane"), "Plane outside 32 bits");
    for (auto& bound : box.bounds) {
        bound = to_int(read_int(input, "Expected bounds"), "Bound outside 32 bits");
    }
    ModelFace face{};
    for (auto& index : face.vertices) {
        const auto value = read_int(input, "Expected vertex indices");
        if (value < 0 || value > 65535) throw std::invalid_argument("Vertex index outside 16 bits");
        index = static_cast<std::uint32_t>(value);
    }
    const auto index = static_cast<std::uint32_t>(mesh.faces.size());
    face.material = index;
    face.face_normal = static_cast<std::uint16_t>(index);
    ModelNormal normal{};
    for (auto& value : normal.direction) {
        value = to_short(read_int(input, "Expected normal"), "Normal outside 16 bits");
    }
    normal.flags = static_cast<std::uint16_t>(read_int(input, "Expected normal flags"));
    ModelMaterial material;
    material.flags = material_flags;
    mesh.faces.push_back(face);
    mesh.boxes.push_back(box);
    mesh.normals.push_back(normal);
    mesh.materials.push_back(material);
}

std::vector<Scenario> parse_scenarios() {
    std::vector<Scenario> scenarios;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::istringstream input(line);
        std::string keyword;
        input >> keyword;
        if (keyword == "segment" || keyword == "modelhit") {
            scenarios.push_back(parse_header(keyword, input));
            continue;
        }
        if (scenarios.empty()) throw std::invalid_argument("Data line before a scenario header");
        auto& scenario = scenarios.back();
        if (keyword == "record") parse_record(scenario, input);
        else if (keyword == "mesh") parse_mesh(scenario, input);
        else if (keyword == "vertex") parse_vertex(scenario, input);
        else if (keyword == "face") parse_face(scenario, input);
        else throw std::invalid_argument("Unknown scenario keyword " + keyword);
    }
    return scenarios;
}

std::vector<WorldHitRecord> flatten(const Scenario& scenario, std::size_t list) {
    std::vector<WorldHitRecord> records;
    for (const auto& entry : scenario.lists[list]) {
        WorldHitRecord record = entry.record;
        record.meshes.clear();
        for (const auto& mesh : entry.meshes) {
            WorldHitMesh view = mesh.view;
            view.faces = &mesh.faces;
            view.face_boxes = &mesh.boxes;
            view.vertices = &mesh.vertices;
            view.normals = &mesh.normals;
            view.materials = &mesh.materials;
            record.meshes.push_back(view);
        }
        records.push_back(record);
    }
    return records;
}

const char* kind_name(dfre::WorldSegmentKind kind) {
    switch (kind) {
        case dfre::WorldSegmentKind::zero_length: return "zero_length";
        case dfre::WorldSegmentKind::rejected: return "rejected";
        case dfre::WorldSegmentKind::cleared: return "cleared";
        case dfre::WorldSegmentKind::terrain: return "terrain";
        case dfre::WorldSegmentKind::surface: return "surface";
    }
    return "unknown";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "usage: world_hits_probe COMMAND < input\n";
            return 1;
        }
        const std::string command = argv[1];
        if (command == "sqrt") {
            std::string line;
            while (std::getline(std::cin, line)) {
                std::istringstream input(line);
                const auto value = to_int(read_int(input, "Expected one value"), "Value outside 32 bits");
                std::cout << dfre::collision_sqrt(value) << '\n';
            }
        } else if (command == "length") {
            std::string line;
            while (std::getline(std::cin, line)) {
                std::istringstream input(line);
                const auto first = to_int(read_int(input, "Expected three values"), "Value outside 32 bits");
                const auto second = to_int(read_int(input, "Expected three values"), "Value outside 32 bits");
                const auto third = to_int(read_int(input, "Expected three values"), "Value outside 32 bits");
                std::cout << dfre::collision_approximate_length(first, second, third) << '\n';
            }
        } else if (command == "scenario") {
            const auto scenarios = parse_scenarios();
            for (const auto& scenario : scenarios) {
                if (!scenario.segment) {
                    dfre::WorldHitQuery query;
                    query.origin = scenario.origin;
                    query.end = scenario.end;
                    query.direction = scenario.direction;
                    query.length = scenario.length;
                    query.bounds = scenario.bounds;
                    query.skip = scenario.skip;
                    const auto records = flatten(scenario, 0);
                    dfre::WorldModelHit hit;
                    const auto count = dfre::world_model_list_hit(records, query, hit);
                    std::cout << count << ' ' << (hit.hit ? 1 : 0) << ' ' << (hit.divided ? 1 : 0) << ' '
                              << hit.record << ' ' << hit.mesh << ' ' << hit.face << ' ' << hit.material_flags
                              << ' ' << hit.distance << ' ' << hit.masked << '\n';
                } else {
                    const dfre::TerrainGeometry geometry(make_heights(), make_characters(), make_detail());
                    const auto bounds = make_max_height();
                    const auto first = flatten(scenario, 0);
                    const auto second = flatten(scenario, 1);
                    const auto result = dfre::world_segment_query(geometry, bounds, scenario.water,
                        first, second, scenario.skip[0], scenario.skip[1], scenario.origin, scenario.end);
                    std::cout << result.code << ' ' << kind_name(result.kind) << ' ' << result.distance << ' '
                              << result.slot << ' ' << result.record << ' ' << result.mesh << ' ' << result.face
                              << ' ' << result.material_flags << ' ' << result.masked << ' '
                              << (result.water_layer ? 1 : 0) << ' ' << (result.divided ? 1 : 0) << '\n';
                }
            }
        } else {
            std::cerr << "unknown command " << command << '\n';
            return 1;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "world_hits_probe: " << error.what() << '\n';
        return 1;
    }
}
