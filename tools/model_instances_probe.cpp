#include "dfre/model_instances.hpp"
#include <iostream>
#include <string>

// Line protocol probe for tools/check_model_instances_original.py.
// Read that script's docstring for the mode meanings. One case per input
// record; the checker sends cases in the same order it replays them.
namespace {
dfre::ModelInstanceFields read_fields() {
    dfre::ModelInstanceFields fields;
    for (auto& value : fields.translation) std::cin >> value;
    for (auto& value : fields.angles) std::cin >> value;
    return fields;
}

void print_matrix(const dfre::ModelMatrix& matrix) {
    for (std::size_t k = 0; k < matrix.size(); ++k) {
        if (k) std::cout << ' ';
        std::cout << matrix[k];
    }
    std::cout << '\n';
}

// instance: t0 t1 t2 a0 a1 a2 w0..w11 -> local line, camera line.
void mode_instance() {
    dfre::ModelMatrix view{};
    dfre::ModelInstanceFields fields;
    while (true) {
        fields = read_fields();
        if (!(std::cin >> view[0])) break;
        for (std::size_t k = 1; k < view.size(); ++k) std::cin >> view[k];
        print_matrix(dfre::model_instance_local_matrix(fields));
        print_matrix(dfre::model_instance_camera_matrix(fields, view));
    }
}

// multiply: left 12, right 12 -> product line.
void mode_multiply() {
    dfre::ModelMatrix left{}, right{};
    while (std::cin >> left[0]) {
        for (std::size_t k = 1; k < left.size(); ++k) std::cin >> left[k];
        for (auto& value : right) std::cin >> value;
        print_matrix(dfre::model_multiply_matrix(left, right));
    }
}

// scaled: t0 t1 t2 a0 a1 a2 scale -> scaled local line.
void mode_scaled() {
    dfre::ModelInstanceFields fields;
    std::int32_t scale = 0;
    while (std::cin >> fields.translation[0]) {
        for (std::size_t k = 1; k < fields.translation.size(); ++k) std::cin >> fields.translation[k];
        for (auto& value : fields.angles) std::cin >> value;
        std::cin >> scale;
        print_matrix(dfre::model_instance_local_matrix_scaled(fields, scale));
    }
}

// counter: time count_raw duration_raw -> frame value, "off", or "error".
void mode_counter() {
    std::uint32_t time = 0;
    dfre::ModelInstanceLod lod;
    while (std::cin >> time >> lod.frame_count_raw >> lod.frame_duration_raw) {
        try {
            const auto frame = dfre::model_instance_animation_frame(time, lod);
            if (frame) std::cout << *frame << '\n';
            else std::cout << "off\n";
        } catch (const std::invalid_argument&) {
            std::cout << "error\n";
        }
    }
}

// callback: mesh_count t0 t1 t2 a0 a1 a2 -> count line, count matrix lines.
void mode_callback() {
    dfre::ModelInstanceFields fields;
    std::size_t mesh_count = 0;
    while (std::cin >> mesh_count) {
        for (auto& value : fields.translation) std::cin >> value;
        for (auto& value : fields.angles) std::cin >> value;
        const auto matrices = dfre::model_instance_local_matrices(mesh_count, fields);
        std::cout << matrices.size() << '\n';
        for (const auto& matrix : matrices) print_matrix(matrix);
    }
}

// generic: time count_raw duration_raw mesh_count mask t0 t1 t2 a0 a1 a2 w0..w11
//   frame line ("N", "off", or "error"); when not an error:
//   mesh_count line, one matrix line per mesh, local line, camera line.
void mode_generic() {
    std::uint32_t time = 0;
    dfre::ModelInstanceLod lod;
    std::size_t mesh_count = 0;
    std::uint32_t mask = 0;
    dfre::ModelMatrix view{};
    dfre::ModelInstanceFields fields;
    while (std::cin >> time >> lod.frame_count_raw >> lod.frame_duration_raw >> mesh_count >> mask) {
        (void)mask;
        fields = read_fields();
        for (auto& value : view) std::cin >> value;
        lod.mesh_count = mesh_count;
        try {
            const auto draw = dfre::build_model_instance_draw(lod, fields, view, time);
            if (draw.frame) std::cout << *draw.frame << '\n';
            else std::cout << "off\n";
            std::cout << draw.mesh_count << '\n';
            for (const auto& matrix : draw.matrices) print_matrix(matrix);
            print_matrix(draw.local);
            print_matrix(draw.camera);
        } catch (const std::invalid_argument&) {
            std::cout << "error\n";
        }
    }
}

// static: use_secondary has_primary has_secondary distance mask thr0..3
//   c0 c1 c2 c3 (-1 means a null record) t0 t1 t2 a0 a1 a2 w0..w11
//   "lod fill draws" line; when a model exists: local line, camera line,
//   fill matrix lines, one line of draw mesh indices.
void mode_static() {
    std::uint32_t use_secondary = 0, has_primary = 0, has_secondary = 0, mask = 0;
    std::int32_t distance = 0;
    std::array<std::int32_t, 4> thresholds{};
    std::array<std::int32_t, 4> counts{};
    dfre::ModelMatrix view{};
    dfre::ModelInstanceFields fields;
    while (std::cin >> use_secondary >> has_primary >> has_secondary >> distance >> mask) {
        (void)mask;
        for (auto& value : thresholds) std::cin >> value;
        for (auto& value : counts) std::cin >> value;
        fields = read_fields();
        for (auto& value : view) std::cin >> value;
        dfre::ModelStaticObject object;
        object.use_secondary = use_secondary != 0;
        dfre::ModelStaticRecord record;
        record.lod_thresholds = thresholds;
        for (std::size_t k = 0; k < counts.size(); ++k) {
            if (counts[k] < 0) continue;
            dfre::ModelInstanceLod lod;
            lod.mesh_count = std::size_t(counts[k]);
            record.lods[k] = lod;
        }
        if (has_primary) object.primary = record;
        if (has_secondary) object.secondary = record;
        try {
            const auto draw = dfre::build_model_static_draw(object, fields, view, distance);
            if (!draw.has_model) {
                std::cout << "-2 0 0\n";
                continue;
            }
            std::cout << (draw.lod ? int(*draw.lod) : -1) << ' ' << draw.matrix_slots << ' '
                      << draw.mesh_indices.size() << '\n';
            print_matrix(draw.local);
            print_matrix(draw.camera);
            for (const auto& matrix : draw.matrices) print_matrix(matrix);
            for (std::size_t k = 0; k < draw.mesh_indices.size(); ++k) {
                if (k) std::cout << ' ';
                std::cout << draw.mesh_indices[k];
            }
            std::cout << '\n';
        } catch (const std::invalid_argument&) {
            std::cout << "error\n";
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: model_instances_probe MODE\n";
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "instance") mode_instance();
    else if (mode == "multiply") mode_multiply();
    else if (mode == "scaled") mode_scaled();
    else if (mode == "counter") mode_counter();
    else if (mode == "callback") mode_callback();
    else if (mode == "generic") mode_generic();
    else if (mode == "static") mode_static();
    else {
        std::cerr << "unknown mode\n";
        return 2;
    }
    return 0;
}
