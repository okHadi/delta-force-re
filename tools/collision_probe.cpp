// Collision probe: decodes stored collision data and runs the ported ray stage.
// Reads one command per line from standard input; used by native tests and by
// tools/check_collision_original.py for the original x86 replay.
#include "dfre/collision.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::invalid_argument("Cannot open " + path);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

// Deterministic terrain used by the original-code replay. The Python checker
// generates the same bytes from the same formulas.
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

struct Terrain {
    dfre::TerrainGeometry geometry = [] {
        return dfre::TerrainGeometry(make_heights(), make_characters(), make_detail());
    }();
    dfre::AssetBytes max_height = make_max_height();
};

void decode_volumes(const std::string& path) {
    const auto bytes = read_file(path);
    const auto model = dfre::decode_model(bytes);
    for (std::size_t lod = 0; lod < model.lods.size(); ++lod) {
        for (const auto& range : dfre::collision_mesh_ranges(bytes, lod)) {
            std::cout << "mesh " << lod << ' ' << range.start << ' ' << range.count << '\n';
        }
        for (const auto& volume : dfre::decode_collision_volumes(model.lods[lod])) {
            std::cout << "volume " << lod << ' ' << volume.type << ' ' << volume.flags;
            for (auto value : volume.center) std::cout << ' ' << value;
            std::cout << ' ' << volume.radius << ' ' << volume.horizontal_radius << ' ' << volume.horizontal_radius_copy;
            for (auto value : volume.half_extent) std::cout << ' ' << value;
            for (auto value : volume.bounds) std::cout << ' ' << value;
            std::cout << '\n';
        }
    }
}

void decode_faces(const std::string& path) {
    const auto bytes = read_file(path);
    const auto model = dfre::decode_model(bytes);
    for (std::size_t lod = 0; lod < model.lods.size(); ++lod) {
        for (const auto& box : dfre::collision_face_boxes(bytes, lod)) {
            std::cout << "face " << lod << ' ' << box.plane_constant;
            for (auto value : box.bounds) std::cout << ' ' << value;
            std::cout << '\n';
        }
    }
}

void march_lines(Terrain& terrain, bool detail) {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::int64_t limit = 0;
        dfre::CollisionSegment ray;
        if (!(input >> limit >> ray.start[0] >> ray.start[1] >> ray.start[2]
                >> ray.delta[0] >> ray.delta[1] >> ray.delta[2])) {
            throw std::invalid_argument("Expected limit, start, and delta per line");
        }
        if (limit < std::numeric_limits<std::int32_t>::min() || limit > std::numeric_limits<std::int32_t>::max()) {
            throw std::invalid_argument("Segment limit outside signed 32-bit range");
        }
        ray.limit = static_cast<std::int32_t>(limit);
        const auto result = detail ? dfre::terrain_ray_march_detail(terrain.geometry, terrain.max_height, ray)
                                   : dfre::terrain_ray_march(terrain.geometry, ray);
        std::cout << (result.hit ? 1 : 0) << ' ' << result.lambda << '\n';
    }
}

void point_lines() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::array<std::int32_t, 3> point{};
        std::array<std::array<std::int16_t, 3>, 3> vertices{};
        std::int64_t flags = 0;
        if (!(input >> flags >> point[0] >> point[1] >> point[2])) {
            throw std::invalid_argument("Expected flags and a point per line");
        }
        for (auto& vertex : vertices) {
            for (auto& axis : vertex) {
                std::int64_t value = 0;
                if (!(input >> value)) throw std::invalid_argument("Expected three vertices per line");
                if (value < -32768 || value > 32767) throw std::invalid_argument("Vertex outside 16-bit range");
                axis = static_cast<std::int16_t>(value);
            }
        }
        std::cout << (dfre::collision_point_in_face(point, vertices, static_cast<std::uint16_t>(flags)) ? 1 : 0) << '\n';
    }
}

void face_ray_lines() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::array<std::int32_t, 3> p1{}, p2{};
        std::array<std::int16_t, 3> normal{};
        std::array<std::array<std::int16_t, 3>, 3> vertices{};
        std::int64_t flags = 0, plane = 0, threshold = 0, material = 0;
        if (!(input >> p1[0] >> p1[1] >> p1[2] >> p2[0] >> p2[1] >> p2[2]
                >> normal[0] >> normal[1] >> normal[2] >> flags >> plane >> threshold >> material)) {
            throw std::invalid_argument("Expected segment, normal, and face values per line");
        }
        for (auto& vertex : vertices) {
            for (auto& axis : vertex) {
                std::int64_t value = 0;
                if (!(input >> value)) throw std::invalid_argument("Expected three vertices per line");
                if (value < -32768 || value > 32767) throw std::invalid_argument("Vertex outside 16-bit range");
                axis = static_cast<std::int16_t>(value);
            }
        }
        const auto point = dfre::collision_face_ray(p1, p2, normal, static_cast<std::uint16_t>(flags),
            static_cast<std::int32_t>(plane), vertices, static_cast<std::int32_t>(threshold),
            static_cast<std::uint32_t>(material));
        if (!point) {
            std::cout << "0\n";
        } else {
            std::cout << "1 " << (*point)[0] << ' ' << (*point)[1] << ' ' << (*point)[2] << '\n';
        }
    }
}

void box_lines() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::array<std::int32_t, 3> p1{}, p2{};
        std::array<std::int32_t, 6> bounds{};
        std::int64_t tolerance = 0;
        if (!(input >> p1[0] >> p1[1] >> p1[2] >> p2[0] >> p2[1] >> p2[2] >> tolerance)) {
            throw std::invalid_argument("Expected two points and a tolerance per line");
        }
        for (auto& bound : bounds) {
            if (!(input >> bound)) throw std::invalid_argument("Expected six bounds per line");
        }
        std::cout << (dfre::collision_segment_box(p1, p2, bounds, static_cast<std::int32_t>(tolerance)) ? 1 : 0) << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            std::cerr << "usage: collision_probe COMMAND [FILE] < input\n";
            return 1;
        }
        const std::string command = argv[1];
        if (command == "volumes") {
            if (argc != 3) throw std::invalid_argument("volumes needs a model path");
            decode_volumes(argv[2]);
        } else if (command == "faces") {
            if (argc != 3) throw std::invalid_argument("faces needs a model path");
            decode_faces(argv[2]);
        } else if (command == "march" || command == "march-detail") {
            Terrain terrain;
            march_lines(terrain, command == "march-detail");
        } else if (command == "point") {
            point_lines();
        } else if (command == "faceray") {
            face_ray_lines();
        } else if (command == "box") {
            box_lines();
        } else {
            std::cerr << "unknown command " << command << '\n';
            return 1;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "collision_probe: " << error.what() << '\n';
        return 1;
    }
}
