#include "dfre/model_specials.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

// Silent test: only data rules run here. No device is opened and nothing is
// drawn. Golden values come from the original Df.exe callbacks emulated by
// tools/check_model_specials_original.py.

namespace {

bool require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

dfre::ModelSpecialInstance instance_with(std::int32_t mesh_count) {
    dfre::ModelSpecialInstance instance;
    instance.mesh_count = mesh_count;
    instance.mesh_base = 0x40000000u;
    instance.element_base = 0x40000000u;
    instance.divisor_58 = 0x10000;
    instance.flag_5a = 1;
    instance.threshold_40 = 0x10000000;
    return instance;
}

dfre::ModelSpecialObject object_with() {
    dfre::ModelSpecialObject object;
    object.distance_fc = 0x2000;
    object.value_98 = 0x08000000;
    object.value_a0 = 0x10000000;
    object.value_e0 = 2;
    object.angle_9c = 0x00200000;
    object.angle_a4 = 0x00400000;
    object.angle_c4 = 0x00100000;
    object.angle_c8 = 0x00300000;
    object.value_94 = 0x400000;
    object.scale_7c = 0x180000;
    object.z_30 = 0x3000;
    object.z_high = 0;
    return object;
}

const dfre::ModelSpecialDef* def_named(const std::string& name) {
    const dfre::ModelSpecialDef* defs = dfre::model_special_defs();
    for (std::size_t index = 0; index < dfre::model_special_def_count(); ++index) {
        if (name == defs[index].name) {
            return &defs[index];
        }
    }
    return nullptr;
}

} // namespace

int main() {
    // Callback table rows owned by this module (0x500c1c, 16 rows total).
    if (!require(dfre::model_special_def_count() == 14,
                 "Special table must hold the 14 owned rows.")) return 1;
    if (!require(dfre::find_model_special_def(0x0u) == nullptr,
                 "Unknown keys must not resolve.")) return 1;
    const struct {
        std::uint32_t key;
        const char* name;
        std::uint32_t matrix;
        std::uint32_t draw;
    } rows[] = {
        {0x62697264u, "drib", 0x44ce80u, 0x44cef0u},
        {0x65666374u, "tcfe", 0x42a600u, 0x42a610u},
        {0x65663364u, "d3fe", 0x480e70u, 0x480e80u},
        {0x66616365u, "ecaf", 0x480f40u, 0x480f80u},
        {0x66697368u, "hsif", 0x44c860u, 0x44c8f0u},
        {0x68656c6fu, "oleh", 0x44c4c0u, 0x44c570u},
        {0x6c616d70u, "pmal", 0x44d0d0u, 0x44d150u},
        {0x6e756c6cu, "llun", 0x481110u, 0x481120u},
        {0x70616c6du, "mlap", 0x44d2a0u, 0x44d5e0u},
        {0x70656e67u, "gnep", 0x44d990u, 0x44da20u},
        {0x70777030u, "0pwp", 0x481650u, 0x481660u},
        {0x7368726bu, "krhs", 0x44cdf0u, 0x44ce00u},
        {0x74697265u, "erit", 0x42a170u, 0x42a290u},
        {0x74726565u, "eert", 0x4816d0u, 0x4817a0u},
    };
    for (const auto& row : rows) {
        const dfre::ModelSpecialDef* def = dfre::find_model_special_def(row.key);
        if (!require(def != nullptr, "Row key must resolve.")) return 1;
        if (!require(std::string(def->name) == row.name, "Row name failed.")) return 1;
        if (!require(def->matrix_address == row.matrix && def->draw_address == row.draw,
                     "Row addresses failed.")) return 1;
    }
    if (!require(dfre::find_model_special_def(0x6f726730u) == nullptr,
                 "org0 belongs to another module.")) return 1;
    if (!require(dfre::find_model_special_def(0x676e7263u) == nullptr,
                 "gnrc belongs to another module.")) return 1;

    // Plan shapes verified by replay (mesh offsets per tag).
    const struct {
        const char* tag;
        std::int32_t meshes;
        std::vector<std::uint32_t> offsets;
        bool view_multiply;
    } shapes[] = {
        {"0pwp", 3, {0x00, 0x70, 0xe0}, false},
        {"krhs", 2, {0x00, 0x70}, true},
        {"d3fe", 3, {0x00, 0x70, 0xe0}, true},
        {"tcfe", 3, {0x00}, true},
        {"ecaf", 3, {0x00}, true},
        {"eert", 3, {0x00, 0x70, 0xe0}, true},
        {"drib", 1, {0x00, 0x70, 0xe0}, true},
        {"erit", 1, {0x00, 0xe0, 0x150}, true},
        {"hsif", 6, {0x00, 0x70, 0xe0, 0x150, 0x1c0, 0x230}, true},
        {"gnep", 6, {0x00, 0x70, 0xe0, 0x150, 0x1c0, 0x230}, true},
        {"llun", 3, {}, false},
        {"pmal", 3, {0x00, 0x70, 0xe0}, true},
        {"oleh", 1, {0x00, 0x70, 0x1c0, 0x230, 0x2a0}, true},
        {"mlap", 3, {0x00, 0x70, 0x230}, true},
    };
    for (const auto& shape : shapes) {
        const dfre::ModelSpecialDef* def = def_named(shape.tag);
        if (!require(def != nullptr, "Shape tag must exist.")) return 1;
        const dfre::ModelSpecialPlan plan = dfre::build_model_special_plan(
            def->tag, instance_with(shape.meshes), object_with());
        if (!require(plan.steps.size() == shape.offsets.size(),
                     "Plan step count differs from the original replay.")) return 1;
        for (std::size_t index = 0; index < shape.offsets.size(); ++index) {
            if (!require(plan.steps[index].mesh_offset == shape.offsets[index],
                         "Plan mesh offset differs from the original replay.")) return 1;
        }
        if (!require(plan.view_multiply == shape.view_multiply,
                     "Plan view multiply differs from the original replay.")) return 1;
    }

    // Palm mesh choice is a selector on object +0xe0 (0x44d5e0).
    const struct {
        std::int32_t selector;
        std::size_t steps;
        dfre::ModelSpecialMatrixKind kind;
    } palm[] = {
        {0, 3, dfre::ModelSpecialMatrixKind::base},
        {1, 2, dfre::ModelSpecialMatrixKind::base},
        {2, 3, dfre::ModelSpecialMatrixKind::base},
        {0x10, 3, dfre::ModelSpecialMatrixKind::translate},
        {0x11, 1, dfre::ModelSpecialMatrixKind::translate},
        {0x20, 2, dfre::ModelSpecialMatrixKind::translate},
        {3, 0, dfre::ModelSpecialMatrixKind::base},
    };
    for (const auto& item : palm) {
        dfre::ModelSpecialObject object = object_with();
        object.value_e0 = item.selector;
        const dfre::ModelSpecialPlan plan = dfre::build_model_special_plan(
            dfre::ModelSpecialTag::palm, instance_with(4), object);
        if (!require(plan.steps.size() == item.steps, "Palm step count failed.")) return 1;
        if (item.steps > 0 &&
            !require(plan.steps[0].kind == item.kind, "Palm step kind failed.")) return 1;
    }

    // Pivot transform is T(-p) x R x T(p); identity when the angles are zero.
    const std::array<std::int32_t, 3> pivot{0x1000, 0x2000, 0x3000};
    const dfre::ModelMatrix identity = dfre::model_special_pivot_matrix(pivot, {0, 0, 0});
    const auto minimum = std::numeric_limits<std::int32_t>::min();
    if (!require(dfre::model_special_pivot_matrix({minimum, minimum, minimum}, {0, 0, 0}) == identity,
                 "Minimum signed pivot must retain the identity transform")) return 1;
    const dfre::ModelMatrix want_identity = dfre::model_identity_matrix();
    if (!require(identity == want_identity,
                 "Zero angles must produce the identity pivot.")) return 1;
    const dfre::ModelMatrix rotated = dfre::model_special_pivot_matrix(pivot, {0, 0, 0x400000});
    if (!require(rotated != want_identity, "A rotation must change the pivot.")) return 1;
    // Pivot 0 with no angles is a pure identity as well.
    if (!require(dfre::model_special_pivot_matrix({0, 0, 0}, {0, 0, 0}) == want_identity,
                 "Origin pivot must be identity.")) return 1;

    // Counter rule (0x42a610 / 0x480e80), golden values from the original.
    {
        dfre::ModelSpecialInstance instance = instance_with(1);
        dfre::ModelSpecialObject object = object_with();
        object.distance_fc = 100;
        instance.flag_5a = 0;
        if (!require(dfre::model_special_counter(instance, object) == 0,
                     "Counter is zero when the flag is clear.")) return 1;
        instance.flag_5a = 1;
        instance.threshold_40 = 16777216;   // >> 16 = 256 > 100
        if (!require(dfre::model_special_counter(instance, object) == 100,
                     "Counter divides by the 16.16 divisor.")) return 1;
        instance.threshold_40 = 0;
        if (!require(dfre::model_special_counter(instance, object) == 0,
                     "Counter resets inside the threshold.")) return 1;
        instance.divisor_58 = 0;
        if (!require(dfre::model_special_counter(instance, object) == 0,
                     "Zero divisor must not divide.")) return 1;
    }

    // Fish and penguins now emit their full replay-verified plans.
    for (const auto tag : {dfre::ModelSpecialTag::fish, dfre::ModelSpecialTag::peng}) {
        const dfre::ModelSpecialPlan plan = dfre::build_model_special_plan(
            tag, instance_with(4), object_with());
        if (!require(plan.steps.size() == 6, "Animated tag step count failed.")) return 1;
        if (!require(plan.steps[0].kind == dfre::ModelSpecialMatrixKind::base,
                     "Animated tag must start from the base matrix.")) return 1;
        if (!require(plan.slots.size() == 4, "Animated tag slot count failed.")) return 1;
    }
    // The penguin leans placement[3] before building the base.
    {
        dfre::ModelSpecialObject object = object_with();
        object.angle_e4 = 0x00500000;
        object.mode_f0 = 0;
        const dfre::ModelSpecialPlan plan = dfre::build_model_special_plan(
            dfre::ModelSpecialTag::peng, instance_with(6), object);
        if (!require(plan.placement_angle_offset == 0x6000000,
                     "Penguin lean must clamp to 0x600000 * -0x10.")) return 1;
    }
    // Fish drops the object phase byte twice (0x44c8f0).
    {
        dfre::ModelSpecialObject object = object_with();
        object.phase_33 = 0x40;
        const dfre::ModelSpecialPlan plan = dfre::build_model_special_plan(
            dfre::ModelSpecialTag::fish, instance_with(6), object);
        if (!require(plan.writes.size() == 2, "Fish must write the phase twice.")) return 1;
        if (!require(plan.writes[0].value == 0xC0 && plan.writes[1].value == 0x40,
                     "Fish phase bytes must wrap as signed bytes.")) return 1;
        if (!require(!plan.writes[0].restore && plan.writes[1].restore,
                     "Fish phase restore marker failed.")) return 1;
    }
    // Both tire element pivots keep the same angle.
    {
        const dfre::ModelSpecialPlan plan = dfre::build_model_special_plan(
            dfre::ModelSpecialTag::tire, instance_with(3), object_with());
        if (!require(plan.steps.size() == 3, "Tire step count failed.")) return 1;
        if (!require(plan.steps[1].kind == dfre::ModelSpecialMatrixKind::pivot &&
                     plan.steps[2].kind == dfre::ModelSpecialMatrixKind::pivot,
                     "Tire step kinds failed.")) return 1;
        if (!require(plan.slots.size() == 3, "Tire slot count failed.")) return 1;
        if (!require(plan.localized_slots == std::vector<std::uint32_t>{1},
                     "Tire localized slot must be 1.")) return 1;
    }
    // Localized slot sets verified by the replay for the copy-style tags.
    struct SlotCase {
        dfre::ModelSpecialTag tag;
        std::vector<std::uint32_t> slots;
    };
    const SlotCase slot_cases[] = {
        {dfre::ModelSpecialTag::lamp, {2}},
        {dfre::ModelSpecialTag::fish, {2, 3, 4, 5}},
        {dfre::ModelSpecialTag::peng, {1, 2, 3, 4, 5}},
        {dfre::ModelSpecialTag::bird, {}},
        {dfre::ModelSpecialTag::tree, {}},
    };
    for (const auto& item : slot_cases) {
        const dfre::ModelSpecialPlan plan = dfre::build_model_special_plan(
            item.tag, instance_with(6), object_with());
        if (!require(plan.localized_slots == item.slots,
                     "Localized slot set failed.")) return 1;
    }
    // llun draws nothing and has no matrix callback output.
    {
        const dfre::ModelSpecialPlan plan = dfre::build_model_special_plan(
            dfre::ModelSpecialTag::llun, instance_with(3), object_with());
        if (!require(!plan.draws && plan.steps.empty() && plan.slots.empty(),
                     "llun must stay empty.")) return 1;
    }

    return 0;
}
