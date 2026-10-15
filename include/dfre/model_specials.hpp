#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "dfre/models.hpp"

namespace dfre {

// Special model tags rebuilt from Df.exe (SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff).
// Data only: this module produces typed draw/matrix plans; the renderer draws.
//
// Original callback table 0x500c1c, 16 rows of 0x14 bytes. The org0 and gnrc
// rows belong to other modules; this module owns the other 14 keys.

enum class ModelSpecialTag {
    bird, llun, pwp0, tcfe, ef3d, face, fish, helo, lamp, palm, peng, shark, tire, tree,
};

struct ModelSpecialDef {
    ModelSpecialTag tag;
    std::uint32_t key;              // 4-byte key as a little-endian dword
    const char* name;               // original tag word
    std::uint32_t matrix_address;   // 0x500c1c row +0x08
    std::uint32_t draw_address;     // 0x500c1c row +0x04
};

const ModelSpecialDef* model_special_defs();
std::size_t model_special_def_count();
const ModelSpecialDef* find_model_special_def(std::uint32_t key);

// Raw instance fields (original offsets).
struct ModelSpecialInstance {
    std::int32_t mesh_count = 0;      // +0x98
    std::uint32_t mesh_base = 0;      // +0x9c mesh array, 0x70-byte records
    std::uint32_t element_base = 0;   // +0xa4 element array, 0xc-byte records
    std::int32_t divisor_58 = 0;      // +0x58, 16.16
    std::int32_t flag_5a = 0;         // +0x5a, word
    std::int32_t threshold_40 = 0;    // +0x40, 16.16
};

// Raw object fields (original offsets).
struct ModelSpecialObject {
    std::int32_t distance_fc = 0;      // +0xfc
    std::int32_t value_98 = 0;         // +0x98
    std::int32_t value_a0 = 0;         // +0xa0
    std::int32_t value_e0 = 0;         // +0xe0 selection or angle source
    std::int32_t angle_9c = 0;         // +0x9c angle source
    std::int32_t angle_a4 = 0;         // +0xa4 angle source
    std::int32_t angle_c4 = 0;         // +0xc4 angle source
    std::int32_t angle_c8 = 0;         // +0xc8 angle source
    std::int32_t value_94 = 0;         // +0x94 angle source (tire)
    std::int32_t scale_7c = 0;         // +0x7c scaled base
    std::int32_t z_30 = 0;             // +0x30 yaw angle
    std::int32_t phase_ec = 0;         // +0xec animation phase (fish)
    std::int32_t mode_f0 = 0;          // +0xf0 mode (peng)
    std::int32_t angle_e4 = 0;         // +0xe4 (peng)
    std::int32_t angle_e8 = 0;         // +0xe8 (peng)
    std::int32_t z_high = 0;           // 0x8cfa30 camera yaw (tree/face)
    std::int32_t z_low = 0;            // 0x8cfa34 camera pitch (tcfe direction)
    std::int32_t phase_33 = 0;         // +0x33 low byte (fish phase)
    std::int32_t flag_3c = 0;          // +0x3c direction gate (tcfe)
    std::int32_t callback_arg3 = 0;    // third callback argument (face slot base)
};

// How a step matrix is built from the shared base matrix.
enum class ModelSpecialMatrixKind {
    base,        // shared base matrix only
    translate,   // base x T(-element)
    pivot,       // base x T(-element) x R(angles) x T(element)
};

struct ModelSpecialStep {
    std::uint32_t mesh_offset = 0;      // added to mesh_base
    ModelSpecialMatrixKind kind = ModelSpecialMatrixKind::base;
    std::uint32_t element_index = 0;    // 0xc-byte element record
    std::array<std::int32_t, 3> angles{}; // p3, p4, p5 angle sources
};

// One matrix slot the matrix callback writes (0xc22b80 + index * 0x40).
struct ModelSpecialSlot {
    std::uint32_t index = 0;
    ModelSpecialMatrixKind kind = ModelSpecialMatrixKind::base;
    std::uint32_t element_index = 0;
    std::array<std::int32_t, 3> angles{};
};

// An ordered side effect the original performs around the draws.
struct ModelSpecialWrite {
    enum class Target { object_z, object_direction, object_phase, counter };
    Target target = Target::counter;
    std::int32_t value = 0;
    bool restore = false;   // true when the original saves and writes back
};

struct ModelSpecialPlan {
    ModelSpecialTag tag = ModelSpecialTag::bird;
    bool draws = true;             // false for llun (draw callback is empty)
    bool has_base_matrix = true;   // false when the matrix callback returns 0
    bool view_multiply = false;    // shared base multiplied by 0x8d1340
    bool scaled_base = false;      // 0x45d2f0 with object +0x7c instead of 0x45d1e0
    std::int32_t placement_angle_offset = 0;  // added to placement[3] (peng)
    std::vector<ModelSpecialStep> steps;
    std::vector<ModelSpecialSlot> slots;          // slots the matrix callback writes
    std::vector<std::uint32_t> localized_slots;   // slots whose +0x3c flag is 1
    std::vector<ModelSpecialWrite> writes;
};

ModelSpecialPlan build_model_special_plan(ModelSpecialTag tag,
                                          const ModelSpecialInstance& instance,
                                          const ModelSpecialObject& object);

// Executes the plan for one draw. `object_matrix` is the object placement
// matrix (instance API), `view` the world-to-camera matrix, `elements` the
// element records (x, y, z triples). Returns one matrix per step.
std::vector<ModelMatrix> model_special_step_matrices(
    const ModelSpecialPlan& plan, const ModelMatrix& object_matrix,
    const ModelMatrix& view,
    const std::vector<std::array<std::int32_t, 3>>& elements);

// Slot matrices the matrix callback writes (0xc22b80 + index * 0x40).
std::vector<ModelMatrix> model_special_slot_matrices(
    const ModelSpecialPlan& plan, const ModelMatrix& slot_base,
    const std::vector<std::array<std::int32_t, 3>>& elements);

// One pivot transform (0x45d400): T(-p) x R(angles) x T(p).
ModelMatrix model_special_pivot_matrix(const std::array<std::int32_t, 3>& pivot,
                                       const std::array<std::int32_t, 3>& angles);

// The triangle fold used by the fish/peng animation angles.
std::int32_t model_special_angle_fold(std::uint32_t packed);

// The counter rule before draws (0x42a610 / 0x480e80).
std::int32_t model_special_counter(const ModelSpecialInstance& instance,
                                   const ModelSpecialObject& object);
// The same rule before the threshold clamp.
std::int32_t model_special_counter_raw(const ModelSpecialInstance& instance,
                                       const ModelSpecialObject& object);

} // namespace dfre
