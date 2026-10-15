#pragma once

// Runtime item definitions: the original 0x4827A0 ITEMS.DEF load into the
// table at 0xC23810 (count 0xC23814, 0x13C bytes per record).
//
// Source: Df.exe SHA-256
// abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff.
// Addresses are original virtual addresses. Field rules are observed from the
// count callback 0x481BB0 and the populate callback 0x481BE0 (bodies in
// .local/reports/callbacks-items-input-results-20261016.json). The resource
// loader 0x45D6F0 fills the model/character/animation pointers after the load
// (0x45D6F0 body in .local/reports/model-item-load-order.json). See
// docs/item-runtime.md and tools/check_item_runtime_original.py.
//
// The script text keeps its case. Words in ITEMS.DEF ("begin", "type", the
// attrib words, ...) are compared against the callback constants with the
// case-insensitive 0x49BE20 compare, so both sides fold ASCII A-Z.

#include "dfre/items.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace dfre {

// Original record geometry. 0x4827E9 computes count * 0x13C; 0x481C44 tests
// the name against 0x1E; 0x482712 walks the four 0xC-byte ambient slots.
inline constexpr std::size_t runtime_item_size = 0x13c;
inline constexpr std::size_t runtime_item_name_max = 30;
inline constexpr std::uint32_t runtime_item_id_bias = 100000; // 0x186A0 at 0x481D16
inline constexpr std::size_t runtime_item_ambient_slots = 4;
inline constexpr std::size_t runtime_item_ambient_string = 12;

// "attrib:" word to the bit that 0x4824FE..0x4826AE ORs into +0x24. The flag
// byte is written byte-wise; the three following bytes stay zero.
enum ItemFlag : std::uint8_t {
    item_flag_good = 0x01,    // 0x4CDBA0
    item_flag_evil = 0x02,    // 0x4CDBA8
    item_flag_aidata = 0x04,  // 0x4CDBB0; bit 2 marks a live person record (0x449710)
    item_flag_fo = 0x08,      // 0x4CDBB8
    item_flag_movecb = 0x10,  // 0x4CDBBC
    item_flag_powerup = 0x20, // 0x4CDBC4
    item_flag_flesh = 0x40,   // 0x4CDBCC
    item_flag_noak = 0x80,    // 0x4CDBD4 ("NoAK")
};
std::uint8_t original_item_attrib_flag(std::string_view word) noexcept;

// "type" word to the number 0x4822D3..0x48249E writes at +0x28. Unknown words
// write nothing, so a repeated line with an unknown word keeps the old value.
enum class RuntimeItemType : std::uint32_t {
    unset = 0,
    vehicle = 1,
    decoration = 2,
    person = 3,
    marker = 4,
    building = 5,
    object = 6,
    effect = 8,
};
RuntimeItemType original_runtime_item_type(std::string_view word) noexcept;

// 0x49AFA0 atoi: skip isspace, accept one sign, fold digits with 32-bit
// wrapping multiply/add. 0x49CAAB atof: decimal strtod grammar (no inf, nan
// or hex forms), zero without digits, overflow gives +/-HUGE_VAL.
std::int32_t original_item_atoi(std::string_view text) noexcept;
double original_item_atof(std::string_view text);

// One original 0x13C-byte runtime item definition. Members sit at the original
// offsets; the reserved byte arrays keep the record exact. Strings are NUL
// terminated copies of the ITEMS.DEF tokens with no length check (only the
// name is truncated). Values default to zero like the 0x49A620 memset.
struct RuntimeItemDefinition {
    char name[32];                       // +0x000 begin value, first 30 bytes
    std::uint32_t id;                    // +0x020 atoi(id) - 100000
    std::uint8_t flags;                  // +0x024 attrib: union
    std::uint8_t reserved_25[3];         // +0x025
    std::uint32_t type;                  // +0x028 RuntimeItemType
    char graphic[16];                    // +0x02C
    char husk[16];                       // +0x03C
    char chr_file[16];                   // +0x04C
    char anim_def[16];                   // +0x05C
    std::uint32_t model;                 // +0x06C 0x45D6F0 graphic resource
    std::uint32_t husk_model;            // +0x070 0x45D6F0 husk resource
    std::uint32_t character;             // +0x074 0x45D6F0 chr_file resource (type 3)
    std::uint32_t animation;             // +0x078 0x45D6F0 anim_def resource (type 3)
    char ai_function[8];                 // +0x07C
    std::uint32_t ai_data;               // +0x084 read into actors (0x449710); no script writer
    char move_function[8];               // +0x088
    std::uint32_t move_data;             // +0x090 read into actors (0x449710); no script writer
    std::uint32_t hp;                    // +0x094
    std::uint32_t hp_secondary;          // +0x098 second hp value
    std::uint32_t effect_count;          // +0x09C 0x45D6F0 effect frame product (type 8)
    std::uint8_t reserved_a0[0x34];      // +0x0A0
    std::uint32_t phrase_set;            // +0x0D4
    std::uint32_t face_set;              // +0x0D8
    std::uint32_t acceleration;          // +0x0DC atoi << 10
    std::uint32_t turn_rate;             // +0x0E0 ((atoi << 16) / 360) >> 6
    std::int32_t axle_dist;              // +0x0E4 trunc(atof * 65536)
    char ambient[4][12];                 // +0x0E8 four 12-byte slots
    std::uint32_t ambient_handles[4];    // +0x118 0x4011C0 sound lookup results
    std::uint32_t ambient_sound[4];      // +0x128 second ambient script values
    std::uint32_t interrupt_sound_id;    // +0x138
};
static_assert(sizeof(RuntimeItemDefinition) == runtime_item_size, "record size changed");
static_assert(offsetof(RuntimeItemDefinition, name) == 0x000, "name offset");
static_assert(offsetof(RuntimeItemDefinition, id) == 0x020, "id offset");
static_assert(offsetof(RuntimeItemDefinition, flags) == 0x024, "flags offset");
static_assert(offsetof(RuntimeItemDefinition, type) == 0x028, "type offset");
static_assert(offsetof(RuntimeItemDefinition, graphic) == 0x02c, "graphic offset");
static_assert(offsetof(RuntimeItemDefinition, husk) == 0x03c, "husk offset");
static_assert(offsetof(RuntimeItemDefinition, chr_file) == 0x04c, "chr_file offset");
static_assert(offsetof(RuntimeItemDefinition, anim_def) == 0x05c, "anim_def offset");
static_assert(offsetof(RuntimeItemDefinition, model) == 0x06c, "model offset");
static_assert(offsetof(RuntimeItemDefinition, husk_model) == 0x070, "husk_model offset");
static_assert(offsetof(RuntimeItemDefinition, character) == 0x074, "character offset");
static_assert(offsetof(RuntimeItemDefinition, animation) == 0x078, "animation offset");
static_assert(offsetof(RuntimeItemDefinition, ai_function) == 0x07c, "ai_function offset");
static_assert(offsetof(RuntimeItemDefinition, ai_data) == 0x084, "ai_data offset");
static_assert(offsetof(RuntimeItemDefinition, move_function) == 0x088, "move_function offset");
static_assert(offsetof(RuntimeItemDefinition, move_data) == 0x090, "move_data offset");
static_assert(offsetof(RuntimeItemDefinition, hp) == 0x094, "hp offset");
static_assert(offsetof(RuntimeItemDefinition, hp_secondary) == 0x098, "hp_secondary offset");
static_assert(offsetof(RuntimeItemDefinition, effect_count) == 0x09c, "effect_count offset");
static_assert(offsetof(RuntimeItemDefinition, phrase_set) == 0x0d4, "phrase_set offset");
static_assert(offsetof(RuntimeItemDefinition, face_set) == 0x0d8, "face_set offset");
static_assert(offsetof(RuntimeItemDefinition, acceleration) == 0x0dc, "acceleration offset");
static_assert(offsetof(RuntimeItemDefinition, turn_rate) == 0x0e0, "turn_rate offset");
static_assert(offsetof(RuntimeItemDefinition, axle_dist) == 0x0e4, "axle_dist offset");
static_assert(offsetof(RuntimeItemDefinition, ambient) == 0x0e8, "ambient offset");
static_assert(offsetof(RuntimeItemDefinition, ambient_handles) == 0x118, "ambient_handles offset");
static_assert(offsetof(RuntimeItemDefinition, ambient_sound) == 0x128, "ambient_sound offset");
static_assert(offsetof(RuntimeItemDefinition, interrupt_sound_id) == 0x138, "interrupt_sound_id offset");

// Byte view for record comparisons (the record is trivially copyable).
inline const std::uint8_t* runtime_item_bytes(const RuntimeItemDefinition& item) noexcept {
    return reinterpret_cast<const std::uint8_t*>(&item);
}

// Maps decoded ITEMS.DEF definitions (the parent-owned ItemCatalog) to runtime
// records in file order. Repeated keys keep the last write; an unknown type
// word leaves the earlier number. Fields without a line stay zero.
std::vector<RuntimeItemDefinition> build_runtime_items(const ItemCatalog& catalog);

// Original 0x4828C0 id scan: first record whose +0x20 equals id, else zero
// (id zero is the "null" record loaded first).
std::size_t runtime_item_index(const std::vector<RuntimeItemDefinition>& items, std::uint32_t id) noexcept;

} // namespace dfre
