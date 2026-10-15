#pragma once
#include "dfre/model_instances.hpp"
#include "dfre/models.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// Original org0 character matrix and draw callbacks 0x481130 and 0x481330.
// Targets Df.exe sha256 abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01f651ff.
//
// 0x481130 builds one 64-byte matrix per mesh from the runtime character pose:
//   record +0x18 null: each of +0x98 matrices becomes identity (0x45c910).
//   otherwise: parent chain through mesh delta origins (+0x28), pose angles (angle << 7 with
//   the first and third negated), and the record pitch word at +0x34 applied to bones 1 and 2.
//   This is character_mesh_matrices() for the localized LOD, already replay verified.
// 0x481330 then, in order:
//   1. writes global 0xad3cf4 = record +0x98 (mesh count),
//   2. builds the actor matrix with 0x45d1e0 from record +0x24 after zeroing the pitch word
//      +0x34 (restored before 0x481130 runs),
//   3. multiplies it into camera matrix 0x8d1340 (0x45cb40),
//   4. runs 0x481130 and multiplies every mesh matrix by the actor-camera matrix in place,
//   5. draws each mesh through 0x46b1d0 (mesh, matrix table, draw context, mesh index),
//   6. clears 0xad3cf4 and resolves two attachments:
//      +0x90 weapon chain: only stored types 0xfc3 and 0xfc4 participate; record +0x6c bit 0x10
//      overrides the type from record +0x70 slot 1 or 2; the caller registry resolves the type
//      to a weapon record whose +0x50 table selects the LOD by the callback's LOD index.
//      +0x8c secondary record: its +0x50 table selects the LOD the same way.
//      Weapon and secondary matrices are the stored products 0xc22bc0 x 0xc22b00 and
//      0xc22d40 x 0xc22b40; only LOD meshes above the count guards draw.
//   7. record +0x6d bits 8 and 0x10 draw secondary meshes 1 and 2, each advancing the
//      alternate counter at 0xc23800 (old + 1, wrap to 0 above 2) and writing it to 0xad3cf4
//      for the draw, then clearing 0xad3cf4.
// Missing pointers stay explicit nulls in this API: the caller supplies resolved records and
// null pointers reproduce the original skip behavior. No attachment is guessed here.

namespace dfre {

// Runtime instance fields 0x481330 reads. +0x24 translation and +0x30/+0x34/+0x38 angles use
// the model_instances.hpp layout (16.16 translation, full-turn yaw, pitch, roll).
struct CharacterDrawFields {
    ModelInstanceFields transform{};
    std::uint8_t flags_6c{};   // +0x6c: bit 0x10 enables the weapon slot override
    std::uint8_t flags_6d{};   // +0x6d: bit 8 draws secondary mesh 1, bit 0x10 mesh 2
    std::uint16_t weapon_slot{}; // +0x70
    std::size_t mesh_count{};  // +0x98
};

// One caller resolved attachment record (original +0x90 weapon record or +0x8c secondary
// record). A null optional is the original null pointer; entries may also be null.
struct CharacterAttachmentRecord {
    std::size_t lod_count{};                                      // +0x14
    std::vector<const ModelLod*> lods;                            // +0x50 + index * 4
};

struct CharacterAttachments {
    bool weapon_present{};          // +0x90 and [+0x90][4] non-null
    std::uint32_t weapon_type{};    // [+0x90][4][0x20]: only 0xfc3 and 0xfc4 participate
    std::optional<CharacterAttachmentRecord> weapon{};
    bool secondary_present{};       // +0x8c and [+0x8c][0x6c] non-null
    std::optional<CharacterAttachmentRecord> secondary{};
};

// Stored attachment matrices: 0xc22bc0 x 0xc22b00 for the weapon and 0xc22d40 x 0xc22b40 for
// the secondary record. 0xc22bc0 and 0xc22d40 alias actor matrix table slots 1 and 7, so after
// the table is written the left operands are those slots; the values here are the stale fallback
// used only when the table is too short for the slot.
struct CharacterAttachmentMatrices {
    ModelMatrix weapon_left{}, weapon_right{}, secondary_left{}, secondary_right{};
};

enum class CharacterDrawSource { actor, weapon, secondary };

struct CharacterDrawItem {
    CharacterDrawSource source{};
    std::size_t mesh_index{};   // index into the source mesh array (+0x9c + index * 0x70)
    std::size_t matrix_index{}; // 0x46b1d0 param 4
    std::size_t matrix_table{}; // index into CharacterDrawPlan::matrix_tables
    std::int32_t material_frame{}; // value of 0xad3cf4 during this draw
    std::uint32_t draw_context{};  // 0x46b1d0 param 3, the callback's opaque argument
};

struct CharacterDrawPlan {
    bool character_matrices{};    // false when record +0x18 is null (identity fill)
    ModelMatrix actor_local{};    // 0x45d1e0 with the pitch word zeroed
    ModelMatrix actor_camera{};   // 0x45cb40(camera, actor_local, actor_local)
    std::vector<std::vector<ModelMatrix>> matrix_tables; // 0 actor, 1 weapon, 2 secondary
    std::vector<CharacterDrawItem> draws;                // original order
    std::vector<std::int32_t> material_frame_writes;     // ordered 0xad3cf4 writes
    std::optional<std::uint32_t> alternate_counter{};    // new 0xc23800 value when advanced
    std::uint32_t weapon_type{};  // resolved 0x4828c0 argument, 0 when the lookup is skipped
    bool weapon_skipped{};        // original skip outcome for +0x90
    bool secondary_skipped{};     // original skip outcome for +0x8c
};

// Original 0x481130 alone. character null is the record +0x18 null identity fill.
std::vector<ModelMatrix> character_draw_matrices(const CharacterModel* character,
    const std::vector<CharacterBonePose>* pose, const ModelLod* localized, std::uint32_t pitch,
    std::size_t mesh_count);

// Original 0x481330. alternate_counter is 0xc23800 before the call. draw_context is the
// callback's opaque second argument, passed to every mesh draw. lod_index is the callback's
// fourth argument and selects the attachment LOD.
CharacterDrawPlan build_character_draw_plan(std::uint32_t alternate_counter,
    const CharacterDrawFields& fields, const CharacterModel* character,
    const std::vector<CharacterBonePose>* pose, const ModelLod* localized,
    const CharacterAttachments& attachments, const CharacterAttachmentMatrices& attachment_matrices,
    const ModelMatrix& world_to_camera, std::size_t lod_index, std::uint32_t draw_context);

} // namespace dfre
