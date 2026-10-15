#pragma once
#include "dfre/model_visibility.hpp"
#include "dfre/models.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// Original mesh-instance matrices for loaded world placements.
// Targets Df.exe sha256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
//
// Evidence order (all original x86 entries, replay-checked in
// tools/check_model_instances_original.py):
//   0x45d1e0 builds one local 3x4 matrix from an object block at +0x24:
//            translation +0x24/+0x28/+0x2c, full-turn angles +0x30 (yaw),
//            +0x34 (pitch), +0x38 (roll). The composition is identical to
//            model_local_matrix (roll, then pitch, then yaw, translation last).
//   0x45d2f0 is the same composition with a uniform scale matrix base
//            (scale value shifted left by 6; 64 is 1.0).
//   0x45cb40 is the 22-bit fixed-point matrix product; identical to
//            model_multiply_matrix.
//   0x480ff0 is the crng matrix callback: one local matrix per container mesh
//            slot (no camera multiply).
//   0x481060 is the crng draw callback: camera matrix (0x8d1340 x local), the
//            material animation frame counter 0xad3cf4, and one draw per
//            container mesh. The frame rule is
//            (time / (frame_duration_raw >> 16)) % (frame_count_raw >> 16) with
//            the frame_count/duration words of the runtime LOD record.
//   0x46b830 is the static-object draw: mesh selection by object +0x6c bit 2,
//            camera matrix, LOD choice by screen radius against the file
//            thresholds, a matrix slot per first-LOD mesh, then one draw per
//            mesh of the chosen LOD. It passes clip mask and level 0 to
//            0x46b1d0. No static caller references 0x46b830 in the shipped
//            image (full dword/relative-call scan); the body is kept exact.
//
// The runtime LOD record (0xb8 bytes per 0x485e30) fields read by these paths:
//   +0x40 frame count, dword; the whole count is raw >> 16.
//   +0x58 frame duration, dword; the whole duration is raw >> 16 and the high
//         word at +0x5a is the non-zero guard. 0x481060 leaves 0xad3cf4
//         unchanged when that guard is zero.
//   +0x98 mesh count; +0x9c mesh array, 0x70-byte entries in file order.
// The decoded ModelLod keeps the same order, and raw_header[0x40..0x43] /
// [0x58..0x5b] carry those two dwords. See .local/opencode/model_instances-needs.md.
namespace dfre {

// One object instance block at object +0x24. Translation is 16.16; angles are
// unsigned full-turn yaw, pitch, roll.
struct ModelInstanceFields {
    std::array<std::int32_t, 3> translation{};
    std::array<std::uint32_t, 3> angles{};
};

// Original 0x45d1e0; exactly model_local_matrix for the same fields.
ModelMatrix model_instance_local_matrix(const ModelInstanceFields& fields);
// Original 0x45d1e0 followed by 0x45cb40(world_to_camera, local, local).
ModelMatrix model_instance_camera_matrix(const ModelInstanceFields& fields,
    const ModelMatrix& world_to_camera);
// Original 0x45d2f0: scaled_local = translation x rotation x scale.
// The scale matrix is diag(scale << 6); observed callers are the face (0x480f40)
// and ef3d (0x480e70) matrix callbacks, not the generic/static paths.
ModelMatrix model_instance_local_matrix_scaled(const ModelInstanceFields& fields, std::int32_t scale);

// One runtime LOD record subset (see the field map above).
struct ModelInstanceLod {
    std::size_t mesh_count{}; // +0x98
    std::int32_t frame_count_raw{}; // +0x40
    std::int32_t frame_duration_raw{}; // +0x58
};

// Original 0x481060 frame counter. The original writes it to global 0xad3cf4
// before the mesh draws and clears that global after them.
// Returns nullopt when the duration whole part is zero: the original then keeps
// the previous counter value (the guard is the high word at +0x5a).
// Throws std::invalid_argument when the count whole part is zero with a non-zero
// duration, or when the signed divide overflows (INT32_MIN % -1); the original
// raises the divide error in both cases (integer fault).
std::optional<std::int32_t> model_instance_animation_frame(std::uint32_t time,
    const ModelInstanceLod& lod);

// Original 0x480ff0. One local matrix per slot; slot 0 is the 0x45d1e0 output
// and every other slot is a copy. No camera multiply.
std::vector<ModelMatrix> model_instance_local_matrices(std::size_t mesh_count,
    const ModelInstanceFields& fields);

// Original 0x481060 draw decision for one crng container.
struct ModelInstanceDraw {
    std::optional<std::int32_t> frame{}; // null: the original leaves 0xad3cf4 unchanged
    ModelMatrix local{}, camera{};
    std::size_t mesh_count{};
    // The original builds one stack slot and passes slot 0 for every mesh. This
    // port repeats that camera matrix so the existing mesh draw can index one
    // matrix per mesh without changing any drawn value.
    std::vector<ModelMatrix> matrices;
};
ModelInstanceDraw build_model_instance_draw(const ModelInstanceLod& lod,
    const ModelInstanceFields& fields, const ModelMatrix& world_to_camera, std::uint32_t time);

// Original 0x46b830 inputs. A record is one runtime model (thresholds and the
// four LOD records); the original picks object +0x14 only when flag byte +0x6c
// bit 2 is set and that pointer is non-null, otherwise object +0x10.
struct ModelStaticRecord {
    std::array<std::int32_t, 4> lod_thresholds{}; // model +0x18..+0x24
    // model +0x50..+0x5c; nullopt is a null record pointer.
    std::array<std::optional<ModelInstanceLod>, 4> lods{};
};

struct ModelStaticObject {
    bool use_secondary{}; // object +0x6c bit 2
    std::optional<ModelStaticRecord> primary{}; // object +0x10
    std::optional<ModelStaticRecord> secondary{}; // object +0x14
};

struct ModelStaticDraw {
    bool has_model{};
    std::optional<std::size_t> lod{}; // null: radius at or below 0x8000, or below every threshold
    ModelMatrix local{}, camera{};
    // Slots the original initializes: max(first-LOD mesh count, 1); it fills slots
    // 1.. with copies of slot 0. This port returns that many entries plus a
    // repeat of the camera matrix up to the chosen mesh count so the existing
    // mesh draw can index one matrix per mesh.
    std::size_t matrix_slots{};
    std::vector<ModelMatrix> matrices;
    std::vector<std::size_t> mesh_indices; // draws, 0..chosen mesh count - 1
};
// Throws std::invalid_argument when the original would dereference a null
// record: the fill count read of model +0x50, or the selected LOD record.
ModelStaticDraw build_model_static_draw(const ModelStaticObject& object,
    const ModelInstanceFields& fields, const ModelMatrix& world_to_camera,
    std::int32_t screen_radius);

} // namespace dfre
