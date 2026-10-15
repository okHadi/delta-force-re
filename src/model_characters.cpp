#include "dfre/model_characters.hpp"

#include <stdexcept>

namespace dfre {
namespace {
void require(bool value, const char* message) {
    if (!value) throw std::invalid_argument(message);
}

// Original 0xc23800 advance: old + 1, wrapping to 0 above 2.
std::uint32_t advance_counter(std::uint32_t value) {
    const auto next = value + 1;
    return next > 2 ? 0u : next;
}

struct AttachmentDraw {
    const ModelLod* lod{};
    bool draw{};
};

// Shared attachment resolution for +0x90 and +0x8c: record +0x14 LOD count, +0x50 table.
AttachmentDraw resolve_attachment(const std::optional<CharacterAttachmentRecord>& record,
    std::size_t lod_index) {
    if (!record) return {};
    require(record->lods.size() == record->lod_count, "attachment lod table size differs from count");
    if (lod_index >= record->lod_count) return {};
    const auto* lod = record->lods[lod_index];
    return {lod, lod != nullptr && !lod->meshes.empty()};
}
} // namespace

std::vector<ModelMatrix> character_draw_matrices(const CharacterModel* character,
    const std::vector<CharacterBonePose>* pose, const ModelLod* localized, std::uint32_t pitch,
    std::size_t mesh_count) {
    require(mesh_count > 0, "character draw needs at least one mesh");
    if (!character) return std::vector<ModelMatrix>(mesh_count, model_identity_matrix());
    require(pose != nullptr && localized != nullptr, "character matrices require pose and localized lod");
    require(localized->meshes.size() == pose->size(), "mesh matrix count differs from pose");
    // 0x481130 fills one matrix per pose entry; the draw loops +0x98 entries.
    require(localized->meshes.size() == mesh_count, "mesh matrix count differs from mesh count");
    return character_mesh_matrices(*character, *localized, *pose, pitch, nullptr);
}

CharacterDrawPlan build_character_draw_plan(std::uint32_t alternate_counter,
    const CharacterDrawFields& fields, const CharacterModel* character,
    const std::vector<CharacterBonePose>* pose, const ModelLod* localized,
    const CharacterAttachments& attachments, const CharacterAttachmentMatrices& attachment_matrices,
    const ModelMatrix& world_to_camera, std::size_t lod_index, std::uint32_t draw_context) {
    require(fields.mesh_count > 0, "character draw needs at least one mesh");
    CharacterDrawPlan plan;
    plan.character_matrices = character != nullptr;
    // 0x48136f zeroes the pitch word for 0x45d1e0 and restores it before 0x481130.
    auto local_fields = fields.transform;
    local_fields.angles[1] = 0;
    plan.actor_local = model_instance_local_matrix(local_fields);
    plan.actor_camera = model_instance_camera_matrix(local_fields, world_to_camera);
    auto matrices = character_draw_matrices(character, pose, localized, fields.transform.angles[1],
        fields.mesh_count);
    // 0x4813c4 multiplies every mesh matrix by the actor-camera matrix in place.
    for (auto& matrix : matrices) matrix = model_multiply_matrix(plan.actor_camera, matrix);
    plan.matrix_tables.push_back(std::move(matrices));
    const auto frame = std::int32_t(fields.mesh_count);
    plan.material_frame_writes.push_back(frame); // 0x481355 writes +0x98 before the mesh loop.
    for (std::size_t mesh = 0; mesh < fields.mesh_count; ++mesh)
        plan.draws.push_back({CharacterDrawSource::actor, mesh, mesh, 0, frame, draw_context});
    plan.material_frame_writes.push_back(0); // 0x481417 clears the frame before attachments.
    // 0x48142a: recorded weapon type, then the +0x6c bit 0x10 slot override, then 0x4828c0.
    std::uint32_t weapon_type = 0;
    if (attachments.weapon_present &&
        (attachments.weapon_type == 0xfc3 || attachments.weapon_type == 0xfc4))
        weapon_type = attachments.weapon_type;
    if (fields.flags_6c & 0x10) {
        if (fields.weapon_slot == 1) weapon_type = 0xfc3;
        else if (fields.weapon_slot == 2) weapon_type = 0xfc4;
    }
    // 0xc22bc0 and 0xc22d40 live inside the character matrix table (base 0xc22b80, stride
    // 0x40), so slots 1 and 7 are the attachment mount matrices once the table is written.
    // A shorter table leaves the caller supplied stale values in place. The table is looked
    // up per call because later pushes can move the vector storage.
    const auto mount = [&](std::size_t slot, const ModelMatrix& stale) -> const ModelMatrix& {
        const auto& written = plan.matrix_tables[0];
        return written.size() > slot ? written[slot] : stale;
    };
    if (weapon_type == 0) {
        plan.weapon_skipped = true; // 0x48145d skips the type lookup entirely.
    } else {
        plan.weapon_type = weapon_type;
        const auto weapon = resolve_attachment(attachments.weapon, lod_index);
        plan.weapon_skipped = !weapon.draw;
        if (weapon.draw) {
            std::vector<ModelMatrix> table{model_multiply_matrix(
                mount(1, attachment_matrices.weapon_left), attachment_matrices.weapon_right)};
            plan.matrix_tables.push_back(std::move(table));
            plan.draws.push_back({CharacterDrawSource::weapon, 0, 0, 1, 0, draw_context});
        }
    }
    // 0x481500 secondary record. The counter only changes when an extra mesh draws.
    auto counter = alternate_counter;
    bool advanced = false;
    const auto secondary = resolve_attachment(attachments.secondary, lod_index);
    plan.secondary_skipped = !secondary.draw;
    if (secondary.draw) {
        std::vector<ModelMatrix> table{model_multiply_matrix(
            mount(7, attachment_matrices.secondary_left), attachment_matrices.secondary_right)};
        plan.matrix_tables.push_back(std::move(table));
        const auto table_index = plan.matrix_tables.size() - 1;
        plan.draws.push_back({CharacterDrawSource::secondary, 0, 0, table_index, 0, draw_context});
        if ((fields.flags_6d & 8) && secondary.lod->meshes.size() > 1) {
            counter = advance_counter(counter);
            advanced = true;
            plan.material_frame_writes.push_back(std::int32_t(counter));
            plan.draws.push_back({CharacterDrawSource::secondary, 1, 0, table_index,
                std::int32_t(counter), draw_context});
            plan.material_frame_writes.push_back(0);
        }
        if ((fields.flags_6d & 0x10) && secondary.lod->meshes.size() > 2) {
            counter = advance_counter(counter);
            advanced = true;
            plan.material_frame_writes.push_back(std::int32_t(counter));
            plan.draws.push_back({CharacterDrawSource::secondary, 2, 0, table_index,
                std::int32_t(counter), draw_context});
            plan.material_frame_writes.push_back(0);
        }
        if (advanced) plan.alternate_counter = counter;
    }
    return plan;
}

} // namespace dfre
