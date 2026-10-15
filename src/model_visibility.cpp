#include "dfre/model_visibility.hpp"
#include <cstdlib>
#include <stdexcept>
#include <utility>

// Original routines reproduced here:
//   0x485040 fill one model queue record for a generic object
//   0x485290 scene model collection (second loop over 0x9f3098 records)
//   0x485420 large-radius model pass
//   0x4854d0 cull pass over the near and character lists
//   0x485580 draw pass over the near and character lists
//   0x484fb0 row-table visibility test
//   0x44acf0 sphere projection and clip mask
//   0x46b780 object model and LOD level selection
//   0x4818f0 / 0x481a00 callback row lookup
//   0x46a930 clip plane choice for one mask
namespace dfre {
namespace {

std::int32_t signed_shift(std::int32_t value, unsigned shift) {
    return std::int32_t(value >> shift);
}

// 64-bit product shifted right with the original shrd truncation.
std::int32_t shifted_product(std::int32_t left, std::int32_t right, unsigned shift) {
    const auto product = (std::int64_t(left) * std::int64_t(right)) >> shift;
    return std::int32_t(std::uint32_t(std::uint64_t(product)));
}

std::int32_t wrapped_add(std::int32_t left, std::int32_t right) {
    return std::int32_t(std::uint32_t(std::uint32_t(left) + std::uint32_t(right)));
}

std::int32_t wrapped_sub(std::int32_t left, std::int32_t right) {
    return std::int32_t(std::uint32_t(std::uint32_t(left) - std::uint32_t(right)));
}

std::int16_t low_word(std::int32_t value) {
    return std::int16_t(std::uint16_t(std::uint32_t(value)));
}

// 0x485040 head: hidden flag, model, and the 0x3fc camera-axis prefilter.
bool collect_prefilter(const ModelCollectInput& input, const ModelViewState& view) {
    if (input.model == nullptr) return false;
    if (input.hidden) return false;
    const auto camera_x = signed_shift(view.camera_position[0], 16);
    const auto camera_y = signed_shift(view.camera_position[1], 16);
    const auto distance_x = std::abs(wrapped_sub(signed_shift(input.position[0], 16), camera_x));
    if (distance_x > model_view_world_limit) return false;
    const auto distance_y = std::abs(wrapped_sub(signed_shift(input.position[1], 16), camera_y));
    if (distance_y > model_view_world_limit) return false;
    return true;
}

// 0x485040 middle: transform, 0x3fc depth filter, sphere projection.
std::optional<ModelProjectionResult> collect_project(const ModelCollectInput& input,
    const ModelViewState& view) {
    const auto camera_point = model_transform_point(view.world_to_camera, input.position);
    if (std::abs(signed_shift(camera_point[0], 16)) >= model_view_world_limit) return {};
    const auto result = project_model_object(view.projection, camera_point, input.radius);
    if (result.state == ModelProjectionState::outside) return {};
    return result;
}

ModelQueueEntry make_entry(const ModelCollectInput& input, const ModelProjectionResult& result) {
    ModelQueueEntry entry;
    entry.object = input.identity;
    entry.screen_radius = result.screen_radius;
    entry.clip_mask = result.clip_mask;
    entry.screen_x = low_word(signed_shift(result.point[0], 16));
    entry.screen_y = low_word(signed_shift(result.point[1], 16));
    entry.camera_depth = result.point[2];
    return entry;
}

} // namespace

bool model_row_visible(const ModelQueueEntry& entry, const ModelScreenBounds& bounds,
    const ModelRowDepthTable& table) {
    if (table.row_keys == nullptr || table.row_words == nullptr) {
        throw std::invalid_argument("model row table is missing");
    }
    const auto radius = signed_shift(entry.screen_radius, 16);
    const auto screen_x = entry.screen_x;
    const auto screen_y = entry.screen_y;
    // 0x484fb0 works on the packed 16-bit fields: y reads +0x10, x reads +0x0e.
    auto row = wrapped_sub(screen_y, radius);
    if (row < bounds.top) row = bounds.top;
    if (row > bounds.bottom) return false;
    if (table.row_keys[row] <= entry.camera_depth) return false;
    auto first = wrapped_sub(screen_x, radius);
    if (first < bounds.left) first = bounds.left;
    auto last = wrapped_add(screen_x, radius);
    if (last > bounds.right) last = bounds.right;
    if (last < first) return false;
    const auto depth = low_word(signed_shift(entry.camera_depth, 10));
    const auto* words = table.row_words[row];
    if (words == nullptr) return false;
    for (auto column = first; column <= last; ++column) {
        if (std::uint16_t(depth) <= words[column]) return true;
    }
    return false;
}

ModelProjectionResult project_model_object(const ModelViewProjection& projection,
    const std::array<std::int32_t, 3>& camera_point, std::int32_t radius) {
    ModelProjectionResult result;
    result.point = camera_point;
    const auto depth = camera_point[0];
    const auto outside = [&] {
        result.state = ModelProjectionState::outside;
        return result;
    };
    // Plane 0 (near): only the first component and the offset apply.
    auto near_distance = wrapped_sub(shifted_product(projection.planes[0].normal[0], depth, 22),
        projection.planes[0].offset);
    if (near_distance < wrapped_sub(0, radius)) return outside();
    if (near_distance <= radius) result.clip_mask |= 2;
    if (wrapped_add(near_distance, radius) > model_view_far_depth) return outside();
    // Planes 1 and 2 use the x and y components; the original does not subtract their offsets.
    for (std::size_t index = 1; index <= 2; ++index) {
        const auto& plane = projection.planes[index];
        const auto distance = wrapped_add(shifted_product(plane.normal[0], depth, 22),
            shifted_product(plane.normal[1], camera_point[1], 22));
        if (distance < wrapped_sub(0, radius)) return outside();
        if (distance <= radius) result.clip_mask |= (index == 1 ? 4u : 8u);
    }
    // Planes 3 and 4 use the x and z components.
    for (std::size_t index = 3; index <= 4; ++index) {
        const auto& plane = projection.planes[index];
        const auto distance = wrapped_add(shifted_product(plane.normal[0], depth, 22),
            shifted_product(plane.normal[2], camera_point[2], 22));
        if (distance < wrapped_sub(0, radius)) return outside();
        if (distance <= radius) result.clip_mask |= (index == 3 ? 0x10u : 0x20u);
    }
    if (depth < radius) {
        result.state = ModelProjectionState::camera_inside_sphere;
        result.clip_mask = 0x3e;
        result.screen_radius = 0x4000000;
        return result;
    }
    if (depth == 0 || std::abs(std::int64_t(depth)) < 2) {
        // The original signed division overflows or traps for these depths.
        throw std::invalid_argument("model projection depth is below the original division limit");
    }
    const auto magnitude = std::uint32_t(0x100000000ull / std::uint64_t(std::abs(std::int64_t(depth))));
    const auto reciprocal = depth < 0 ? std::int32_t(0u - magnitude) : std::int32_t(magnitude);
    const auto scale = shifted_product(reciprocal, std::int32_t(std::uint32_t(projection.focal) << 16), 16);
    result.state = ModelProjectionState::projected;
    result.screen_radius = shifted_product(scale, radius, 16);
    result.point[0] = wrapped_sub(projection.centre_x, shifted_product(scale, camera_point[1], 16));
    result.point[1] = wrapped_sub(projection.centre_y, shifted_product(scale, camera_point[2], 16));
    result.point[2] = depth;
    return result;
}

std::optional<ModelCollectResult> collect_scene_model(const ModelCollectInput& input,
    const ModelViewState& view) {
    if (!collect_prefilter(input, view)) return {};
    const auto projection = collect_project(input, view);
    if (!projection) return {};
    ModelCollectResult result;
    result.entry = make_entry(input, *projection);
    if (projection->screen_radius > model_view_large_radius) {
        result.queue = ModelQueue::large;
        result.entry.screen_x = 0;
        result.entry.screen_y = 0;
        result.entry.camera_depth = 0;
    } else {
        result.queue = ModelQueue::near;
    }
    return result;
}

std::optional<ModelQueueEntry> collect_effect_model(const ModelCollectInput& input,
    const ModelViewState& view) {
    ModelCollectInput visible = input;
    visible.hidden = false;
    if (!collect_prefilter(visible, view)) return {};
    const auto projection = collect_project(visible, view);
    if (!projection) return {};
    return make_entry(visible, *projection);
}

std::optional<ModelQueueEntry> collect_character_model(const ModelCollectInput& input,
    const ModelViewState& view) {
    if (!collect_prefilter(input, view)) return {};
    ModelCollectInput character = input;
    character.radius = model_view_character_radius;
    const auto projection = collect_project(character, view);
    if (!projection) return {};
    return make_entry(character, *projection);
}

std::size_t cull_model_queue(ModelQueueList& entries, const ModelScreenBounds& bounds,
    const ModelRowDepthTable& table, const void* suppressed_object, bool allow_suppressed) {
    std::size_t visible = 0;
    for (auto& entry : entries) {
        if (!model_row_visible(entry, bounds, table)) entry.object = nullptr;
        if (entry.object != nullptr && entry.object == suppressed_object && !allow_suppressed) {
            entry.object = nullptr;
        }
        if (entry.object != nullptr) ++visible;
    }
    return visible;
}

const void* select_object_model(const ModelObjectView& object) {
    if (object.prefer_secondary && object.secondary != nullptr) return object.secondary;
    return object.primary;
}

ModelDrawCallback resolve_model_draw_callback(const ModelCallbackRow* rows, std::size_t count,
    std::uint32_t key) {
    if (rows == nullptr || count == 0) return nullptr;
    for (std::size_t index = 0; index < count; ++index) {
        if (rows[index].key == key) return rows[index].draw;
    }
    return rows[0].draw;
}

ModelMatrixCallback resolve_model_matrix_callback(const ModelCallbackRow* rows, std::size_t count,
    std::uint32_t key) {
    if (rows == nullptr || count == 0) return nullptr;
    for (std::size_t index = 0; index < count; ++index) {
        if (rows[index].key == key) return rows[index].matrix;
    }
    return rows[0].matrix;
}

const ModelCallbackTag* find_model_callback_tag(std::uint32_t key) {
    for (const auto& tag : model_callback_tags) {
        if (tag.key == key) return &tag;
    }
    return nullptr;
}

std::array<ModelCallbackRow, model_callback_tags.size()> model_callback_rows() {
    std::array<ModelCallbackRow, model_callback_tags.size()> rows{};
    for (std::size_t index = 0; index < rows.size(); ++index) {
        rows[index].key = model_callback_tags[index].key;
        rows[index].draw = reinterpret_cast<ModelDrawCallback>(
            static_cast<std::uintptr_t>(model_callback_tags[index].draw_address));
        rows[index].matrix = reinterpret_cast<ModelMatrixCallback>(
            static_cast<std::uintptr_t>(model_callback_tags[index].matrix_address));
    }
    return rows;
}

std::optional<std::uint32_t> select_model_level(const std::array<std::int32_t, 4>& lod_thresholds,
    std::int32_t screen_radius) {
    if (screen_radius <= model_view_minimum_radius) return {};
    for (std::uint32_t level = 0; level < lod_thresholds.size(); ++level) {
        if (screen_radius > lod_thresholds[level]) return level;
    }
    return {};
}

std::optional<ModelDrawCall> select_model_draw(const ModelRuntimeModel& model,
    const ModelObjectView& object, std::uint32_t clip_mask, std::int32_t screen_radius) {
    const auto model_pointer = select_object_model(object);
    if (model_pointer == nullptr) return {};
    const auto level = select_model_level(model.lod_thresholds, screen_radius);
    if (!level) return {};
    if (model.lod_data[*level] == nullptr || model.lod_draw[*level] == nullptr) return {};
    ModelDrawCall call;
    call.object = object.identity;
    call.level_data = model.lod_data[*level];
    call.callback = model.lod_draw[*level];
    call.clip_mask = clip_mask;
    call.level = *level;
    return call;
}

std::vector<ModelDrawCall> run_model_draw_pass(const ModelQueueList& entries,
    const ModelPassAccessors& accessors) {
    if (!accessors.object || !accessors.model) {
        throw std::invalid_argument("model draw pass accessors are missing");
    }
    std::vector<ModelDrawCall> calls;
    calls.reserve(entries.size());
    for (const auto& entry : entries) {
        if (entry.object == nullptr) continue;
        auto object = accessors.object(entry.object);
        if (object.identity == nullptr) object.identity = entry.object;
        const auto model_pointer = select_object_model(object);
        if (model_pointer == nullptr) continue;
        const auto* model = accessors.model(model_pointer);
        if (model == nullptr) continue;
        const auto call = select_model_draw(*model, object, entry.clip_mask, entry.screen_radius);
        if (call) calls.push_back(*call);
    }
    return calls;
}

bool model_needs_face_clipping(std::uint32_t clip_mask) {
    return clip_mask != 0 && (clip_mask & 0x40) == 0;
}

std::vector<ModelClipPlane> model_clip_planes_for_mask(std::uint32_t clip_mask,
    const std::array<ModelClipPlane, 5>& planes) {
    // 0x46a930 order: bit 2 (plane 0), 4 (plane 1), 8 (plane 2), 0x20 (plane 4), 0x10 (plane 3).
    static const std::array<std::pair<std::uint32_t, std::size_t>, 5> choices{{
        {2, 0}, {4, 1}, {8, 2}, {0x20, 4}, {0x10, 3},
    }};
    std::vector<ModelClipPlane> result;
    for (const auto& [bit, index] : choices) {
        if ((clip_mask & bit) != 0) result.push_back(planes[index]);
    }
    return result;
}

} // namespace dfre
