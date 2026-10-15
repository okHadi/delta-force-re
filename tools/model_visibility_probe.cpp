#include "dfre/model_visibility.hpp"
#include <iostream>
#include <string>

// Line protocol probe for tools/check_model_visibility_original.py.
// Read the docstring of that script for the mode meanings.
namespace {
dfre::ModelViewState read_view() {
    dfre::ModelViewState view;
    std::cin >> view.projection.focal >> view.projection.centre_x >> view.projection.centre_y;
    for (auto& plane : view.projection.planes) {
        for (auto& value : plane.normal) std::cin >> value;
        std::cin >> plane.offset;
    }
    for (auto& value : view.world_to_camera) std::cin >> value;
    for (auto& value : view.camera_position) std::cin >> value;
    return view;
}

void read_tables(std::vector<std::int32_t>& keys, std::vector<std::vector<std::uint16_t>>& rows,
    std::vector<const std::uint16_t*>& pointers, int count) {
    keys.assign(std::size_t(count), 0);
    rows.assign(std::size_t(count), std::vector<std::uint16_t>(std::size_t(count), 0));
    for (int row = 0; row < count; ++row) {
        std::cin >> keys[std::size_t(row)];
        for (int column = 0; column < count; ++column) {
            int value = 0;
            std::cin >> value;
            rows[std::size_t(row)][std::size_t(column)] = std::uint16_t(value);
        }
    }
    pointers.clear();
    for (const auto& row : rows) pointers.push_back(row.data());
}

void mode_lod() {
    std::array<std::int32_t, 4> thresholds{};
    std::int32_t radius = 0;
    while (std::cin >> thresholds[0]) {
        for (std::size_t k = 1; k < thresholds.size(); ++k) std::cin >> thresholds[k];
        std::cin >> radius;
        const auto level = dfre::select_model_level(thresholds, radius);
        std::cout << (level ? int(*level) : -1) << '\n';
    }
}

void mode_project() {
    dfre::ModelViewProjection projection;
    while (std::cin >> projection.focal >> projection.centre_x >> projection.centre_y) {
        for (auto& plane : projection.planes) {
            for (auto& value : plane.normal) std::cin >> value;
            std::cin >> plane.offset;
        }
        std::int32_t radius = 0, depth = 0, point_y = 0, point_z = 0;
        std::cin >> radius >> depth >> point_y >> point_z;
        const auto result = dfre::project_model_object(projection, {depth, point_y, point_z}, radius);
        const int state = result.state == dfre::ModelProjectionState::outside ? 0 :
            (result.state == dfre::ModelProjectionState::projected ? 1 : 2);
        // 0x44acf0 returns 1 for outside; otherwise it returns the accumulated clip mask.
        const auto returned = state == 0 ? 1u : result.clip_mask;
        std::cout << state << ' ' << returned << ' ' << result.point[0] << ' '
                  << result.point[1] << ' ' << result.point[2] << ' ' << result.screen_radius << '\n';
    }
}

void mode_collect() {
    std::uint32_t kind = 0, hidden = 0, class_kind = 0;
    std::int32_t px = 0, py = 0, pz = 0, radius = 0;
    std::size_t identity = 0;
    while (true) {
        const auto view = read_view();
        if (!(std::cin >> kind >> px >> py >> pz >> radius >> hidden >> class_kind)) break;
        dfre::ModelCollectInput input;
        input.identity = reinterpret_cast<const void*>(++identity);
        input.position = {px, py, pz};
        input.model = reinterpret_cast<const void*>(0x5000);
        input.radius = radius;
        input.hidden = hidden != 0;
        if (kind == 1) {
            const auto result = dfre::collect_character_model(input, view);
            if (!result) {
                std::cout << "reject\n";
                continue;
            }
            std::cout << "character " << result->clip_mask << ' ' << result->screen_radius << ' '
                      << result->screen_x << ' ' << result->screen_y << ' ' << result->camera_depth << '\n';
        } else if (kind == 2) {
            const auto result = dfre::collect_effect_model(input, view);
            if (!result) {
                std::cout << "reject\n";
                continue;
            }
            std::cout << "near " << result->clip_mask << ' ' << result->screen_radius << ' '
                      << result->screen_x << ' ' << result->screen_y << ' ' << result->camera_depth << '\n';
        } else {
            const auto result = dfre::collect_scene_model(input, view);
            if (!result) {
                std::cout << "reject\n";
                continue;
            }
            std::cout << (result->queue == dfre::ModelQueue::large ? "large" : "near") << ' '
                      << result->entry.clip_mask << ' ' << result->entry.screen_radius << ' '
                      << result->entry.screen_x << ' ' << result->entry.screen_y << ' '
                      << result->entry.camera_depth << '\n';
        }
    }
}

void mode_visible() {
    dfre::ModelScreenBounds bounds;
    std::cin >> bounds.left >> bounds.top >> bounds.right >> bounds.bottom;
    std::vector<std::int32_t> keys;
    std::vector<std::vector<std::uint16_t>> rows;
    std::vector<const std::uint16_t*> pointers;
    read_tables(keys, rows, pointers, 24);
    const dfre::ModelRowDepthTable table{keys.data(), pointers.data()};
    dfre::ModelQueueEntry entry;
    while (std::cin >> entry.screen_radius >> entry.screen_x >> entry.screen_y >> entry.camera_depth) {
        std::cout << (dfre::model_row_visible(entry, bounds, table) ? 1 : 0) << '\n';
    }
}

void mode_cull() {
    dfre::ModelScreenBounds bounds;
    std::cin >> bounds.left >> bounds.top >> bounds.right >> bounds.bottom;
    std::vector<std::int32_t> keys;
    std::vector<std::vector<std::uint16_t>> rows;
    std::vector<const std::uint16_t*> pointers;
    read_tables(keys, rows, pointers, 24);
    const dfre::ModelRowDepthTable table{keys.data(), pointers.data()};
    std::size_t object = 0, suppressed = 0, allow = 0, kind = 0;
    std::int32_t radius = 0, x = 0, y = 0, depth = 0;
    while (std::cin >> object >> suppressed >> allow >> kind >> radius >> x >> y >> depth) {
        dfre::ModelQueueEntry entry;
        entry.object = reinterpret_cast<const void*>(object);
        entry.screen_radius = radius;
        entry.screen_x = x;
        entry.screen_y = y;
        entry.camera_depth = depth;
        dfre::ModelQueueList queue{entry};
        // 0x4854d0 loop 1 (near list) has no suppression; loop 2 (character list) does.
        if (kind == 0) dfre::cull_model_queue(queue, bounds, table);
        else dfre::cull_model_queue(queue, bounds, table, reinterpret_cast<const void*>(suppressed), allow != 0);
        std::cout << (queue[0].object == nullptr ? 0u : std::size_t(object)) << '\n';
    }
}

void mode_pass() {
    std::array<std::int32_t, 4> thresholds{};
    while (std::cin >> thresholds[0]) {
        for (std::size_t k = 1; k < thresholds.size(); ++k) std::cin >> thresholds[k];
        dfre::ModelRuntimeModel runtime;
        runtime.lod_thresholds = thresholds;
        std::int32_t id = 0;
        for (auto& data : runtime.lod_data) {
            std::cin >> id;
            data = id < 0 ? nullptr : reinterpret_cast<const void*>(std::size_t(id));
        }
        for (auto& callback : runtime.lod_draw) {
            std::cin >> id;
            callback = reinterpret_cast<dfre::ModelDrawCallback>(std::size_t(id));
        }
        std::size_t primary = 0, secondary = 0, prefer = 0, count = 0;
        std::cin >> primary >> secondary >> prefer >> count;
        const void* object_ids[5]{nullptr, reinterpret_cast<const void*>(1), reinterpret_cast<const void*>(2),
            reinterpret_cast<const void*>(3), reinterpret_cast<const void*>(4)};
        dfre::ModelQueueList entries;
        for (std::size_t index = 0; index < count; ++index) {
            std::size_t object = 0;
            std::uint32_t radius = 0, mask = 0;
            std::cin >> object >> radius >> mask;
            dfre::ModelQueueEntry entry;
            entry.object = object_ids[object];
            entry.screen_radius = std::int32_t(radius);
            entry.clip_mask = mask;
            entries.push_back(entry);
        }
        const dfre::ModelPassAccessors accessors{
            [&](const void* pointer) {
                dfre::ModelObjectView view;
                view.identity = pointer;
                view.primary = primary == 0 ? nullptr : reinterpret_cast<const void*>(primary);
                view.secondary = secondary == 0 ? nullptr : reinterpret_cast<const void*>(secondary);
                view.prefer_secondary = prefer != 0;
                return view;
            },
            [&](const void*) -> const dfre::ModelRuntimeModel* { return &runtime; }};
        bool any = false;
        for (const auto& call : dfre::run_model_draw_pass(entries, accessors)) {
            std::cout << "call " << call.level << ' ' << std::size_t(call.level_data) << ' '
                      << call.clip_mask << ' ' << std::size_t(call.object) << '\n';
            any = true;
        }
        if (!any) std::cout << "none\n";
    }
}

void mode_prep() {
    std::uint32_t kind = 0, hidden = 0, effect = 0;
    std::int32_t px = 0, py = 0, pz = 0, radius = 0;
    std::int32_t ex = 0, ey = 0, ez = 0, er = 0;
    std::size_t identity = 0;
    while (true) {
        const auto view = read_view();
        if (!(std::cin >> kind >> px >> py >> pz >> radius >> hidden >> effect >> ex >> ey >> ez >> er)) break;
        dfre::ModelQueueList near, large, character;
        dfre::ModelCollectInput input;
        input.identity = reinterpret_cast<const void*>(++identity);
        input.position = {px, py, pz};
        input.model = reinterpret_cast<const void*>(0x5000);
        input.radius = radius;
        input.hidden = hidden != 0;
        if (kind == 1) {
            const auto collected = dfre::collect_character_model(input, view);
            if (collected) character.push_back(*collected);
        } else {
            const auto collected = dfre::collect_scene_model(input, view);
            if (collected) {
                if (collected->queue == dfre::ModelQueue::large) large.push_back(collected->entry);
                else near.push_back(collected->entry);
            }
        }
        if (effect != 0) {
            dfre::ModelCollectInput effect_input;
            effect_input.identity = reinterpret_cast<const void*>(++identity);
            effect_input.position = {ex, ey, ez};
            effect_input.model = reinterpret_cast<const void*>(0x5000);
            effect_input.radius = er;
            const auto collected = dfre::collect_effect_model(effect_input, view);
            if (collected) near.push_back(*collected);
        }
        const auto print = [](const char* name, const dfre::ModelQueueList& queue) {
            std::cout << name << ' ' << queue.size();
            for (const auto& entry : queue) {
                std::cout << ' ' << entry.clip_mask << ' ' << entry.screen_radius << ' ' << entry.screen_x
                          << ' ' << entry.screen_y << ' ' << entry.camera_depth;
            }
            std::cout << '\n';
        };
        print("near", near);
        print("large", large);
        print("character", character);
    }
}
void mode_callback() {
    const auto rows = dfre::model_callback_rows();
    std::cout << "table " << rows.size() << '\n';
    for (const auto& row : rows) {
        std::cout << row.key << ' ' << std::size_t(row.draw) << ' ' << std::size_t(row.matrix) << '\n';
    }
    std::uint32_t key = 0;
    while (std::cin >> key) {
        std::cout << std::size_t(dfre::resolve_model_draw_callback(rows.data(), rows.size(), key)) << ' '
                  << std::size_t(dfre::resolve_model_matrix_callback(rows.data(), rows.size(), key)) << '\n';
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: model_visibility_probe MODE\n";
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "lod") mode_lod();
    else if (mode == "project") mode_project();
    else if (mode == "collect") mode_collect();
    else if (mode == "visible") mode_visible();
    else if (mode == "cull") mode_cull();
    else if (mode == "pass") mode_pass();
    else if (mode == "prep") mode_prep();
    else if (mode == "callback") mode_callback();
    else {
        std::cerr << "unknown mode\n";
        return 2;
    }
    return std::cin.eof() || mode == "pass" || mode == "prep" || mode == "callback" ? 0 : 2;
}
