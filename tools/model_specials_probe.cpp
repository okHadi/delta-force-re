#include "dfre/model_specials.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr char kDigits[] = "0123456789abcdef";

std::int32_t parse_int(const std::string& text) {
    return static_cast<std::int32_t>(std::stoul(text, nullptr, 0));
}

void print_hex32(std::uint32_t value) {
    for (int shift = 28; shift >= 0; shift -= 4) {
        std::cout << kDigits[(value >> shift) & 15];
    }
}

std::vector<std::int32_t> parse_words(const std::string& text) {
    std::vector<std::int32_t> words;
    for (std::size_t at = 0; at + 8 <= text.size(); at += 8) {
        words.push_back(parse_int("0x" + text.substr(at, 8)));
    }
    return words;
}

dfre::ModelMatrix to_matrix(const std::vector<std::int32_t>& words, std::size_t offset) {
    dfre::ModelMatrix matrix{};
    for (std::size_t index = 0; index < 12; ++index) {
        matrix[index] = words[offset + index];
    }
    return matrix;
}

dfre::ModelSpecialTag parse_tag(const std::string& name) {
    const dfre::ModelSpecialDef* defs = dfre::model_special_defs();
    for (std::size_t index = 0; index < dfre::model_special_def_count(); ++index) {
        if (name == defs[index].name) {
            return defs[index].tag;
        }
    }
    std::cerr << "Unknown tag: " << name << '\n';
    std::exit(1);
}

void run_draw(std::istringstream& parts) {
    std::string tag_name, object_hex, view_hex, elements_hex, slot_hex;
    std::string mesh_count, value_98, value_a0, value_e0, value_94;
    std::string angle_9c, angle_a4, angle_c4, angle_c8, z_high, z_30;
    std::string flag_5a, divisor_58, threshold_40, distance_fc;
    std::string phase_ec, angle_e4, angle_e8, mode_f0, phase_33, flag_3c, z_low, callback_arg3;
    parts >> tag_name >> mesh_count >> value_98 >> value_a0 >> value_e0 >> value_94 >>
        angle_9c >> angle_a4 >> angle_c4 >> angle_c8 >> z_high >> z_30 >>
        phase_ec >> angle_e4 >> angle_e8 >> mode_f0 >> phase_33 >> flag_3c >> z_low >> callback_arg3 >>
        flag_5a >> divisor_58 >> threshold_40 >> distance_fc >>
        object_hex >> slot_hex >> view_hex >> elements_hex;
    dfre::ModelSpecialInstance instance;
    instance.mesh_count = parse_int(mesh_count);
    instance.mesh_base = 0x40000000u;   // marker base; the probe reports offsets
    instance.flag_5a = parse_int(flag_5a);
    instance.divisor_58 = parse_int(divisor_58);
    instance.threshold_40 = parse_int(threshold_40);
    dfre::ModelSpecialObject object;
    object.value_98 = parse_int(value_98);
    object.value_a0 = parse_int(value_a0);
    object.value_e0 = parse_int(value_e0);
    object.value_94 = parse_int(value_94);
    object.angle_9c = parse_int(angle_9c);
    object.angle_a4 = parse_int(angle_a4);
    object.angle_c4 = parse_int(angle_c4);
    object.angle_c8 = parse_int(angle_c8);
    object.z_high = parse_int(z_high);
    object.z_30 = parse_int(z_30);
    object.distance_fc = parse_int(distance_fc);
    object.phase_ec = parse_int(phase_ec);
    object.angle_e4 = parse_int(angle_e4);
    object.angle_e8 = parse_int(angle_e8);
    object.mode_f0 = parse_int(mode_f0);
    object.phase_33 = parse_int(phase_33);
    object.flag_3c = parse_int(flag_3c);
    object.z_low = parse_int(z_low);
    object.callback_arg3 = parse_int(callback_arg3);
    const dfre::ModelSpecialPlan plan =
        dfre::build_model_special_plan(parse_tag(tag_name), instance, object);
    const auto object_words = parse_words(object_hex);
    const auto slot_words = parse_words(slot_hex);
    const auto view_words = parse_words(view_hex);
    const auto element_words = parse_words(elements_hex);
    std::vector<std::array<std::int32_t, 3>> elements;
    for (std::size_t at = 0; at + 3 <= element_words.size(); at += 3) {
        elements.push_back({element_words[at], element_words[at + 1], element_words[at + 2]});
    }
    const auto object_matrix = to_matrix(object_words, 0);
    const auto view_matrix = to_matrix(view_words, 0);
    const auto matrices = dfre::model_special_step_matrices(
        plan, object_matrix, view_matrix, elements);
    const auto slots = dfre::model_special_slot_matrices(
        plan, to_matrix(slot_words, 0), elements);
    std::cout << matrices.size() << ' ' << (plan.view_multiply ? 1 : 0) << ' '
              << (plan.has_base_matrix ? 1 : 0) << ' '
              << (plan.draws ? 1 : 0) << ' '
              << plan.placement_angle_offset << ' '
              << dfre::model_special_counter(instance, object) << '\n';
    for (std::size_t index = 0; index < matrices.size(); ++index) {
        std::cout << (instance.mesh_base + plan.steps[index].mesh_offset) << ' ';
        for (const auto word : matrices[index]) {
            print_hex32(static_cast<std::uint32_t>(word));
            std::cout << ' ';
        }
        std::cout << '\n';
    }
    std::cout << slots.size() << '\n';
    for (std::size_t index = 0; index < slots.size(); ++index) {
        std::cout << plan.slots[index].index << ' ';
        for (const auto word : slots[index]) {
            print_hex32(static_cast<std::uint32_t>(word));
            std::cout << ' ';
        }
        std::cout << '\n';
    }
    std::cout << plan.localized_slots.size();
    for (const auto index : plan.localized_slots) {
        std::cout << ' ' << index;
    }
    std::cout << '\n';
    std::cout << plan.writes.size() << '\n';
    for (const auto& write : plan.writes) {
        std::cout << static_cast<int>(write.target) << ' ' << write.value << ' '
                  << (write.restore ? 1 : 0) << '\n';
    }
}

void run_counter(std::istringstream& parts) {
    std::string flag, divisor, threshold, distance;
    parts >> flag >> divisor >> threshold >> distance;
    dfre::ModelSpecialInstance instance;
    instance.flag_5a = parse_int(flag);
    instance.divisor_58 = parse_int(divisor);
    instance.threshold_40 = parse_int(threshold);
    dfre::ModelSpecialObject object;
    object.distance_fc = parse_int(distance);
    std::cout << dfre::model_special_counter(instance, object) << '\n';
}

void run_table() {
    // 0xbd0068 holds 2561 sine entries (one full turn over 2048 steps) that the
    // original interpolates, using index + 1 as the cosine. Same values the
    // shared character probe publishes.
    bool first = true;
    for (unsigned index = 0; index < 2561; ++index) {
        const auto value = static_cast<std::int32_t>(
            std::sin(static_cast<double>(index) * 0.00306796162109375) * 4194304.0);
        if (!first) {
            std::cout << ' ';
        }
        first = false;
        std::cout << value;
    }
    std::cout << '\n';
}

void run_lookup(std::istringstream& parts) {
    std::string key_text;
    parts >> key_text;
    const std::uint32_t key = static_cast<std::uint32_t>(
        std::stoul(key_text, nullptr, 16));
    const dfre::ModelSpecialDef* def = dfre::find_model_special_def(key);
    if (def == nullptr) {
        std::cout << "0\n";
        return;
    }
    std::cout << def->name << ' ';
    print_hex32(def->matrix_address);
    std::cout << ' ';
    print_hex32(def->draw_address);
    std::cout << '\n';
}

} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream parts(line);
        std::string command;
        parts >> command;
        if (command == "draw") {
            run_draw(parts);
        } else if (command == "counter") {
            run_counter(parts);
        } else if (command == "lookup") {
            run_lookup(parts);
        } else if (command == "table") {
            run_table();
        } else {
            std::cerr << "Unknown probe command: " << command << '\n';
            return 1;
        }
        if (!std::cout) {
            return 1;
        }
    }
    return std::cin.bad() ? 1 : 0;
}
