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
    std::uint8_t texture_key{}, frame_count{}, animation_parameter{};
    std::array<std::uint8_t, 3> flat_rgb{};
    std::vector<std::size_t> texture_candidates;
    std::array<std::uint8_t, 76> raw{};
};
struct ModelLod {
    std::uint32_t flags{};
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
// Apply the original v5 loader origin step to one decoded LOD. Returns a copy.
ModelLod localize_model_lod(const ModelLod& lod);
struct CharacterTrack {
    std::uint32_t offset{}, count{}, stride{};
};
struct CharacterBone {
    std::uint32_t offset{}, flags{};
    std::array<CharacterTrack, 3> tracks;
    std::vector<std::uint8_t> raw;
};
struct CharacterFrame {
    std::uint32_t offset{}, next_stride{};
    std::array<std::uint8_t, 52> raw_header{};
    std::vector<std::array<std::uint8_t, 4>> packed_angles;
};
struct CharacterAnimation {
    std::uint32_t slot{}, offset{};
    std::array<std::uint8_t, 20> raw_header{};
    std::vector<CharacterFrame> frames;
};
struct CharacterModel {
    std::vector<std::uint8_t> raw;
    bool runtime_prepared{};
    std::vector<CharacterBone> bones;
    std::vector<std::uint32_t> bone_map;
    std::uint32_t declared_animation_count{};
    std::vector<CharacterAnimation> animations;
};
struct CharacterBonePose {
    std::array<std::int32_t, 3> angles; // Original angle units: unsigned stored byte << 17.
    std::array<std::int32_t, 3> scale; // Original 16.16 units.
};
// Reads the observed DF1 CHR1 layout. Track and frame flags stay raw.
CharacterModel decode_character(const std::vector<std::uint8_t>& bytes);
// Copy and prepare character fields using original 0x42c9a0; source stays unchanged.
CharacterModel prepare_character_runtime(const CharacterModel& source);
// Original 0x43a0e0 pose read; motion and interpolation are separate steps.
std::vector<CharacterBonePose> character_frame_pose(const CharacterFrame& frame);
// Core original per-bone deltas. Target pose includes any frame scale values.
std::vector<CharacterBonePose> character_pose_delta(const std::vector<CharacterBonePose>& current,
    const std::vector<CharacterBonePose>& target, std::uint16_t ticks);
std::vector<CharacterBonePose> advance_character_pose(const std::vector<CharacterBonePose>& current,
    const std::vector<CharacterBonePose>& delta);


using ModelMatrix=std::array<std::int32_t,12>; // Row-major 3x4; rotation uses 22 fractional bits.
// Original unsigned full-turn angle; returns sine and cosine in 22-bit fixed units.
std::array<std::int32_t,2> fixed_sine_cosine(std::uint32_t angle);
ModelMatrix model_identity_matrix();
ModelMatrix model_multiply_matrix(const ModelMatrix& left,const ModelMatrix& right);
std::array<std::int32_t,3> model_transform_point(const ModelMatrix& matrix,
    const std::array<std::int32_t,3>& point);
ModelMatrix model_local_matrix(const std::array<std::int32_t,3>& translation,
    const std::array<std::uint32_t,3>& angles);
std::vector<ModelMatrix> character_bone_matrices(const CharacterModel& prepared,
    const std::vector<CharacterBonePose>& pose,std::uint32_t heading_adjustment=0,
    const ModelMatrix* world=nullptr);

}
