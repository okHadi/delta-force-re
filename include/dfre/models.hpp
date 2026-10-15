#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dfre {
struct ModelTexture {
    std::string name;
    std::uint16_t width{}, height{}, sequence{};
    std::uint8_t flags{};
    std::vector<std::uint8_t> pixels;
    std::array<std::uint8_t, 1024> palette_rgb_pad{};
};
struct ModelVertex { std::array<std::int16_t, 3> position; std::uint16_t mesh; };
struct ModelNormal { std::array<std::int16_t, 3> direction; std::uint16_t flags; };
struct ModelFace {
    std::array<std::uint32_t, 3> vertices, normals;
    std::array<std::int32_t, 3> u, v; // Raw 16.16 texture coordinates.
    std::uint32_t material;
    std::uint16_t flags, face_normal;
};
struct ModelMesh {
    std::uint32_t vertex_start{}, vertex_count{}, normal_start{}, normal_count{}, face_start{}, face_count{};
    bool has_v5_transform{};
    std::uint32_t parent_index{}; // Raw value; root conventions need runtime checks.
    std::array<std::int32_t, 3> origin{}, delta_origin{}; // Raw 24.8 values.
    std::vector<std::uint8_t> raw; // Animation fields remain undecoded.
};
struct ModelMaterial {
    std::string name;
    std::uint32_t flags{};
    std::uint8_t texture_key{}, frame_count{}, timer{};
    std::array<std::uint8_t, 3> flat_rgb{};
    std::vector<std::size_t> texture_candidates;
    std::array<std::uint8_t, 76> raw{};
};
struct ModelLod {
    std::vector<ModelVertex> vertices;
    std::vector<ModelNormal> normals;
    std::vector<ModelFace> faces;
    std::vector<ModelMesh> meshes;
    std::vector<std::array<std::int32_t, 3>> hardpoints; // Raw 24.8 positions.
    std::vector<std::array<std::uint8_t, 72>> collision_volumes;
    std::vector<ModelMaterial> materials;
    std::vector<std::uint8_t> raw_header;
};
struct Model {
    std::string name;
    std::uint8_t version{};
    std::vector<ModelTexture> textures;
    std::vector<ModelLod> lods;
};
// Throws std::invalid_argument for unsupported or invalid model data.
Model decode_model(const std::vector<std::uint8_t>& bytes);
}
