#include "dfre/model_instances.hpp"
#include "dfre/renderer.hpp"
#include <iostream>
#include <stdexcept>

// Native checks for the mesh-instance module. Stock fixture values are from the
// shipped Df.pff members (captured 2026-10-16) so the tests also pin real data:
//   M16.3DI     crng, thresholds 100/50/10/0 px, LOD0 mesh 2, count 3, duration 5
//   AK47.3DI    crng, thresholds 0, LOD0 mesh 2 count 3 duration 5, LOD1 duration 0
//   BARRET.3DI  crng, LOD3 count 0 duration 0 (inactive counter)
//   FIR2.3DI    crng, thresholds 100/25/0/0, mesh counts 1/2/1 (fill quirk)
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

dfre::ModelMatrix test_view() {
    dfre::ModelMatrix view = dfre::model_identity_matrix();
    view[3] = 12345;
    view[7] = -6789;
    view[11] = 24680;
    return view;
}
} // namespace

int main() {
    using namespace dfre;
    try {
        // 0x45d1e0 is exactly model_local_matrix for the object +0x24 block.
        const std::array<ModelInstanceFields, 4> fields{{
            {{0, 0, 0}, {0, 0, 0}},
            {{0x1234, -0x2345, 0x400000}, {0x40000000, 0x20000000, 0x7654321}},
            {{-0x4000000, 0x4000000, 0x7fffffff}, {0xffffffff, 0x1, 0x8000000}},
            {{123, -456, 789}, {0, 0x200000, 0}},
        }};
        for (const auto& item : fields) {
            check(model_instance_local_matrix(item) ==
                model_local_matrix(item.translation, item.angles),
                "0x45d1e0 wrapper differs from model_local_matrix");
        }
        check(model_instance_local_matrix(fields[0]) == model_identity_matrix(),
            "zero instance fields did not give the identity matrix");
        const auto view = test_view();
        for (const auto& item : fields) {
            check(model_instance_camera_matrix(item, view) ==
                model_multiply_matrix(view, model_instance_local_matrix(item)),
                "0x45cb40 camera product differs from model_multiply_matrix");
        }
        check(model_instance_camera_matrix(fields[0], view) == view,
            "identity instance did not keep the view matrix");

        // 0x45d2f0: scale 0x10000 is 1.0 (the matrix base becomes identity), and
        // the zero-angle case is exactly translation x diag(scale << 6).
        for (const auto& item : fields) {
            check(model_instance_local_matrix_scaled(item, 0x10000) ==
                model_instance_local_matrix(item),
                "unit scale changed the local matrix");
        }
        ModelMatrix expected = model_local_matrix({12, -34, 56}, {0, 0, 0});
        expected[0] = 0x200000;
        expected[5] = 0x200000;
        expected[10] = 0x200000;
        check(model_instance_local_matrix_scaled({{12, -34, 56}, {0, 0, 0}}, 0x8000) == expected,
            "scale matrix base changed");

        // 0x481060 frame rule with the stock M16 values: (time / 5) % 3.
        ModelInstanceLod m16; // M16.3DI LOD0.
        m16.mesh_count = 2;
        m16.frame_count_raw = 0x30000;
        m16.frame_duration_raw = 0x50000;
        check(model_instance_animation_frame(0, m16) == 0, "frame 0 changed");
        check(model_instance_animation_frame(5, m16) == 1, "frame 1 changed");
        check(model_instance_animation_frame(14, m16) == 2, "frame 2 changed");
        check(model_instance_animation_frame(15, m16) == 0, "frame wrap changed");
        check(model_instance_animation_frame(1234567, m16) == 1234567 / 5 % 3, "integer frame changed");
        ModelInstanceLod ak47; // AK47.3DI LOD1: duration 0 keeps the previous global.
        ak47.mesh_count = 2;
        ak47.frame_count_raw = 0x30000;
        ak47.frame_duration_raw = 0;
        check(!model_instance_animation_frame(1234567, ak47), "zero duration produced a frame");
        ModelInstanceLod baret; // BARRET.3DI LOD3: count 0 and duration 0.
        baret.mesh_count = 1;
        check(!model_instance_animation_frame(99, baret), "inactive BARRET record produced a frame");
        ModelInstanceLod broken;
        broken.mesh_count = 1;
        broken.frame_count_raw = 0;
        broken.frame_duration_raw = 0x50000;
        try {
            model_instance_animation_frame(99, broken);
            check(false, "zero frame count with a duration was accepted");
        } catch (const std::invalid_argument&) {}
        ModelInstanceLod negative;
        negative.mesh_count = 1;
        negative.frame_count_raw = -1;
        negative.frame_duration_raw = 0x80000000; // entire unsigned divisor
        check(model_instance_animation_frame(0xffffffffu, negative) == 0,
            "negative duration divisor changed");
        ModelInstanceLod fractional;
        fractional.mesh_count = 1;
        fractional.frame_count_raw = 0x2abcd;
        fractional.frame_duration_raw = 0x3ffff;
        check(model_instance_animation_frame(0xffffff, fractional) == 1, "fractional words changed the whole parts");
        ModelInstanceLod overflow;
        overflow.mesh_count = 1;
        overflow.frame_count_raw = -1;
        overflow.frame_duration_raw = 0x10000;
        try {
            model_instance_animation_frame(0x80000000u, overflow);
            check(false, "signed divide overflow was accepted");
        } catch (const std::invalid_argument&) {}
        check(model_instance_animation_frame(5, overflow) == 0, "5 % -1 changed");
        check(model_instance_animation_frame(0x80000001u, overflow) == 0, "INT_MIN+1 % -1 changed");

        // 0x480ff0 slots: slot 0 is the local matrix and every later slot copies it;
        // slot 0 exists even for a zero mesh count.
        const auto slots = model_instance_local_matrices(3, fields[1]);
        check(slots.size() == 3, "callback slot count changed");
        for (const auto& slot : slots) check(slot == model_instance_local_matrix(fields[1]),
            "callback slot differs from the local matrix");
        check(model_instance_local_matrices(0, fields[1]).size() == 1,
            "zero mesh count did not keep slot 0");

        // 0x481060 draw decision: camera matrix for every mesh, frame from the counter.
        const auto draw = build_model_instance_draw(m16, fields[0], view, 7);
        check(draw.frame == 1 && draw.mesh_count == 2 && draw.matrices.size() == 2,
            "generic draw record changed");
        check(draw.local == model_instance_local_matrix(fields[0]) &&
            draw.camera == model_instance_camera_matrix(fields[0], view),
            "generic draw matrices changed");
        for (const auto& matrix : draw.matrices) check(matrix == draw.camera,
            "generic draw matrix slot differs from the camera matrix");
        const auto inactive = build_model_instance_draw(ak47, fields[0], view, 7);
        check(!inactive.frame && inactive.matrices.size() == 2, "inactive generic draw changed");
        ModelInstanceLod empty_lod;
        const auto empty = build_model_instance_draw(empty_lod, fields[0], view, 7);
        check(!empty.frame && empty.mesh_count == 0 && empty.matrices.empty(),
            "empty generic container changed");

        // 0x46b830 static draw. M16 thresholds select slot 0 for radius 0x640001
        // and slot 2 for radius 0xa0001.
        ModelStaticRecord m16_record;
        m16_record.lod_thresholds = {0x640000, 0x320000, 0xa0000, 0};
        m16_record.lods = {std::optional<ModelInstanceLod>{m16},
                           std::optional<ModelInstanceLod>{m16},
                           std::optional<ModelInstanceLod>{m16},
                           std::optional<ModelInstanceLod>{}};
        ModelStaticObject static_m16;
        static_m16.primary = m16_record;
        auto result = build_model_static_draw(static_m16, fields[0], view, 0x640001);
        check(result.has_model && result.lod == 0 && result.matrix_slots == 2 &&
            result.matrices.size() == 2 && result.mesh_indices == std::vector<std::size_t>{0, 1},
            "static slot 0 draw changed");
        check(result.local == model_instance_local_matrix(fields[0]) &&
            result.camera == model_instance_camera_matrix(fields[0], view),
            "static matrices changed");
        result = build_model_static_draw(static_m16, fields[0], view, 0xa0001);
        check(result.lod == 2 && result.matrices.size() == 2, "static slot 2 draw changed");
        result = build_model_static_draw(static_m16, fields[0], view, 0x8000);
        check(result.has_model && !result.lod && result.matrices.empty(),
            "floor radius did not skip the draw but keep the matrices");
        result = build_model_static_draw(static_m16, fields[0], view, 0x640000);
        check(result.lod == 1, "threshold equality did not fall through to the next level");

        // 0x46b830 head: bit 2 picks the secondary only when it exists.
        ModelStaticRecord short_record;
        short_record.lod_thresholds = {0x10000, 0, 0, 0};
        short_record.lods[0] = ModelInstanceLod{3, 0, 0};
        ModelStaticRecord long_record = short_record;
        long_record.lods[0] = ModelInstanceLod{5, 0, 0};
        ModelStaticObject choose;
        choose.primary = short_record;
        choose.secondary = long_record;
        choose.use_secondary = true;
        result = build_model_static_draw(choose, fields[0], view, 0x10001);
        check(result.lod == 0 && result.matrix_slots == 5, "secondary record was not used");
        choose.use_secondary = false;
        result = build_model_static_draw(choose, fields[0], view, 0x10001);
        check(result.lod == 0 && result.matrix_slots == 3, "flag without bit 2 did not keep the primary");
        choose.use_secondary = true;
        choose.secondary.reset();
        result = build_model_static_draw(choose, fields[0], view, 0x10001);
        check(result.lod == 0 && result.matrix_slots == 3, "missing secondary did not fall back");
        ModelStaticObject absent;
        check(!build_model_static_draw(absent, fields[0], view, 0x10001).has_model,
            "empty object produced a model");
        absent.use_secondary = true;
        check(!build_model_static_draw(absent, fields[0], view, 0x10001).has_model,
            "flag without either record produced a model");

        // Fill quirk and null records, from the stock FIR2.3DI shape: the fill
        // count always comes from LOD 0, the draw count from the selected LOD.
        ModelStaticRecord fir2_record;
        fir2_record.lod_thresholds = {0x640000, 0x190000, 0, 0};
        fir2_record.lods[0] = ModelInstanceLod{1, 0, 0};
        fir2_record.lods[1] = ModelInstanceLod{2, 0, 0};
        fir2_record.lods[2] = ModelInstanceLod{1, 0, 0};
        ModelStaticObject fir2;
        fir2.primary = fir2_record;
        result = build_model_static_draw(fir2, fields[0], view, 0x190001);
        check(result.lod == 1 && result.matrix_slots == 1 && result.matrices.size() == 2 &&
            result.mesh_indices == std::vector<std::size_t>{0, 1},
            "FIR2 fill quirk changed");
        ModelStaticObject null_first = fir2;
        null_first.primary->lods[0] = std::nullopt;
        try {
            build_model_static_draw(null_first, fields[0], view, 0x190001);
            check(false, "null first record fill was accepted");
        } catch (const std::invalid_argument&) {}
        ModelStaticObject null_selected;
        ModelStaticRecord null_record;
        null_record.lod_thresholds = {0x10000, 0, 0, 0};
        null_record.lods[0] = ModelInstanceLod{2, 0, 0};
        null_selected.primary = null_record;
        try {
            build_model_static_draw(null_selected, fields[0], view, 0x8001);
            check(false, "null selected record was accepted");
        } catch (const std::invalid_argument&) {}
        result = build_model_static_draw(null_selected, fields[0], view, 0x8000);
        check(result.has_model && !result.lod, "null selected record changed the floor radius path");

        // The module output feeds the existing mesh draw: one camera matrix per
        // mesh lets draw_model_mesh32 render a placed instance.
        ModelLod lod;
        lod.vertices = {{{256, 128, 128}, 0}, {{256, 128, -128}, 0}, {{256, -128, 128}, 0}};
        lod.normals = {{{0, 0, 0}, 0}};
        ModelFace face{};
        face.vertices = {0, 1, 2};
        lod.faces = {face};
        ModelMesh mesh;
        mesh.vertex_count = 3;
        mesh.normal_count = 1;
        mesh.face_count = 1;
        lod.meshes = {mesh};
        ModelMaterial material;
        material.flags = 0x21;
        material.raw[61] = 0x18;
        lod.materials = {material};
        RasterTexture32 texture;
        texture.pixels = {7};
        texture.shade_colours.resize(64 * 256);
        texture.shade_colours[3 * 256 + 7] = 0x12345678;
        texture.uniform_shade = 65536;
        RasterMaterial32 binding{&texture, nullptr, 0, 0xabcdef12};
        const auto mesh_camera = make_model_camera({}, {}, {0, 0, 7, 7}, 90);
        ModelInstanceLod placed;
        placed.mesh_count = 1;
        const auto placed_draw = build_model_instance_draw(placed, fields[0], model_identity_matrix(), 0);
        std::vector<std::uint32_t> colours(64, 0);
        std::vector<std::uint16_t> depths(64, 65535);
        check(draw_model_mesh32(lod, 0, placed_draw.matrices, mesh_camera, {}, {binding}, 8, 8,
              colours, depths) > 0 && colours[27] == 0x12345678,
            "placed instance mesh draw lost its geometry or shade");
        const auto static_result = build_model_static_draw(fir2, fields[0], model_identity_matrix(), 0x190001);
        check(static_result.matrices.size() >= 2, "static matrices do not cover the drawn mesh count");

        std::cout << "model instances tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "model instances test failure: " << error.what() << '\n';
        return 1;
    }
}
