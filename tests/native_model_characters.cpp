#include "dfre/model_characters.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

dfre::ModelMatrix identity() {
    dfre::ModelMatrix value{};
    value[0] = value[5] = value[10] = 4194304;
    return value;
}

dfre::ModelMatrix translation(std::int32_t x, std::int32_t y, std::int32_t z) {
    auto value = identity();
    value[3] = x;
    value[7] = y;
    value[11] = z;
    return value;
}

dfre::CharacterModel make_character(const std::vector<std::int32_t>& parents) {
    dfre::CharacterModel character;
    character.runtime_prepared = true;
    std::size_t offset = 0;
    for (std::size_t index = 0; index < parents.size(); ++index) {
        dfre::CharacterBone bone;
        bone.offset = offset;
        character.bones.push_back(std::move(bone));
        character.bone_map.push_back(index);
        offset += 64;
    }
    character.raw.assign(offset, 0);
    for (std::size_t index = 0; index < parents.size(); ++index) {
        const auto parent = std::uint32_t(parents[index]);
        for (unsigned byte = 0; byte < 4; ++byte)
            character.raw[index * 64 + 56 + byte] = std::uint8_t(parent >> (8 * byte));
    }
    return character;
}

std::vector<dfre::CharacterBonePose> make_pose(std::size_t count, std::int32_t angle) {
    std::vector<dfre::CharacterBonePose> pose(count);
    for (auto& bone : pose) {
        bone.angles = {angle, angle, angle};
        bone.scale = {65536, 65536, 65536};
    }
    return pose;
}
} // namespace

int main() {
    {
        // Identity fill when the record has no character.
        dfre::CharacterDrawFields fields;
        fields.transform.translation = {65536, 131072, 196608};
        fields.mesh_count = 2;
        const dfre::CharacterAttachments attachments;
        const dfre::CharacterAttachmentMatrices matrices;
        const auto plan = dfre::build_character_draw_plan(0, fields, nullptr, nullptr, nullptr,
            attachments, matrices, identity(), 0, 0);
        check(!plan.character_matrices, "identity fill flag changed");
        check(plan.actor_local == translation(65536, 131072, 196608), "actor local matrix changed");
        check(plan.actor_camera == plan.actor_local, "identity camera changed the actor matrix");
        check(plan.matrix_tables.size() == 1 && plan.matrix_tables[0].size() == 2,
            "identity fill count changed");
        check(plan.matrix_tables[0][0] == plan.actor_local && plan.matrix_tables[0][1] == plan.actor_local,
            "identity mesh matrices changed");
        check(plan.draws.size() == 2 && plan.draws[0].source == dfre::CharacterDrawSource::actor,
            "identity draw list changed");
        check(plan.draws[0].mesh_index == 0 && plan.draws[1].mesh_index == 1 &&
              plan.draws[0].matrix_index == 0 && plan.draws[1].matrix_index == 1,
            "identity draw indices changed");
        check(plan.draws[0].material_frame == 2 && plan.draws[1].material_frame == 2,
            "identity material frame changed");
        check(plan.material_frame_writes == std::vector<std::int32_t>({2, 0}), "identity frame writes changed");
        check(plan.weapon_skipped && plan.secondary_skipped && plan.weapon_type == 0, "identity skip flags changed");
        check(!plan.alternate_counter, "identity fill advanced the counter");
    }
    {
        // Pose path: the plan table equals character_mesh_matrices times the actor camera matrix,
        // and the pitch word reaches 0x481130 while the actor matrix keeps it zero.
        const auto character = make_character({-1, 0, 0});
        const auto pose = make_pose(3, 0x123456);
        dfre::ModelLod localized;
        localized.origins_localized = true;
        localized.meshes.resize(3);
        localized.meshes[1].delta_origin = {1000, 2000, 3000};
        localized.meshes[2].delta_origin = {-1000, 500, 250};
        dfre::CharacterDrawFields fields;
        fields.transform.angles = {0x200000, 0x40000000, 0x100000};
        fields.mesh_count = 3;
        const dfre::CharacterAttachments attachments;
        const dfre::CharacterAttachmentMatrices matrices;
        const auto plan = dfre::build_character_draw_plan(0, fields, &character, &pose, &localized,
            attachments, matrices, identity(), 0, 0);
        check(plan.character_matrices, "pose path flag changed");
        const auto bones = dfre::character_mesh_matrices(character, localized, pose, fields.transform.angles[1]);
        check(plan.matrix_tables[0].size() == bones.size(), "pose matrix count changed");
        for (std::size_t i = 0; i < bones.size(); ++i)
            check(plan.matrix_tables[0][i] == dfre::model_multiply_matrix(plan.actor_camera, bones[i]),
                "pose matrix multiply changed");
        auto no_pitch = fields.transform;
        no_pitch.angles[1] = 0;
        check(plan.actor_local == dfre::model_instance_local_matrix(no_pitch),
            "actor matrix kept the pitch word");
        auto other_pitch = fields;
        other_pitch.transform.angles[1] = 0x50000000;
        const auto changed = dfre::build_character_draw_plan(0, other_pitch, &character, &pose,
            &localized, attachments, matrices, identity(), 0, 0);
        check(changed.matrix_tables[0][1] != plan.matrix_tables[0][1], "pitch did not reach the bone matrices");
        auto low_pitch = fields;
        low_pitch.transform.angles[1] = 0xc0000000;
        const auto wrapped = dfre::build_character_draw_plan(0, low_pitch, &character, &pose,
            &localized, attachments, matrices, identity(), 0, 0);
        check(wrapped.matrix_tables[0][1] != plan.matrix_tables[0][1], "high-half pitch did not apply");
        try {
            auto bad = fields;
            bad.mesh_count = 2;
            dfre::build_character_draw_plan(0, bad, &character, &pose, &localized, attachments,
                matrices, identity(), 0, 0);
            check(false, "pose and mesh count mismatch accepted");
        } catch (const std::invalid_argument&) {}
    }
    {
        // Weapon attachment: resolved record, matrix product, and skip rules.
        dfre::ModelLod lod;
        lod.meshes.resize(1);
        auto* weapon_lod = &lod;
        dfre::CharacterAttachments attachments;
        attachments.weapon_present = true;
        attachments.weapon_type = 0xfc3;
        dfre::CharacterAttachmentRecord record;
        record.lod_count = 2;
        record.lods = {weapon_lod, nullptr};
        attachments.weapon = record;
        dfre::CharacterAttachmentMatrices matrices;
        matrices.weapon_left = translation(7, 8, 9);
        matrices.weapon_right = identity();
        dfre::CharacterDrawFields fields;
        fields.mesh_count = 1;
        auto plan = dfre::build_character_draw_plan(0, fields, nullptr, nullptr, nullptr, attachments,
            matrices, identity(), 0, 0);
        check(plan.weapon_type == 0xfc3 && !plan.weapon_skipped, "weapon type or skip changed");
        check(plan.draws.size() == 2 && plan.draws[1].source == dfre::CharacterDrawSource::weapon,
            "weapon draw missing");
        check(plan.matrix_tables.size() == 2 && plan.matrix_tables[1].size() == 1 &&
              plan.matrix_tables[1][0] == translation(7, 8, 9), "weapon matrix product changed");
        check(plan.draws[1].matrix_index == 0 && plan.draws[1].matrix_table == 1 &&
              plan.draws[1].material_frame == 0, "weapon draw fields changed");
        // Slot override replaces the recorded type.
        auto slot = fields;
        slot.flags_6c = 0x10;
        slot.weapon_slot = 2;
        plan = dfre::build_character_draw_plan(0, slot, nullptr, nullptr, nullptr, attachments, matrices,
            identity(), 0, 0);
        check(plan.weapon_type == 0xfc4, "weapon slot override changed");
        // LOD index past the record count and null LOD entries skip.
        plan = dfre::build_character_draw_plan(0, fields, nullptr, nullptr, nullptr, attachments, matrices,
            identity(), 1, 0);
        check(plan.weapon_skipped && plan.matrix_tables.size() == 1, "null weapon lod did not skip");
        plan = dfre::build_character_draw_plan(0, fields, nullptr, nullptr, nullptr, attachments, matrices,
            identity(), 2, 0);
        check(plan.weapon_skipped, "lod index past the weapon table did not skip");
        // A recorded type other than 0xfc3 or 0xfc4 never reaches the lookup.
        auto other = attachments;
        other.weapon_type = 0x100;
        plan = dfre::build_character_draw_plan(0, fields, nullptr, nullptr, nullptr, other, matrices,
            identity(), 0, 0);
        check(plan.weapon_type == 0 && plan.weapon_skipped, "unsupported weapon type reached the lookup");
    }
    {
        // Secondary extra meshes: counter advance order and frame writes.
        dfre::ModelLod lod;
        lod.meshes.resize(3);
        dfre::CharacterAttachments attachments;
        attachments.secondary_present = true;
        dfre::CharacterAttachmentRecord record;
        record.lod_count = 1;
        record.lods = {&lod};
        attachments.secondary = record;
        dfre::CharacterAttachmentMatrices matrices;
        matrices.secondary_left = identity();
        matrices.secondary_right = translation(4, 5, 6);
        dfre::CharacterDrawFields fields;
        fields.mesh_count = 1;
        fields.flags_6d = 0x18;
        const auto plan = dfre::build_character_draw_plan(1, fields, nullptr, nullptr, nullptr, attachments,
            matrices, identity(), 0, 0);
        check(!plan.secondary_skipped && plan.draws.size() == 4, "secondary draw count changed");
        check(plan.draws[1].source == dfre::CharacterDrawSource::secondary && plan.draws[1].mesh_index == 0,
            "secondary main draw changed");
        check(plan.draws[2].mesh_index == 1 && plan.draws[2].material_frame == 2 &&
              plan.draws[3].mesh_index == 2 && plan.draws[3].material_frame == 0,
            "secondary alternate frames changed");
        check(plan.material_frame_writes == std::vector<std::int32_t>({1, 0, 2, 0, 0, 0}),
            "secondary frame writes changed");
        check(plan.alternate_counter && *plan.alternate_counter == 0, "final counter changed");
        check(plan.matrix_tables[1][0] == translation(4, 5, 6), "secondary matrix product changed");
        // One extra mesh only: flags 8 with two meshes.
        auto one_extra = fields;
        one_extra.flags_6d = 8;
        auto small = lod;
        small.meshes.resize(2);
        attachments.secondary->lods = {&small};
        const auto single = dfre::build_character_draw_plan(1, one_extra, nullptr, nullptr, nullptr,
            attachments, matrices, identity(), 0, 0);
        check(single.draws.size() == 3 && single.alternate_counter && *single.alternate_counter == 2,
            "single extra mesh counter changed");
    }
    {
        // Attachment mounts alias the actor table slots once it is long enough: 0xc22bc0 is
        // slot 1 and 0xc22d40 is slot 7 (base 0xc22b80, stride 0x40).
        dfre::ModelLod weapon;
        weapon.meshes.resize(1);
        dfre::ModelLod secondary;
        secondary.meshes.resize(3);
        dfre::CharacterAttachments attachments;
        attachments.weapon_present = true;
        attachments.weapon_type = 0xfc3;
        attachments.weapon = dfre::CharacterAttachmentRecord{1, {&weapon}};
        attachments.secondary_present = true;
        attachments.secondary = dfre::CharacterAttachmentRecord{1, {&secondary}};
        dfre::CharacterAttachmentMatrices matrices;
        matrices.weapon_left = translation(9, 9, 9);       // stale, ignored with eight slots
        matrices.weapon_right = identity();
        matrices.secondary_left = translation(10, 10, 10); // stale, ignored with eight slots
        matrices.secondary_right = identity();
        dfre::CharacterDrawFields fields;
        fields.mesh_count = 8;
        const auto plan = dfre::build_character_draw_plan(0, fields, nullptr, nullptr, nullptr, attachments,
            matrices, identity(), 0, 0);
        check(plan.matrix_tables.size() == 3, "alias case table count changed");
        check(plan.matrix_tables[1][0] == plan.matrix_tables[0][1], "weapon mount did not alias slot 1");
        check(plan.matrix_tables[2][0] == plan.matrix_tables[0][7], "secondary mount did not alias slot 7");
        auto short_fields = fields;
        short_fields.mesh_count = 7;
        const auto short_plan = dfre::build_character_draw_plan(0, short_fields, nullptr, nullptr, nullptr,
            attachments, matrices, identity(), 0, 0);
        check(short_plan.matrix_tables[1][0] == short_plan.matrix_tables[0][1],
            "short table weapon mount changed");
        check(short_plan.matrix_tables[2][0] == translation(10, 10, 10),
            "short table did not keep the stale secondary mount");
    }
    {
        // Validation.
        dfre::CharacterDrawFields fields;
        const dfre::CharacterAttachments attachments;
        const dfre::CharacterAttachmentMatrices matrices;
        try {
            dfre::build_character_draw_plan(0, fields, nullptr, nullptr, nullptr, attachments, matrices,
                identity(), 0, 0);
            check(false, "zero mesh count accepted");
        } catch (const std::invalid_argument&) {}
        const auto character = make_character({-1});
        const auto pose = make_pose(1, 0);
        dfre::ModelLod localized;
        localized.origins_localized = true;
        localized.meshes.resize(2);
        fields.mesh_count = 2;
        try {
            dfre::build_character_draw_plan(0, fields, &character, &pose, &localized, attachments, matrices,
                identity(), 0, 0);
            check(false, "pose and mesh mismatch accepted");
        } catch (const std::invalid_argument&) {}
        dfre::CharacterAttachmentRecord broken;
        broken.lod_count = 2;
        broken.lods = {nullptr};
        auto attach = attachments;
        attach.weapon_present = true;
        attach.weapon_type = 0xfc3;
        attach.weapon = broken;
        try {
            dfre::build_character_draw_plan(0, fields, nullptr, nullptr, nullptr, attach, matrices,
                identity(), 0, 0);
            check(false, "attachment lod table size mismatch accepted");
        } catch (const std::invalid_argument&) {}
    }
    std::cout << "model characters ok\n";
    return 0;
}
