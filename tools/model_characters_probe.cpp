#include "dfre/model_characters.hpp"

#include <cmath>
#include <deque>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
unsigned read() {
    unsigned value;
    if (!(std::cin >> value)) throw std::invalid_argument("missing input");
    return value;
}

std::int32_t signed_word() {
    return std::int32_t(read());
}

dfre::ModelMatrix matrix() {
    dfre::ModelMatrix value{};
    for (auto& word : value) word = signed_word();
    return value;
}

dfre::Model read_model_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::invalid_argument("cannot open model file");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    return dfre::decode_model(bytes);
}

dfre::CharacterModel read_character_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::invalid_argument("cannot open character file");
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    return dfre::prepare_character_runtime(dfre::decode_character(bytes));
}

// Print one ready fixture line from real PLAYER01 data: first LOD, first animation frame.
void emit_fixture(const std::string& model_path, const std::string& character_path) {
    const auto model = read_model_file(model_path);
    if (model.lods.empty() || model.lods[0].meshes.empty()) throw std::invalid_argument("model has no meshes");
    const auto localized = dfre::localize_model_lod(model.lods[0]);
    const auto character = read_character_file(character_path);
    if (character.animations.empty() || character.animations[0].frames.empty())
        throw std::invalid_argument("character has no frames");
    const auto pose = dfre::character_frame_pose(character.animations[0].frames[0]);
    if (pose.size() != localized.meshes.size()) throw std::invalid_argument("pose count differs from meshes");
    std::vector<unsigned> output{0, 0, static_cast<unsigned>(localized.meshes.size()), 1};
    for (unsigned i = 0; i < 12; ++i) output.push_back(i == 0 || i == 5 || i == 10 ? 4194304u : 0u);
    output.insert(output.end(), {0, 0, 0, 0, 0, 0, 0, 0, 0});
    output.push_back(static_cast<unsigned>(pose.size()));
    for (std::size_t index = 0; index < pose.size(); ++index) {
        const auto bone = character.bones[character.bone_map[index]];
        const auto parent = std::int32_t(std::uint32_t(character.raw[bone.offset + 56]) |
            std::uint32_t(character.raw[bone.offset + 57]) << 8 |
            std::uint32_t(character.raw[bone.offset + 58]) << 16 |
            std::uint32_t(character.raw[bone.offset + 59]) << 24);
        output.push_back(static_cast<unsigned>(parent));
    }
    for (const auto& bone : pose)
        for (const auto angle : bone.angles) output.push_back(static_cast<unsigned>(angle));
    for (const auto& mesh : localized.meshes)
        for (const auto value : mesh.delta_origin) output.push_back(static_cast<unsigned>(value));
    output.insert(output.end(), {0, 0, 0});                       // no weapon attachment
    output.insert(output.end(), {0, 0});                          // no secondary attachment
    for (unsigned i = 0; i < 48; ++i) output.push_back(i % 5 == 0 ? 4194304u : 0u);
    output.push_back(0);
    bool first = true;
    for (const auto value : output) {
        if (!first) std::cout << ' ';
        first = false;
        std::cout << value;
    }
    std::cout << '\n';
}

// Print the port's sine table: entries 0..2047 from fixed_sine_cosine sine samples, entries
// 2048..2559 from cosine samples at the same exact-entry angles, and entry 2560 from the
// documented formula. Entry 2560 is unreachable through the original lookup (index is at most
// 2047 and cosine adds 512). The replay compares these values with original 477510 output.
void emit_sincos_table() {
    std::array<std::int32_t, 2561> values{};
    for (unsigned index = 0; index < 2048; ++index)
        values[index] = dfre::fixed_sine_cosine(index << 21)[0];
    for (unsigned index = 2048; index < 2560; ++index)
        values[index] = dfre::fixed_sine_cosine((index - 512) << 21)[1];
    values[2560] = std::int32_t(std::sin(double(2560) * 0.00306796162109375) * 4194304.0);
    bool first = true;
    for (const auto value : values) {
        if (!first) std::cout << ' ';
        first = false;
        std::cout << value;
    }
    std::cout << '\n';
}

void run_case() {
    const auto lod_index = read();
    const auto counter = read();
    dfre::CharacterDrawFields fields;
    const auto mesh_count = read();
    const auto has_character = read();
    const auto camera = matrix();
    for (auto& value : fields.transform.translation) value = signed_word();
    for (auto& value : fields.transform.angles) value = read();
    fields.flags_6c = std::uint8_t(read());
    fields.flags_6d = std::uint8_t(read());
    fields.weapon_slot = std::uint16_t(read());
    fields.mesh_count = mesh_count;
    dfre::CharacterModel character;
    std::vector<dfre::CharacterBonePose> pose;
    std::vector<std::size_t> parents;
    if (has_character) {
        const auto bone_count = read();
        if (bone_count > 512) throw std::invalid_argument("too many bones");
        parents.resize(bone_count);
        for (auto& value : parents) value = read();
        pose.resize(bone_count);
        for (auto& value : pose)
            for (auto& angle : value.angles) angle = signed_word();
        for (auto& value : pose) value.scale = {65536, 65536, 65536};
    }
    dfre::ModelLod localized;
    localized.origins_localized = true;
    localized.meshes.resize(mesh_count);
    for (auto& mesh : localized.meshes)
        for (auto& value : mesh.delta_origin) value = signed_word();
    if (has_character) {
        character.runtime_prepared = true;
        character.bones.resize(parents.size());
        character.bone_map.resize(parents.size());
        std::size_t offset = 0;
        for (std::size_t index = 0; index < parents.size(); ++index) {
            character.bones[index].offset = offset;
            character.bone_map[index] = index;
            offset += 64;
        }
        character.raw.assign(offset + 64, 0);
        for (std::size_t index = 0; index < parents.size(); ++index) {
            const auto parent = std::uint32_t(parents[index]);
            for (unsigned byte = 0; byte < 4; ++byte)
                character.raw[index * 64 + 56 + byte] = std::uint8_t(parent >> (8 * byte));
        }
    }
    dfre::CharacterAttachments attachments;
    attachments.weapon_present = read() != 0;
    attachments.weapon_type = read();
    const auto weapon_lods = read();
    if (weapon_lods) {
        dfre::CharacterAttachmentRecord record;
        record.lod_count = weapon_lods;
        for (unsigned i = 0; i < weapon_lods; ++i) {
            const auto meshes = signed_word();
            if (meshes < 0) record.lods.push_back(nullptr);
            else {
                static std::deque<dfre::ModelLod> owned; // attachment LODs are draw-only here
                owned.emplace_back();
                owned.back().origins_localized = true;
                owned.back().meshes.resize(std::size_t(meshes));
                record.lods.push_back(&owned.back());
            }
        }
        attachments.weapon = std::move(record);
    }
    attachments.secondary_present = read() != 0;
    const auto secondary_lods = read();
    if (secondary_lods) {
        dfre::CharacterAttachmentRecord record;
        record.lod_count = secondary_lods;
        for (unsigned i = 0; i < secondary_lods; ++i) {
            const auto meshes = signed_word();
            if (meshes < 0) record.lods.push_back(nullptr);
            else {
                static std::deque<dfre::ModelLod> owned;
                owned.emplace_back();
                owned.back().origins_localized = true;
                owned.back().meshes.resize(std::size_t(meshes));
                record.lods.push_back(&owned.back());
            }
        }
        attachments.secondary = std::move(record);
    }
    dfre::CharacterAttachmentMatrices matrices;
    matrices.weapon_left = matrix();
    matrices.weapon_right = matrix();
    matrices.secondary_left = matrix();
    matrices.secondary_right = matrix();
    const auto draw_context = read();
    const auto plan = dfre::build_character_draw_plan(std::uint32_t(counter), fields,
        has_character ? &character : nullptr, has_character ? &pose : nullptr,
        has_character ? &localized : nullptr, attachments, matrices, camera, lod_index, draw_context);
    bool first = true;
    auto emit = [&](long long value) {
        if (!first) std::cout << ' ';
        first = false;
        std::cout << value;
    };
    emit(plan.character_matrices ? 1 : 0);
    for (const auto value : plan.actor_local) emit(value);
    for (const auto value : plan.actor_camera) emit(value);
    emit(static_cast<long long>(plan.matrix_tables.size()));
    for (const auto& table : plan.matrix_tables) {
        emit(static_cast<long long>(table.size()));
        for (const auto& entry : table)
            for (const auto value : entry) emit(value);
    }
    emit(static_cast<long long>(plan.draws.size()));
    for (const auto& draw : plan.draws) {
        emit(static_cast<long long>(draw.source));
        emit(static_cast<long long>(draw.mesh_index));
        emit(static_cast<long long>(draw.matrix_index));
        emit(static_cast<long long>(draw.matrix_table));
        emit(draw.material_frame);
        emit(static_cast<long long>(draw.draw_context));
    }
    emit(static_cast<long long>(plan.material_frame_writes.size()));
    for (const auto value : plan.material_frame_writes) emit(value);
    emit(plan.alternate_counter ? static_cast<long long>(*plan.alternate_counter) : -1);
    emit(plan.weapon_type);
    emit(plan.weapon_skipped ? 1 : 0);
    emit(plan.secondary_skipped ? 1 : 0);
    std::cout << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "fixture") {
            emit_fixture(argv[2], argv[3]);
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "sincos") {
            emit_sincos_table();
            return 0;
        }
        if (argc != 1) return 2;
        unsigned cases;
        if (!(std::cin >> cases)) return 2;
        for (unsigned index = 0; index < cases; ++index) run_case();
        std::cin >> std::ws;
        return std::cin.eof() ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
