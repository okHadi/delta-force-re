#include "dfre/model_specials.hpp"

namespace dfre {
namespace {

ModelSpecialStep base_step(std::uint32_t mesh_offset) {
    ModelSpecialStep step;
    step.mesh_offset = mesh_offset;
    step.kind = ModelSpecialMatrixKind::base;
    return step;
}

ModelSpecialStep pivot_step(std::uint32_t mesh_offset, std::uint32_t element,
                            std::int32_t p3, std::int32_t p4, std::int32_t p5) {
    ModelSpecialStep step;
    step.mesh_offset = mesh_offset;
    step.kind = ModelSpecialMatrixKind::pivot;
    step.element_index = element;
    step.angles = {p3, p4, p5};
    return step;
}

ModelSpecialStep translate_step(std::uint32_t mesh_offset, std::uint32_t element) {
    ModelSpecialStep step;
    step.mesh_offset = mesh_offset;
    step.kind = ModelSpecialMatrixKind::translate;
    step.element_index = element;
    return step;
}

ModelSpecialSlot base_slot(std::uint32_t index) {
    ModelSpecialSlot slot;
    slot.index = index;
    slot.kind = ModelSpecialMatrixKind::base;
    return slot;
}

ModelSpecialSlot pivot_slot(std::uint32_t index, std::uint32_t element,
                            std::int32_t p3, std::int32_t p4, std::int32_t p5) {
    ModelSpecialSlot slot;
    slot.index = index;
    slot.kind = ModelSpecialMatrixKind::pivot;
    slot.element_index = element;
    slot.angles = {p3,p4,p5};
    return slot;
}

ModelSpecialSlot translate_slot(std::uint32_t index, std::uint32_t element) {
    ModelSpecialSlot slot;
    slot.index = index;
    slot.kind = ModelSpecialMatrixKind::translate;
    slot.element_index = element;
    return slot;
}

// 0x44c8f0 / 0x44da20 fold: value below 0x1000001 subtracts 0x800000,
// otherwise 0x1800000 minus the value.
std::int32_t fold(std::uint32_t value) {
    if (value < 0x1000001u) {
        return static_cast<std::int32_t>(value) - 0x800000;
    }
    return 0x1800000 - static_cast<std::int32_t>(value);
}

// The base matrix and the per-mesh copies every matrix callback writes.
void add_base_copies(ModelSpecialPlan& plan, std::size_t count) {
    plan.slots.push_back(base_slot(0));
    for (std::size_t index = 1; index < count; ++index) {
        plan.slots.push_back(base_slot(static_cast<std::uint32_t>(index)));
    }
}

void add_counter_writes(ModelSpecialPlan& plan, const ModelSpecialInstance& instance,
                        const ModelSpecialObject& object) {
    // 0x42a610 / 0x480e80: optional set, optional clamp, final clear.
    const std::int32_t value = model_special_counter_raw(instance, object);
    if (instance.flag_5a != 0) {
        plan.writes.push_back({ModelSpecialWrite::Target::counter, value, false});
        if ((instance.threshold_40 >> 16) <= value) {
            plan.writes.push_back({ModelSpecialWrite::Target::counter, 0, false});
        }
    }
    plan.writes.push_back({ModelSpecialWrite::Target::counter, 0, false});
}

// The scratch is a fixed slot array: a later write replaces the matrix that
// was copied there before.
void set_slot(ModelSpecialPlan& plan, const ModelSpecialSlot& slot) {
    for (auto& existing : plan.slots) {
        if (existing.index == slot.index) {
            existing = slot;
            return;
        }
    }
    plan.slots.push_back(slot);
}

void localize(ModelSpecialPlan& plan, std::initializer_list<std::uint32_t> slots) {
    for (const auto slot : slots) {
        plan.localized_slots.push_back(slot);
    }
}

} // namespace

const ModelSpecialDef* model_special_defs() {
    static const ModelSpecialDef defs[] = {
        {ModelSpecialTag::bird, 0x62697264u, "drib", 0x44ce80u, 0x44cef0u},
        {ModelSpecialTag::tcfe, 0x65666374u, "tcfe", 0x42a600u, 0x42a610u},
        {ModelSpecialTag::ef3d, 0x65663364u, "d3fe", 0x480e70u, 0x480e80u},
        {ModelSpecialTag::face, 0x66616365u, "ecaf", 0x480f40u, 0x480f80u},
        {ModelSpecialTag::fish, 0x66697368u, "hsif", 0x44c860u, 0x44c8f0u},
        {ModelSpecialTag::helo, 0x68656c6fu, "oleh", 0x44c4c0u, 0x44c570u},
        {ModelSpecialTag::lamp, 0x6c616d70u, "pmal", 0x44d0d0u, 0x44d150u},
        {ModelSpecialTag::llun, 0x6e756c6cu, "llun", 0x481110u, 0x481120u},
        {ModelSpecialTag::palm, 0x70616c6du, "mlap", 0x44d2a0u, 0x44d5e0u},
        {ModelSpecialTag::peng, 0x70656e67u, "gnep", 0x44d990u, 0x44da20u},
        {ModelSpecialTag::pwp0, 0x70777030u, "0pwp", 0x481650u, 0x481660u},
        {ModelSpecialTag::shark, 0x7368726bu, "krhs", 0x44cdf0u, 0x44ce00u},
        {ModelSpecialTag::tire, 0x74697265u, "erit", 0x42a170u, 0x42a290u},
        {ModelSpecialTag::tree, 0x74726565u, "eert", 0x4816d0u, 0x4817a0u},
    };
    return defs;
}

std::size_t model_special_def_count() {
    return 14;
}

const ModelSpecialDef* find_model_special_def(std::uint32_t key) {
    const ModelSpecialDef* defs = model_special_defs();
    for (std::size_t index = 0; index < model_special_def_count(); ++index) {
        if (defs[index].key == key) {
            return &defs[index];
        }
    }
    return nullptr;
}

std::int32_t model_special_angle_fold(std::uint32_t packed) {
    return fold(packed);
}

std::int32_t model_special_counter_raw(const ModelSpecialInstance& instance,
                                       const ModelSpecialObject& object) {
    if (instance.flag_5a == 0) {
        return 0;
    }
    const std::int32_t divisor = instance.divisor_58 >> 16;
    if (divisor == 0) {
        return 0;
    }
    return object.distance_fc / divisor;
}

std::int32_t model_special_counter(const ModelSpecialInstance& instance,
                                   const ModelSpecialObject& object) {
    std::int32_t value = model_special_counter_raw(instance, object);
    if ((instance.threshold_40 >> 16) <= value) {
        value = 0;
    }
    return value;
}

ModelSpecialPlan build_model_special_plan(ModelSpecialTag tag,
                                          const ModelSpecialInstance& instance,
                                          const ModelSpecialObject& object) {
    ModelSpecialPlan plan;
    plan.tag = tag;
    const std::size_t count =
        instance.mesh_count > 0 ? static_cast<std::size_t>(instance.mesh_count) : 0;
    switch (tag) {
    case ModelSpecialTag::llun: {
        // 0x481110 returns 0 and 0x481120 is empty: nothing is drawn.
        plan.draws = false;
        plan.has_base_matrix = false;
        break;
    }
    case ModelSpecialTag::pwp0: {
        // 0x481660: every mesh with the plain object matrix, no view multiply.
        plan.has_base_matrix = false;
        for (std::size_t index = 0; index < count; ++index) {
            plan.steps.push_back(base_step(static_cast<std::uint32_t>(index * 0x70)));
        }
        break;
    }
    case ModelSpecialTag::shark: {
        // 0x44ce00: every mesh with the camera matrix.
        plan.has_base_matrix = false;
        plan.view_multiply = true;
        for (std::size_t index = 0; index < count; ++index) {
            plan.steps.push_back(base_step(static_cast<std::uint32_t>(index * 0x70)));
        }
        break;
    }
    case ModelSpecialTag::tcfe: {
        // 0x42a610: scaled object matrix, one mesh, distance counter.
        plan.has_base_matrix = false;
        plan.view_multiply = true;
        plan.scaled_base = true;
        plan.steps.push_back(base_step(0));
        plan.writes.push_back({ModelSpecialWrite::Target::object_z,
                               static_cast<std::int32_t>(std::uint32_t(object.z_high) + 0x7fffffffu),
                               false});
        if (object.flag_3c != 0) {
            plan.writes.push_back({ModelSpecialWrite::Target::object_direction,
                                   static_cast<std::int32_t>(0x80000000u - object.z_low),
                                   false});
        }
        plan.writes.push_back({ModelSpecialWrite::Target::object_z, object.z_30, true});
        add_counter_writes(plan, instance, object);
        break;
    }
    case ModelSpecialTag::ef3d: {
        // 0x480e80: scaled camera matrix for every mesh, then the counter reset.
        plan.has_base_matrix = false;
        plan.view_multiply = true;
        plan.scaled_base = true;
        for (std::size_t index = 0; index < count; ++index) {
            plan.steps.push_back(base_step(static_cast<std::uint32_t>(index * 0x70)));
        }
        add_counter_writes(plan, instance, object);
        break;
    }
    case ModelSpecialTag::face: {
        // 0x480f80: scaled matrix with a raised object z, one mesh.
        plan.view_multiply = true;
        plan.scaled_base = true;
        plan.steps.push_back(base_step(0));
        plan.writes.push_back({ModelSpecialWrite::Target::object_z,
                               static_cast<std::int32_t>(std::uint32_t(object.z_high) + 0x7fffffffu),
                               false});
        plan.writes.push_back({ModelSpecialWrite::Target::object_z, object.z_30, true});
        // 0x480f40 writes the base matrix built with the callback's third
        // argument raised by 0x7fffffff and clears its +0x3c flag.
        plan.slots.push_back(base_slot(0));
        break;
    }
    case ModelSpecialTag::tree: {
        // 0x4817a0: camera matrix, mesh 0, then one pivot per element.
        plan.view_multiply = true;
        plan.steps.push_back(base_step(0));
        for (std::size_t index = 1; index < count; ++index) {
            plan.steps.push_back(pivot_step(static_cast<std::uint32_t>(index * 0x70),
                                            static_cast<std::uint32_t>(index - 1),
                                            static_cast<std::int32_t>(std::uint32_t(object.z_high) - std::uint32_t(object.z_30)), 0, 0));
        }
        // 0x4816d0 writes one pivot per extra mesh into the slots.
        plan.slots.push_back(base_slot(0));
        for (std::size_t index = 1; index < count; ++index) {
            plan.slots.push_back(pivot_slot(static_cast<std::uint32_t>(index),
                                            static_cast<std::uint32_t>(index - 1),
                                            static_cast<std::int32_t>(std::uint32_t(object.callback_arg3) - std::uint32_t(object.z_30)), 0, 0));
        }
        break;
    }
    case ModelSpecialTag::bird: {
        // 0x44cef0: camera matrix, then two element pivots with +-0xe0.
        plan.view_multiply = true;
        plan.steps.push_back(base_step(0));
        plan.steps.push_back(pivot_step(0x70, 0, 0, 0, object.value_e0));
        plan.steps.push_back(pivot_step(0xe0, 1, 0, 0, -object.value_e0));
        add_base_copies(plan, count);
        break;
    }
    case ModelSpecialTag::tire: {
        // 0x42a290: camera matrix, then elements 2 and 1 with +-0x94.
        plan.view_multiply = true;
        plan.steps.push_back(base_step(0));
        plan.steps.push_back(pivot_step(0xe0, 2, object.value_94, 0, 0));
        plan.steps.push_back(pivot_step(0x150, 1, object.value_94, 0, 0));
        // 0x42a170 writes the base, the two translated slots and flag slot 1.
        plan.slots.push_back(base_slot(0));
        plan.slots.push_back(pivot_slot(2, 2, object.value_94, 0, 0));
        plan.slots.push_back(pivot_slot(3, 1, object.value_94, 0, 0));
        localize(plan, {1});
        break;
    }
    case ModelSpecialTag::lamp: {
        // 0x44d150: camera matrix, element 0 pivot on one or two meshes.
        plan.view_multiply = true;
        plan.steps.push_back(base_step(0));
        plan.steps.push_back(pivot_step(0x70, 0, 0, 0, object.value_e0));
        if (count >= 3) {
            plan.steps.push_back(pivot_step(0xe0, 0, 0, 0, object.value_e0));
        }
        // 0x44d0d0: base copies, slot 2 localized.
        add_base_copies(plan, count);
        localize(plan, {2});
        break;
    }
    case ModelSpecialTag::helo: {
        // 0x44c570: four pivots and two mesh choices by fixed thresholds.
        plan.view_multiply = true;
        plan.steps.push_back(base_step(0));
        plan.steps.push_back(pivot_step(object.value_98 < 0x0c000000 ? 0x70 : 0xe0,
                                        0, object.angle_9c, 0, 0));
        plan.steps.push_back(pivot_step(object.value_a0 < 0x0c000000 ? 0x150 : 0x1c0,
                                        2, 0, object.angle_a4, 0));
        plan.steps.push_back(pivot_step(0x230, 4, object.angle_c4, object.angle_c8, 0));
        plan.steps.push_back(base_step(0x2a0));
        // 0x44c4c0: slots 3 and 5, or slots 2 and 4, are localized.
        add_base_copies(plan, count);
        if (object.value_98 < 0x0c000000) {
            localize(plan, {3, 5});
        } else {
            localize(plan, {2, 4});
        }
        break;
    }
    case ModelSpecialTag::palm: {
        // 0x44d5e0 draws and 0x44d2a0 flags decide by object +0xe0.
        plan.view_multiply = true;
        const std::int32_t selector = object.value_e0;
        add_base_copies(plan, count);
        if (selector == 0) {
            plan.steps.push_back(base_step(0));
            plan.steps.push_back(base_step(0x70));
            plan.steps.push_back(base_step(0xe0));
            localize(plan, {3, 4, 5});
        } else if (selector == 1) {
            plan.steps.push_back(base_step(0));
            plan.steps.push_back(base_step(0x150));
            localize(plan, {1, 2, 4, 5});
        } else if (selector == 2) {
            plan.steps.push_back(base_step(0));
            plan.steps.push_back(base_step(0x70));
            plan.steps.push_back(base_step(0x230));
            localize(plan, {2, 3, 4});
        } else if (selector == 0x10) {
            plan.steps.push_back(translate_step(0x70, 0));
            plan.steps.push_back(translate_step(0xe0, 0));
            plan.steps.push_back(translate_step(0x1c0, 0));
            set_slot(plan, translate_slot(1, 0));
            set_slot(plan, translate_slot(2, 0));
            set_slot(plan, translate_slot(4, 0));
            localize(plan, {0, 3, 5});
        } else if (selector == 0x11) {
            plan.steps.push_back(translate_step(0xe0, 1));
            set_slot(plan, translate_slot(2, 1));
            localize(plan, {0, 1, 3, 4, 5});
        } else if (selector == 0x20) {
            plan.steps.push_back(translate_step(0x70, 0));
            plan.steps.push_back(translate_step(0x1c0, 0));
            set_slot(plan, translate_slot(1, 0));
            set_slot(plan, translate_slot(4, 0));
            localize(plan, {0, 2, 3, 5});
        }
        break;
    }
    case ModelSpecialTag::fish: {
        // 0x44c8f0: phase byte, camera matrix, then five element pivots.
        plan.view_multiply = true;
        const std::int32_t phase_once =
            static_cast<std::int32_t>((static_cast<std::uint32_t>(object.phase_33) - 0x80u) & 0xFFu);
        const std::int32_t phase_twice =
            static_cast<std::int32_t>((static_cast<std::uint32_t>(phase_once) - 0x80u) & 0xFFu);
        plan.writes.push_back({ModelSpecialWrite::Target::object_phase, phase_once, false});
        plan.writes.push_back({ModelSpecialWrite::Target::object_phase, phase_twice, true});
        plan.steps.push_back(base_step(0));
        const std::uint32_t phase = static_cast<std::uint32_t>(object.phase_ec);
        const std::int32_t first = fold(phase & 0x1ff0000u) * 64;
        const std::int32_t second = fold((phase >> 1) & 0x1ff0000u) * 16;
        const std::int32_t third = fold((phase >> 1) & 0x1ff0000u) * -0x10;
        const std::int32_t swing = fold((phase * 2) & 0x1ff0000u);
        plan.steps.push_back(pivot_step(0x70, 0, first, 0, 0));
        plan.steps.push_back(pivot_step(0xe0, 1, 0, 0, second));
        plan.steps.push_back(pivot_step(0x150, 2, 0, 0, third));
        plan.steps.push_back(pivot_step(0x1c0, 3, swing * 4, swing * 8, 0));
        plan.steps.push_back(pivot_step(0x230, 4, swing * -4, swing * -8, 0));
        add_base_copies(plan, count);
        // 0x44c860 localizes slots 2, 3, 4 and 5.
        localize(plan, {2, 3, 4, 5});
        break;
    }
    case ModelSpecialTag::peng: {
        // 0x44da20: folded phase angles drive the body lean and four pivots.
        plan.view_multiply = true;
        std::uint32_t body = static_cast<std::uint32_t>(object.value_e0) & 0x1ff0000u;
        std::uint32_t beak = static_cast<std::uint32_t>(object.angle_e4) & 0x1ff0000u;
        std::uint32_t leg = (static_cast<std::uint32_t>(object.angle_e4) - 0x400000u) & 0x1ff0000u;
        std::uint32_t tail = static_cast<std::uint32_t>(object.angle_e8) & 0x1ff0000u;
        if (body > 0x1000000u) {
            body = 0x2000000u - body;
        }
        if (tail > 0x1000000u) {
            tail = 0x2000000u - tail;
        }
        std::int32_t step_angle = fold(beak);
        std::int32_t lean = fold(leg);
        if (lean < 0x600001) {
            if (lean < -0x600000) {
                lean = -0x600000;
            }
        } else {
            lean = 0x600000;
        }
        if (object.mode_f0 == 2) {
            step_angle = step_angle * 2;
            body += 0x04000000u;
        } else if (step_angle < 0) {
            step_angle = 0;
        }
        plan.placement_angle_offset = lean * -0x10;
        plan.steps.push_back(base_step(0));
        plan.steps.push_back(pivot_step(0x70, 0, 0, 0,
                                        static_cast<std::int32_t>(body) << 3));
        plan.steps.push_back(pivot_step(0xe0, 1, 0, 0,
                                        static_cast<std::int32_t>(body) * -8));
        plan.steps.push_back(pivot_step(0x150, 2, 0, step_angle * 16, 0));
        plan.steps.push_back(pivot_step(0x1c0, 3, 0, step_angle * -0x10, 0));
        plan.steps.push_back(pivot_step(0x230, 4, 0,
                                        static_cast<std::int32_t>(tail) * -0x10, 0));
        add_base_copies(plan, count);
        // 0x44d990 localizes slots 1, 2, 3, 4 and 5.
        localize(plan, {1, 2, 3, 4, 5});
        break;
    }
    }
    return plan;
}

ModelMatrix model_special_pivot_matrix(const std::array<std::int32_t, 3>& pivot,
                                       const std::array<std::int32_t, 3>& angles) {
    // 0x45d400: T(p), then 0x45c970 rotations for [5]=axis0, [4]=axis1,
    // [3]=axis2 (each one multiplies on the left), then T(-p). Keeping the
    // original order matters: fixed-point rounding is not associative.
    ModelMatrix local = model_local_matrix(pivot, {0, 0, 0});
    const struct {
        int axis;
        std::int32_t value;
    } rotations[] = {{0, angles[2]}, {1, angles[1]}, {2, angles[0]}};
    for (const auto& rotation : rotations) {
        if (rotation.value == 0) {
            continue;
        }
        std::array<std::uint32_t, 3> single{};
        single[static_cast<std::size_t>(2 - rotation.axis)] =
            static_cast<std::uint32_t>(rotation.value);
        const ModelMatrix axis_matrix = model_local_matrix({0, 0, 0}, single);
        local = model_multiply_matrix(axis_matrix, local);
    }
    const ModelMatrix back = model_local_matrix(
        {-pivot[0], -pivot[1], -pivot[2]}, {0, 0, 0});
    return model_multiply_matrix(back, local);
}

namespace {

ModelMatrix build_step_matrix(const ModelSpecialStep& step, const ModelMatrix& base,
                              const std::vector<std::array<std::int32_t, 3>>& elements) {
    if (step.kind == ModelSpecialMatrixKind::base || step.element_index >= elements.size()) {
        return base;
    }
    const auto& element = elements[step.element_index];
    if (step.kind == ModelSpecialMatrixKind::translate) {
        const ModelMatrix offset = model_local_matrix(
            {-element[0], -element[1], -element[2]}, {0, 0, 0});
        return model_multiply_matrix(base, offset);
    }
    // The callbacks pass the negated element as the 0x45d400 placement, so the
    // effective pivot is T(+element) x R x T(-element).
    const std::array<std::int32_t, 3> placement{
        -element[0], -element[1], -element[2]};
    return model_multiply_matrix(base, model_special_pivot_matrix(placement, step.angles));
}

} // namespace

std::vector<ModelMatrix> model_special_step_matrices(
    const ModelSpecialPlan& plan, const ModelMatrix& object_matrix,
    const ModelMatrix& view,
    const std::vector<std::array<std::int32_t, 3>>& elements) {
    ModelMatrix base = object_matrix;
    if (plan.view_multiply) {
        base = model_multiply_matrix(view, base);
    }
    std::vector<ModelMatrix> result;
    result.reserve(plan.steps.size());
    for (const auto& step : plan.steps) {
        result.push_back(build_step_matrix(step, base, elements));
    }
    return result;
}

std::vector<ModelMatrix> model_special_slot_matrices(
    const ModelSpecialPlan& plan, const ModelMatrix& slot_base,
    const std::vector<std::array<std::int32_t, 3>>& elements) {
    // The matrix callbacks write local matrices: 0x45d1e0 plus the element
    // composition, never the camera multiply. The caller builds slot_base with
    // the same z raise the callback uses (face uses its third argument).
    const ModelMatrix base = slot_base;
    std::vector<ModelMatrix> result;
    result.reserve(plan.slots.size());
    for (const auto& slot : plan.slots) {
        ModelSpecialStep step;
        step.kind = slot.kind;
        step.element_index = slot.element_index;
        step.angles = slot.angles;
        result.push_back(build_step_matrix(step, base, elements));
    }
    return result;
}

} // namespace dfre
