#pragma once
#include "dfre/mission.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dfre {

// Mission actor and person state, rebuilt from the original Df.exe stage that
// converts each BMS object record into a runtime actor record (0x449710) and
// the person-slot allocator it calls (0x44A0C0). The live-type count refresh
// (0x44A110) and the reset scan (0x487E10, thunk 0x488970) are included.
//
// Only the fields the original code writes are modeled. Every other byte of the
// original 0x114-byte actor record and 0xB4-byte person record stays zero, so the
// record images below are exact apart from normalized pointer fields noted on
// each image helper. Behavior the reports do not prove (field meaning, later AI
// update calls) is left untouched and reported instead of guessed.

class AiError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

constexpr std::size_t kAiActorRecordBytes = 0x114;
constexpr std::size_t kAiPersonRecordBytes = 0xB4;
constexpr std::size_t kAiPersonSlotCount = 0x500;
constexpr std::size_t kAiTypeSlotCount = 64;
constexpr std::size_t kAiTypeSlotBytes = 0x40;
constexpr std::size_t kAiTypeSlotCountOffset = 0x28;
constexpr std::size_t kAiTypeSlotPreviousOffset = 0x2C;
constexpr std::size_t kAiTypeSlotAuxOffset = 0x38;

using AiActorImage = std::array<std::uint8_t, kAiActorRecordBytes>;
using AiPersonImage = std::array<std::uint8_t, kAiPersonRecordBytes>;

// Original runtime item-definition fields read by the actor conversion.
// Original table: pointer at 0xC23810, count at 0xC23814, 0x13C bytes per record.
// Item lookup 0x4828C0 matches the first record whose +0x20 equals the BMS type id.
struct AiItemDefinition {
    std::uint32_t id = 0;        // +0x20
    std::uint8_t flags24 = 0;    // +0x24; bit 2 gives the actor a person record
    std::uint32_t field_6c = 0;  // +0x6C
    std::uint32_t field_84 = 0;  // +0x84; callback slot, actor +0x110
    std::uint32_t field_90 = 0;  // +0x90; callback slot, actor +0x10C
    std::uint16_t field_94 = 0;  // +0x94
    std::uint32_t field_28 = 0;  // +0x28; type code read by 0x487C50
    std::uint32_t field_9c = 0;  // +0x9C; timer limit read by 0x487EA0
};

class AiItemDefinitionTable {
public:
    AiItemDefinitionTable() = default;
    explicit AiItemDefinitionTable(std::vector<AiItemDefinition> entries);
    const std::vector<AiItemDefinition>& entries() const noexcept { return entries_; }
    bool empty() const noexcept { return entries_.empty(); }
    void clear_model(std::size_t index);
    std::size_t find_index(std::uint32_t id) const noexcept;
private:
    std::vector<AiItemDefinition> entries_;
};

struct AiMissionConfig {
    bool multiplayer = false;               // original 0x67F520 != 0
    std::uint32_t rank = 0;                 // original 0x67F530
    std::uint32_t animation_counter = 0;    // original 0x4F821C initial value
};

// Original person record (0xB4 bytes). Pool: 0x794680, 0x500 slots, stride 0xB4.
struct AiPersonState {
    std::uint32_t owner = 0;             // +0x00; original actor pointer, 0 = free
    std::string name = {};               // +0x04; People Names entry for BMS name_id
    std::uint8_t flags24 = 0;            // +0x24
    std::uint8_t flags25 = 0;            // +0x25
    std::uint32_t position[3]{};         // +0x54/+0x58/+0x5C; actor position copy
    std::uint32_t angle_yaw = 0;         // +0x60; actor +0x30 copy
    std::uint32_t field_64 = 0;          // +0x64 = raw[0x46]
    std::uint32_t field_68 = 0;          // +0x68 = raw[0x47]
    std::uint32_t field_70 = 0;          // +0x70
    std::uint32_t field_74 = 0;          // +0x74
    std::uint32_t field_78 = 0;          // +0x78
    std::uint32_t field_7c = 0;          // +0x7C
    std::uint32_t field_80 = 0;          // +0x80
    std::uint32_t field_84 = 0;          // +0x84
    std::uint32_t field_88 = 0;          // +0x88
    std::uint32_t field_8c = 0;          // +0x8C
    std::uint32_t field_90 = 0;          // +0x90
    std::uint32_t field_94 = 0;          // +0x94
    std::uint32_t field_98 = 0;          // +0x98
    std::uint8_t field_9c = 0;           // +0x9C = raw[0x42]
    bool tail_flag = false;              // +0xA4 = 1 when raw[0x49] != 0
    std::uint32_t field_ac = 0;          // +0xAC = raw[0x49]
    std::uint32_t field_b0 = 0;          // +0xB0 = raw[0x4A]
};

// Original actor record (0x114 bytes). An inactive record is the zeroed image
// the original leaves behind when its acceptance flags reject the BMS object.
struct AiActorState {
    bool active = false;
    std::uint32_t definition_index = 0;  // +0x00
    std::uint32_t object_id = 0;         // +0x08
    std::uint32_t field_10 = 0;          // +0x10; graphics object handle (0x402EC0 writes def+0x6C)
    std::uint32_t field_14 = 0;          // +0x14; husk handle for groups 0/1
    std::uint32_t field_18 = 0;          // +0x18; animation handle for group 3 (gates 0x487EA0)
    std::uint32_t position[3]{};         // +0x24/+0x28/+0x2C
    std::uint32_t angle_yaw = 0;         // +0x30
    std::uint32_t angle_pitch = 0;       // +0x34
    std::uint8_t field_6c = 0;           // +0x6C; raw state byte; bit 2 skips update/reset
    std::uint16_t field_70 = 0;          // +0x70 = raw[0x43]
    std::uint16_t field_72 = 0;          // +0x72 = raw[0x48]; live-type slot index
    std::uint16_t field_76 = 0;          // +0x76 = raw[0x4C]
    std::uint32_t field_78 = 0;          // +0x78; countdown decremented by 0x487EA0 group 3
    std::uint32_t field_7c = 0;          // +0x7C; only special item ids write it
    std::uint32_t field_fc = 0;          // +0xFC = animation counter % 60
    std::uint32_t field_104 = 0;         // +0x104; 0x487C50 writes the target-list start
    std::uint32_t field_108 = 0;         // +0x108; 0x487C50 writes the target-list count
    std::uint32_t field_10c = 0;         // +0x10C = def+0x90; callback slot, called as (rec)
    std::uint32_t field_110 = 0;         // +0x110 = def+0x84; callback slot, called as (rec, 0)
    std::optional<std::size_t> person_slot; // +0x0C pointer when present
};

// Original person-slot allocator (0x44A0C0): first slot whose +0x00 is zero.
class AiPersonPool {
public:
    AiPersonPool();
    std::optional<std::size_t> allocate(std::uint32_t owner);
    const std::vector<AiPersonState>& slots() const noexcept { return slots_; }
    AiPersonState& slot(std::size_t index) { return slots_[index]; }
private:
    std::vector<AiPersonState> slots_;
};

// Converts BMS objects exactly like the original loader loop calls 0x449710.
class AiActorFactory {
public:
    AiActorFactory(AiItemDefinitionTable definitions, AiMissionConfig config);
    // owner is the opaque person-owner token; the original stores the actor pointer.
    // person_name is the resolved People Names text, or empty when the original
    // RTXT lookup found nothing.
    AiActorState convert(const MissionObject& object, std::uint32_t owner, std::string_view person_name = {});
    const AiPersonPool& people() const noexcept { return people_; }
    std::uint32_t animation_counter() const noexcept { return animation_counter_; }
private:
    AiItemDefinitionTable definitions_;
    AiMissionConfig config_;
    AiPersonPool people_;
    std::uint32_t animation_counter_ = 0;
};

// Serialized record images. Normalized pointer fields:
// actor +0x04 = definition index (original pointer = 0xC23810 + index*0x13C),
// actor +0x0C = person slot index (original pointer = 0x794680 + slot*0xB4),
// person +0x00 = owner token (original actor pointer).
AiActorImage ai_actor_image(const AiActorState& actor);
AiPersonImage ai_person_image(const AiPersonState& person);

// Per-type slot table (original 0x793680, 64 records of 0x40 bytes).
// Loader-written fields: +0x00, +0x30, +0x34; refresh-written: +0x28, +0x2C, +0x38.
class AiTypeSlotTable {
public:
    std::array<std::uint8_t, kAiTypeSlotCount * kAiTypeSlotBytes> bytes{};
    std::uint32_t count(std::size_t slot) const noexcept;
    std::uint32_t previous_count(std::size_t slot) const noexcept;
    std::uint32_t group_word(std::size_t slot, std::size_t word) const noexcept;
    void set_group_record(std::size_t slot, std::uint32_t word0, std::uint32_t word2, std::uint32_t word3);
private:
    std::uint32_t read(std::size_t slot, std::size_t at) const noexcept;
};

// Original BMS loader copy 0x449E62-0x449E98: group record words 0, 2, 3
// become type-slot +0x00, +0x30, +0x34.
void load_type_slots(const std::array<std::array<std::uint32_t, 8>, 64>& group_records, AiTypeSlotTable& table);

// Original live-type refresh 0x44A110. Table visit order is group 1, group 3,
// group 0. Each vector holds exactly the loader's slot count for that group,
// including rejected (inactive) records.
void refresh_live_type_counts(const std::vector<AiActorImage>& group0,
                              const std::vector<AiActorImage>& group1,
                              const std::vector<AiActorImage>& group3,
                              AiTypeSlotTable& table);
void refresh_live_type_counts(const std::vector<AiActorState>& group0,
                              const std::vector<AiActorState>& group1,
                              const std::vector<AiActorState>& group3,
                              AiTypeSlotTable& table);

// Original reset scan 0x487E10 (thunk 0x488970). It also zeroes globals
// 0xC92A90 and 0xC92A98. Records with a zero first dword are skipped; records
// whose +0x6C byte has bit 2 set are skipped. The original then calls the
// unknown reset body FUN_00487C50 for each selected record; this function
// returns the selection only.
struct AiResetSelection {
    std::vector<std::size_t> group3;  // original table 0x7CCA80, count 0x8CFA18
    std::vector<std::size_t> group0;  // original table 0x7DDE80, count 0x8CFA1C
};
AiResetSelection select_actor_reset_targets(const std::vector<AiActorState>& group3,
                                            const std::vector<AiActorState>& group0);

} // namespace dfre
