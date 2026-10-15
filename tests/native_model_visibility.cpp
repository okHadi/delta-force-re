#include "dfre/model_visibility.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

// Synthetic frustum: near plane plus four 45-degree side planes.
dfre::ModelViewProjection test_projection() {
    dfre::ModelViewProjection projection;
    projection.focal = 320; // Whole pixels; the original shifts it left by 16.
    projection.centre_x = 320 << 16;
    projection.centre_y = 240 << 16;
    projection.planes[0] = {{4194304, 0, 0}, 4096};
    projection.planes[1] = {{4194304, -4194304, 0}, 0};
    projection.planes[2] = {{4194304, 4194304, 0}, 0};
    projection.planes[3] = {{4194304, 0, 4194304}, 0};
    projection.planes[4] = {{4194304, 0, -4194304}, 0};
    return projection;
}

dfre::ModelViewState test_view() {
    dfre::ModelViewState view;
    view.camera_position = {0, 0, 0};
    view.world_to_camera = dfre::model_identity_matrix();
    view.projection = test_projection();
    return view;
}

void draw_marker_a(const void*, std::uint32_t, const void*, std::uint32_t) {}
void draw_marker_b(const void*, std::uint32_t, const void*, std::uint32_t) {}
} // namespace

int main() {
    using namespace dfre;
    try {
        // 0x46b780: level thresholds. M16 values (100, 50, 10, 0) in 16.16.
        const std::array<std::int32_t, 4> m16{0x640000, 0x320000, 0xa0000, 0};
        check(select_model_level(m16, 0x700000) == 0, "largest radius did not pick the detailed LOD");
        check(select_model_level(m16, 0x640000) == 1, "threshold equality changed");
        check(select_model_level(m16, 0x320001) == 1, "second level threshold changed");
        check(select_model_level(m16, 0xa0001) == 2, "third level threshold changed");
        check(select_model_level(m16, 0x8001) == 3, "smallest drawn radius did not pick the coarse LOD");
        check(!select_model_level(m16, 0x8000), "half-pixel radius was not skipped");
        check(!select_model_level(m16, 0), "zero radius was not skipped");
        check(select_model_level({0x10000, 0x10000, 0x10000, 0x10000}, 0x10001) == 0,
            "descending thresholds changed the first level");
        check(select_model_level({0x640000, 0x640000, 0x640000, 0}, 0x640000) == 3,
            "equal thresholds did not fall through to the last level");

        // 0x46b780 head: primary and secondary model selection.
        const void* model_a = reinterpret_cast<const void*>(0x1000);
        const void* model_b = reinterpret_cast<const void*>(0x2000);
        check(select_object_model({nullptr, model_a, model_b, false}) == model_a,
            "flag without bit 8 did not pick the primary model");
        check(select_object_model({nullptr, model_a, model_b, true}) == model_b,
            "flag with bit 8 did not pick the secondary model");
        check(select_object_model({nullptr, model_a, nullptr, true}) == model_a,
            "missing secondary model did not fall back to the primary");
        check(select_object_model({nullptr, nullptr, nullptr, false}) == nullptr,
            "empty object produced a model");

        // 0x4818f0 / 0x481a00: first matching row, otherwise the first row.
        const ModelCallbackRow callback_rows[2]{{0x676e7263, draw_marker_a, nullptr},
            {0x6f726730, draw_marker_b, nullptr}};
        check(resolve_model_draw_callback(callback_rows, 2, 0x6f726730) == draw_marker_b, "callback row did not match");
        check(resolve_model_draw_callback(callback_rows, 2, 0x11111111) == draw_marker_a, "callback default changed");
        check(resolve_model_draw_callback(callback_rows, 0, 0x6f726730) == nullptr, "empty callback table did not return null");

        // Proven table 0x500c1c: 16 rows, lookup and typed resolvers.
        check(model_callback_tags.size() == 16, "callback tag table size changed");
        check(model_callback_tags[9].key == 0x6f726730 && model_callback_tags[9].draw_address == 0x481330 &&
            model_callback_tags[9].matrix_address == 0x481130, "org0 callback row changed");
        check(model_callback_tags[5].key == 0x676e7263 && model_callback_tags[5].draw_address == 0x481060 &&
            model_callback_tags[5].matrix_address == 0x480ff0, "generic callback row changed");
        for (const auto& tag : model_callback_tags) {
            const auto* found = find_model_callback_tag(tag.key);
            check(found == &tag, "callback tag lookup missed a proven key");
        }
        check(find_model_callback_tag(0x11111111) == nullptr, "unknown callback tag was found");
        const auto proven = model_callback_rows();
        for (std::size_t index = 0; index < proven.size(); ++index) {
            check(resolve_model_draw_callback(proven.data(), proven.size(), model_callback_tags[index].key) ==
                proven[index].draw, "proven draw lookup changed");
            check(resolve_model_matrix_callback(proven.data(), proven.size(), model_callback_tags[index].key) ==
                proven[index].matrix, "proven matrix lookup changed");
        }
        check(std::size_t(resolve_model_draw_callback(proven.data(), proven.size(), 0x11111111)) ==
            std::size_t(proven[0].draw), "proven draw default changed");
        check(std::size_t(resolve_model_matrix_callback(proven.data(), proven.size(), 0x11111111)) ==
            std::size_t(proven[0].matrix), "proven matrix default changed");

        // 0x44acf0: inside, partial and outside sphere cases.
        const auto projection = test_projection();
        auto result = project_model_object(projection, {64 << 16, 0, 0}, 1 << 16);
        check(result.state == ModelProjectionState::projected && result.clip_mask == 0,
            "inside object mask changed");
        check(result.point[0] == 320 << 16 && result.point[1] == 240 << 16 &&
            result.point[2] == (64 << 16), "inside object placement changed");
        check(result.screen_radius == 5 << 16, "inside object screen radius changed");
        result = project_model_object(projection, {64 << 16, 65 << 16, 0}, 1 << 16);
        check(result.state == ModelProjectionState::projected && (result.clip_mask & 4) != 0 &&
            (result.clip_mask & 8) == 0, "side plane mask bits changed");
        result = project_model_object(projection, {64 << 16, 0, 65 << 16}, 1 << 16);
        check(result.state == ModelProjectionState::projected && (result.clip_mask & 0x20) != 0 &&
            (result.clip_mask & 0x10) == 0, "bottom plane mask bits changed");
        result = project_model_object(projection, {64 << 16, 100 << 16, 0}, 1 << 16);
        check(result.state == ModelProjectionState::outside, "object outside a side plane was kept");
        result = project_model_object(projection, {-(2 << 16), 0, 0}, 1 << 16);
        check(result.state == ModelProjectionState::outside, "object behind the near plane was kept");
        result = project_model_object(projection, {(0x1000 + 1020) << 16, 0, 0}, 1 << 16);
        check(result.state == ModelProjectionState::outside, "object beyond the far bound was kept");
        result = project_model_object(projection, {0x8000, 0, 0}, 1 << 16);
        check(result.state == ModelProjectionState::camera_inside_sphere && result.clip_mask == 0x3e &&
            result.screen_radius == 0x4000000, "sphere containing the camera changed");

        // 0x485040: collection and the 100 px queue split.
        const auto view = test_view();
        ModelCollectInput input;
        input.identity = model_a;
        input.model = model_b;
        input.radius = 1 << 16;
        input.position = {64 << 16, 0, 0};
        auto collected = collect_scene_model(input, view);
        check(collected && collected->queue == ModelQueue::near, "near object queue changed");
        check(collected->entry.object == model_a && collected->entry.screen_radius == 5 << 16,
            "near object radius changed");
        check(collected->entry.screen_x == 320 && collected->entry.screen_y == 240 &&
            collected->entry.camera_depth == 64 << 16, "near object screen placement changed");
        input.position = {64 << 16, 0x3fd << 16, 0};
        check(!collect_scene_model(input, view), "camera-axis prefilter did not reject");
        input.position = {0x3fc << 16, 0, 0};
        check(!collect_scene_model(input, view), "far depth filter did not reject");
        input.hidden = true;
        input.position = {64 << 16, 0, 0};
        check(!collect_scene_model(input, view), "hidden object was collected");
        input.hidden = false;
        input.radius = 32 << 16;
        collected = collect_scene_model(input, view);
        check(collected && collected->queue == ModelQueue::large, "large radius object queue changed");
        check(collected->entry.screen_x == 0 && collected->entry.camera_depth == 0,
            "large queue kept projection fields");
        input.radius = 1 << 16;
        auto character = collect_character_model(input, view);
        check(character && character->camera_depth == 64 << 16 && character->screen_x == 320,
            "character collection changed");
        auto effect = collect_effect_model(input, view);
        check(effect && effect->screen_radius == 5 << 16, "effect collection changed");
        input.hidden = true;
        effect = collect_effect_model(input, view);
        check(effect && effect->screen_radius == 5 << 16, "effect collection must ignore the hidden flag");

        // 0x484fb0: row table visibility and the 0x4854d0 cull pass.
        std::vector<std::int32_t> row_keys(480, 0x7fffffff);
        std::vector<std::vector<std::uint16_t>> rows(480, std::vector<std::uint16_t>(640, 0xffff));
        std::vector<const std::uint16_t*> row_pointers;
        for (const auto& row : rows) row_pointers.push_back(row.data());
        const ModelRowDepthTable table{row_keys.data(), row_pointers.data()};
        const ModelScreenBounds bounds{0, 0, 639, 479};
        ModelQueueEntry entry;
        entry.object = model_a;
        entry.screen_radius = 2 << 16;
        entry.screen_x = 320;
        entry.screen_y = 240;
        entry.camera_depth = 64 << 16;
        ModelQueueList queue{entry};
        check(cull_model_queue(queue, bounds, table) == 1, "visible entry was culled");
        row_keys[238] = 0;
        check(cull_model_queue(queue, bounds, table) == 0 && queue[0].object == nullptr,
            "row depth key did not cull");
        row_keys[238] = 0x7fffffff;
        rows[238].assign(640, 0);
        queue = {entry};
        check(cull_model_queue(queue, bounds, table) == 0, "zero row words did not cull");
        rows[238].assign(640, 0xffff);
        queue = {entry};
        check(cull_model_queue(queue, bounds, table, model_a, false) == 0 && queue[0].object == nullptr,
            "suppressed object was kept");
        queue = {entry};
        check(cull_model_queue(queue, bounds, table, model_a, true) == 1, "allowed object was suppressed");
        entry.screen_y = 476;
        entry.screen_radius = 8 << 16;
        queue = {entry};
        row_keys.assign(480, 0x7fffffff);
        for (auto& row : rows) row.assign(640, 0xffff);
        check(cull_model_queue(queue, bounds, table) == 1, "clamped row range did not stay visible");

        // 0x46a930: mask to plane choice.
        check(!model_needs_face_clipping(0) && !model_needs_face_clipping(0x40) &&
            model_needs_face_clipping(2), "face clipping gate changed");
        const auto selected = model_clip_planes_for_mask(0x3e, projection.planes);
        check(selected.size() == 5 && selected[0].offset == 4096 && selected[1].normal[1] == -4194304 &&
            selected[2].normal[1] == 4194304 && selected[3].normal[2] == -4194304 &&
            selected[4].normal[2] == 4194304, "clip plane order changed");
        check(model_clip_planes_for_mask(4, projection.planes).size() == 1,
            "single clip mask bit selected the wrong plane count");

        // 0x46b780 dispatch and 0x485580 pass.
        ModelRuntimeModel runtime;
        runtime.lod_thresholds = m16;
        runtime.lod_draw = {draw_marker_a, draw_marker_b, draw_marker_a, draw_marker_b};
        runtime.lod_data = {model_a, model_b, model_a, model_b};
        ModelObjectView object;
        object.identity = model_a;
        object.primary = model_b;
        const auto call = select_model_draw(runtime, object, 0x2e, 0x640001);
        check(call && call->level == 0 && call->level_data == model_a && call->callback == draw_marker_a &&
            call->clip_mask == 0x2e && call->object == model_a, "level dispatch changed");
        check(!select_model_draw(runtime, object, 0, 0x8000), "floor radius was dispatched");
        runtime.lod_data[3] = nullptr;
        check(!select_model_draw(runtime, object, 0, 0x8001), "missing level data was dispatched");
        runtime.lod_data[3] = model_b;
        ModelQueueList pass_entries{{nullptr, 0, 0, 0, 0, 0}, {model_a, 0x640001, 0x2e, 0, 0, 0},
            {model_a, 0x8000, 0, 0, 0, 0}};
        std::map<const void*, ModelObjectView> objects{{model_a, object}};
        const ModelPassAccessors accessors{
            [&](const void* pointer) { return objects.at(pointer); },
            [&](const void*) -> const ModelRuntimeModel* { return &runtime; }};
        const auto calls = run_model_draw_pass(pass_entries, accessors);
        check(calls.size() == 1 && calls[0].level == 0 && calls[0].level_data == model_a,
            "draw pass call sequence changed");
        std::cout << "model visibility tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "model visibility test failure: " << error.what() << '\n';
        return 1;
    }
}
