#include "dfre/model_instances.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

// Original routines reproduced here (Df.exe sha256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff):
//   0x45d1e0 mesh-instance local matrix
//   0x45d2f0 mesh-instance local matrix with uniform scale
//   0x45cb40 matrix product (via model_multiply_matrix)
//   0x480ff0 crng matrix callback slots
//   0x481060 crng draw callback counter and camera matrix
//   0x46b830 static-object draw decision
// See include/dfre/model_instances.hpp for the field map and evidence notes.
namespace dfre {
namespace {

// Arithmetic right shift, as the original `sar` does.
std::int32_t signed_shift(std::int32_t value, unsigned shift) {
    return std::int32_t(value >> shift);
}

// Low 32 bits reinterpreted as signed, as the original 32-bit registers do.
std::int32_t wrapped_signed(std::uint32_t value) {
    return value <= 2147483647u ? std::int32_t(value) : std::int32_t(std::int64_t(value) - 4294967296LL);
}

// 0x45ca70 scale matrix: diagonal scale << 6, all other entries zero.
ModelMatrix scale_matrix(std::int32_t scale) {
    ModelMatrix result{};
    const auto value = wrapped_signed(std::uint32_t(scale) << 6);
    result[0] = result[5] = result[10] = value;
    return result;
}

// Exact axis matrix for one rotation component, built through the exported
// model_local_matrix with a single non-zero angle. Identity products in that
// function are exact, so this equals the 0x45c970 matrix.
ModelMatrix axis_matrix(std::size_t index, std::uint32_t angle) {
    std::array<std::uint32_t, 3> angles{};
    angles[index] = angle;
    return model_local_matrix({0, 0, 0}, angles);
}

} // namespace

ModelMatrix model_instance_local_matrix(const ModelInstanceFields& fields) {
    // 0x45d1e0: translations from +0x24/+0x28/+0x2c and angles from
    // +0x30/+0x34/+0x38 give exactly the model_local_matrix composition.
    return model_local_matrix(fields.translation, fields.angles);
}

ModelMatrix model_instance_camera_matrix(const ModelInstanceFields& fields,
    const ModelMatrix& world_to_camera) {
    // Callers run 0x45d1e0 into a buffer, then 0x45cb40(world, local, local).
    return model_multiply_matrix(world_to_camera, model_instance_local_matrix(fields));
}

ModelMatrix model_instance_local_matrix_scaled(const ModelInstanceFields& fields, std::int32_t scale) {
    // 0x45d2f0 starts from the scale matrix and multiplies each axis rotation
    // in roll, pitch, yaw order, then applies the translation matrix. The
    // original rounds after every product, so this keeps the same order instead
    // of scaling a finished local matrix.
    auto accumulated = scale_matrix(scale);
    for (unsigned index : {2u, 1u, 0u}) {
        if (fields.angles[index] != 0) {
            accumulated = model_multiply_matrix(axis_matrix(index, fields.angles[index]), accumulated);
        }
    }
    const auto translation = model_local_matrix(fields.translation, {0, 0, 0});
    return model_multiply_matrix(translation, accumulated);
}

std::optional<std::int32_t> model_instance_animation_frame(std::uint32_t time,
    const ModelInstanceLod& lod) {
    // 0x481060: EAX = [0x6e471c], EBX = [record+0x58] sar 16; DIV; then IDIV by
    // [record+0x40] sar 16. The guard word at +0x5a is the high half of the
    // duration dword, so a zero duration leaves 0xad3cf4 alone.
    const auto duration = signed_shift(lod.frame_duration_raw, 16);
    if (duration == 0) return {};
    const auto count = signed_shift(lod.frame_count_raw, 16);
    if (count == 0) {
        throw std::invalid_argument("model instance frame count is zero with a non-zero duration");
    }
    const auto quotient = time / std::uint32_t(duration); // unsigned `div`
    const auto signed_quotient = wrapped_signed(quotient);
    if (signed_quotient == std::numeric_limits<std::int32_t>::min() && count == -1) {
        // `idiv` raises the divide error on this overflow; C++ % would be
        // undefined. Report it as an invalid input instead.
        throw std::invalid_argument("model instance frame divide overflows");
    }
    return signed_quotient % count; // signed `idiv`
}

std::vector<ModelMatrix> model_instance_local_matrices(std::size_t mesh_count,
    const ModelInstanceFields& fields) {
    // 0x480ff0: slot 0 is the 0x45d1e0 output; slots 1..count-1 copy it. Slot 0
    // is written even when the count is zero or one.
    const auto local = model_instance_local_matrix(fields);
    return std::vector<ModelMatrix>(std::max<std::size_t>(mesh_count, 1), local);
}

ModelInstanceDraw build_model_instance_draw(const ModelInstanceLod& lod,
    const ModelInstanceFields& fields, const ModelMatrix& world_to_camera, std::uint32_t time) {
    // 0x481060 order: frame counter, local matrix, camera product, mesh loop.
    ModelInstanceDraw result;
    result.frame = model_instance_animation_frame(time, lod);
    result.local = model_instance_local_matrix(fields);
    result.camera = model_multiply_matrix(world_to_camera, result.local);
    result.mesh_count = lod.mesh_count;
    // The original passes its single stack slot as the array base with level 0,
    // so every mesh draws with the camera matrix. Repeat it per mesh for the
    // existing mesh draw.
    result.matrices.assign(lod.mesh_count, result.camera);
    return result;
}

ModelStaticDraw build_model_static_draw(const ModelStaticObject& object,
    const ModelInstanceFields& fields, const ModelMatrix& world_to_camera,
    std::int32_t screen_radius) {
    // 0x46b830 head: object +0x6c bit 2 with a non-null +0x14 selects the
    // secondary model, otherwise +0x10; a null model returns immediately.
    ModelStaticDraw result;
    const ModelStaticRecord* model = nullptr;
    if (object.use_secondary && object.secondary) model = &*object.secondary;
    else if (object.primary) model = &*object.primary;
    if (model == nullptr) return result;
    result.has_model = true;
    // The local matrix and the camera product are built before the radius gate.
    result.local = model_instance_local_matrix(fields);
    result.camera = model_multiply_matrix(world_to_camera, result.local);
    const auto level = select_model_level(model->lod_thresholds, screen_radius);
    if (!level) return result;
    // The fill loop always reads the first record (model +0x50) even when a
    // later record is selected; a null record faults in the original.
    if (!model->lods[0]) {
        throw std::invalid_argument("static model draw needs the first LOD record for its matrix fill");
    }
    if (!model->lods[*level]) {
        throw std::invalid_argument("static model draw selected a null LOD record");
    }
    result.lod = *level;
    result.matrix_slots = std::max<std::size_t>(model->lods[0]->mesh_count, 1);
    const auto draws = model->lods[*level]->mesh_count;
    result.matrices.assign(std::max(result.matrix_slots, draws), result.camera);
    result.mesh_indices.reserve(draws);
    for (std::size_t index = 0; index < draws; ++index) result.mesh_indices.push_back(index);
    return result;
}

} // namespace dfre
