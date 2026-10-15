#include "dfre/ai.hpp"

#include <cstring>

namespace dfre {
namespace {

std::uint8_t read_u8(const std::uint8_t* data, std::size_t at) {
    return data[at];
}

std::uint16_t read_u16(const std::uint8_t* data, std::size_t at) {
    return static_cast<std::uint16_t>(data[at] | (static_cast<unsigned>(data[at + 1]) << 8));
}

std::uint32_t read_u32(const std::uint8_t* data, std::size_t at) {
    return static_cast<std::uint32_t>(data[at]) |
           (static_cast<std::uint32_t>(data[at + 1]) << 8) |
           (static_cast<std::uint32_t>(data[at + 2]) << 16) |
           (static_cast<std::uint32_t>(data[at + 3]) << 24);
}

std::int16_t read_i16(const std::uint8_t* data, std::size_t at) {
    return static_cast<std::int16_t>(read_u16(data, at));
}

std::int32_t read_i32(const std::uint8_t* data, std::size_t at) {
    return static_cast<std::int32_t>(read_u32(data, at));
}

void write_u8(std::uint8_t* data, std::size_t at, std::uint8_t value) {
    data[at] = value;
}

void write_u16(std::uint8_t* data, std::size_t at, std::uint16_t value) {
    data[at] = static_cast<std::uint8_t>(value & 0xFF);
    data[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

void write_u32(std::uint8_t* data, std::size_t at, std::uint32_t value) {
    data[at] = static_cast<std::uint8_t>(value & 0xFF);
    data[at + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    data[at + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    data[at + 3] = static_cast<std::uint8_t>(value >> 24);
}

// Original code wraps the 32-bit value before the signed division: SHL reg,0x10
// sets the bit pattern, then SAR reg,0x1f sign-extends it into the IDIV pair.
std::uint32_t full_turn_from_16(std::int32_t value) {
    const std::int32_t shifted = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) << 16);
    return static_cast<std::uint32_t>(shifted / 360) << 16;
}

std::uint32_t fixed_16_16_over_100(std::int32_t value) {
    const std::int32_t shifted = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) << 16);
    return static_cast<std::uint32_t>(shifted / 100);
}

// Original person+0x70 helper: FILD value; FDIV 100.0; FLD1; FDIVRP ST(1)
// (ST(1) = ST(0)/ST(1)); FMUL 65536.0; truncate (0x49BAE6 switches the x87
// rounding mode to truncation for FRNDINT); FISTP.
//
// The exact integer form below equals the original for every int32 value:
// extended-precision relative error is below 2^-62 (absolute error below
// 1.5e-12). The minimum nonzero fractional gap is 1/2^31, so truncation can only differ
// from trunc(6553600/value) when 6553600 is an exact multiple of value (all
// 57 divisors verified against original x86, both signs), and those return
// the exact quotient. Use the same integer rule on each target.

std::uint32_t percent_scaled_16_16(std::int32_t value) {
    if (value == 0) return 0x10000;
    return static_cast<std::uint32_t>(
        static_cast<std::int32_t>(6553600 / static_cast<std::int64_t>(value)));
}

// Person+0x98 stores the 16.16 numerator quotient without the trailing shift
// the actor angle fields use.
std::uint32_t fixed_16_div_360(std::int32_t value) {
    const std::int32_t shifted = static_cast<std::int32_t>(static_cast<std::uint32_t>(value) << 16);
    return static_cast<std::uint32_t>(shifted / 360);
}

std::uint32_t word_times_62(std::int16_t value) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) * 62);
}

} // namespace

AiItemDefinitionTable::AiItemDefinitionTable(std::vector<AiItemDefinition> entries)
    : entries_(std::move(entries)) {
    if (entries_.empty()) throw AiError("AI definition table needs at least the null record");
}

std::size_t AiItemDefinitionTable::find_index(std::uint32_t id) const noexcept {
    for (std::size_t i = 0; i < entries_.size(); ++i)
        if (entries_[i].id == id) return i;
    return 0;
}

AiPersonPool::AiPersonPool() : slots_(kAiPersonSlotCount) {}

std::optional<std::size_t> AiPersonPool::allocate(std::uint32_t owner) {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].owner == 0) {
            slots_[i] = {};
            slots_[i].owner = owner;
            return i;
        }
    }
    return std::nullopt;
}

AiActorFactory::AiActorFactory(AiItemDefinitionTable definitions, AiMissionConfig config)
    : definitions_(std::move(definitions)), config_(config) {
    animation_counter_ = config_.animation_counter;
}

AiActorState AiActorFactory::convert(const MissionObject& object, std::uint32_t owner,
                                     std::string_view person_name) {
    const std::uint8_t* raw = object.raw.data();
    AiActorState actor;  // Inactive records stay the zeroed image.

    const std::uint8_t flags0 = read_u8(raw, 0x0C);
    const std::int32_t rank = static_cast<std::int32_t>(config_.rank);
    if ((flags0 & 0x10) != 0) {
        if (!config_.multiplayer) return actor;
        if (static_cast<std::int32_t>(read_u8(raw, 0x45)) > rank) return actor;
    }
    if ((flags0 & 0x20) != 0) {
        if (config_.multiplayer &&
            static_cast<std::int32_t>(read_u8(raw, 0x44)) < rank) {
            return actor;
        }
    }
    if ((flags0 & 0x40) != 0 && config_.multiplayer) return actor;
    if ((flags0 & 0x80) != 0 && !config_.multiplayer) return actor;

    actor.active = true;
    actor.definition_index = static_cast<std::uint32_t>(definitions_.find_index(read_u32(raw, 0x00)));
    const AiItemDefinition& definition = definitions_.entries()[actor.definition_index];
    actor.object_id = read_u32(raw, 0x08);
    for (std::size_t axis = 0; axis < 3; ++axis)
        actor.position[axis] = read_u32(raw, 0x10 + axis * 4);
    actor.angle_yaw = full_turn_from_16(90 - static_cast<std::int32_t>(read_i16(raw, 0x34)));
    actor.angle_pitch = full_turn_from_16(static_cast<std::int32_t>(read_i16(raw, 0x36)));
    actor.field_70 = read_u8(raw, 0x43);
    actor.field_72 = read_u8(raw, 0x48);
    actor.field_76 = read_u8(raw, 0x4C);
    actor.field_fc = static_cast<std::uint32_t>(static_cast<std::int32_t>(animation_counter_) % 60);
    animation_counter_ += 0x0B;
    actor.field_10c = definition.field_90;
    actor.field_110 = definition.field_84;

    if ((definition.flags24 & 4) != 0) {
        const auto slot = people_.allocate(owner);
        if (!slot) throw AiError("AI person table is full (original writes through a null pointer)");
        actor.person_slot = *slot;
        AiPersonState& person = people_.slot(*slot);
        if (read_u32(raw, 0x04) != 0 && !person_name.empty())
            person.name.assign(person_name.data(), person_name.size());
        std::uint8_t flags24 = 0, flags25 = 0;
        const std::uint8_t flags1 = read_u8(raw, 0x0D);
        if ((flags0 & 0x01) != 0) flags24 |= 0x01;
        if ((flags0 & 0x02) != 0) flags24 |= 0x02;
        if ((flags0 & 0x04) != 0) flags24 |= 0x08;
        if ((flags0 & 0x08) != 0) flags24 |= 0x04;
        if ((flags1 & 0x01) != 0) flags24 |= 0x10;
        if ((flags1 & 0x02) != 0) flags24 |= 0x20;
        if ((flags1 & 0x04) != 0) flags25 |= 0x01;
        person.flags24 = flags24;
        person.flags25 = flags25;
        person.field_64 = read_u8(raw, 0x46);
        person.field_68 = read_u8(raw, 0x47);
        person.field_9c = read_u8(raw, 0x42);
        person.field_70 = percent_scaled_16_16(read_i32(raw, 0x20));
        person.field_74 = read_i32(raw, 0x24) == 0 ? 0x10000 : fixed_16_16_over_100(read_i32(raw, 0x24));
        person.field_78 = read_i32(raw, 0x28) == 0 ? 0x10000 : fixed_16_16_over_100(read_i32(raw, 0x28));
        person.field_80 = read_i32(raw, 0x2C) == 0
                              ? 0x100000
                              : static_cast<std::uint32_t>(read_i32(raw, 0x2C)) << 16;
        person.field_84 = read_i32(raw, 0x30) == 0
                              ? 0x4000000
                              : static_cast<std::uint32_t>(read_i32(raw, 0x30)) << 16;
        person.field_7c = read_u8(raw, 0x4B) == 0
                              ? 0x20
                              : static_cast<std::uint32_t>(read_u8(raw, 0x4B) << 8) / 360;
        person.field_98 = read_u16(raw, 0x38) == 0
                              ? 0x8000
                              : fixed_16_div_360(static_cast<std::int32_t>(read_i16(raw, 0x38)));
        person.field_88 = read_u16(raw, 0x3A) == 0 ? 0x136 : word_times_62(read_i16(raw, 0x3A));
        person.field_8c = read_u16(raw, 0x3C) == 0 ? 0x136 : word_times_62(read_i16(raw, 0x3C));
        person.field_90 = read_u16(raw, 0x3E) == 0 ? 0x136 : word_times_62(read_i16(raw, 0x3E));
        person.field_94 = read_u16(raw, 0x40) == 0 ? 0x48A8 : word_times_62(read_i16(raw, 0x40));
        if (read_u8(raw, 0x49) != 0) {
            person.tail_flag = true;
            person.field_ac = read_u8(raw, 0x49);
            person.field_b0 = read_u8(raw, 0x4A);
        }
        for (std::size_t axis = 0; axis < 3; ++axis)
            person.position[axis] = actor.position[axis];
        person.angle_yaw = actor.angle_yaw;
    }

    const std::uint32_t type_id = read_u32(raw, 0x00);
    if (type_id == 0x1775) {
        actor.field_7c = read_i32(raw, 0x1C) == 0
                             ? 0x8000
                             : static_cast<std::uint32_t>(read_i32(raw, 0x1C)) << 16;
    }
    if (type_id == 0x177D) {
        actor.field_7c = read_u8(raw, 0x4A);
    }
    if (type_id == 0x1776) {
        actor.field_7c = read_i32(raw, 0x1C) == 0
                             ? 0x10000
                             : static_cast<std::uint32_t>(read_i32(raw, 0x1C)) << 16;
    }
    return actor;
}

AiActorImage ai_actor_image(const AiActorState& actor) {
    AiActorImage image{};
    if (!actor.active) return image;
    write_u32(image.data(), 0x00, actor.definition_index);
    write_u32(image.data(), 0x04, actor.definition_index);  // normalized definition pointer
    write_u32(image.data(), 0x08, actor.object_id);
    write_u32(image.data(), 0x0C,
              actor.person_slot ? static_cast<std::uint32_t>(*actor.person_slot) : 0);
    write_u32(image.data(), 0x10, actor.field_10);
    write_u32(image.data(), 0x14, actor.field_14);
    write_u32(image.data(), 0x18, actor.field_18);
    for (std::size_t axis = 0; axis < 3; ++axis)
        write_u32(image.data(), 0x24 + axis * 4, actor.position[axis]);
    write_u32(image.data(), 0x30, actor.angle_yaw);
    write_u32(image.data(), 0x34, actor.angle_pitch);
    write_u8(image.data(), 0x6C, actor.field_6c);
    write_u16(image.data(), 0x70, actor.field_70);
    write_u16(image.data(), 0x72, actor.field_72);
    write_u16(image.data(), 0x76, actor.field_76);
    write_u32(image.data(), 0x78, actor.field_78);
    write_u32(image.data(), 0x7C, actor.field_7c);
    write_u32(image.data(), 0xFC, actor.field_fc);
    write_u32(image.data(), 0x104, actor.field_104);
    write_u32(image.data(), 0x108, actor.field_108);
    write_u32(image.data(), 0x10C, actor.field_10c);
    write_u32(image.data(), 0x110, actor.field_110);
    return image;
}

AiPersonImage ai_person_image(const AiPersonState& person) {
    AiPersonImage image{};
    write_u32(image.data(), 0x00, person.owner);
    if (!person.name.empty()) {
        if (person.name.size() + 1 > 0xB0 - 0x04)
            throw AiError("AI person name exceeds the original record");
        std::memcpy(image.data() + 0x04, person.name.data(), person.name.size());
    }
    write_u8(image.data(), 0x24, person.flags24);
    write_u8(image.data(), 0x25, person.flags25);
    write_u32(image.data(), 0x54, person.position[0]);
    write_u32(image.data(), 0x58, person.position[1]);
    write_u32(image.data(), 0x5C, person.position[2]);
    write_u32(image.data(), 0x60, person.angle_yaw);
    write_u32(image.data(), 0x64, person.field_64);
    write_u32(image.data(), 0x68, person.field_68);
    write_u32(image.data(), 0x70, person.field_70);
    write_u32(image.data(), 0x74, person.field_74);
    write_u32(image.data(), 0x78, person.field_78);
    write_u32(image.data(), 0x7C, person.field_7c);
    write_u32(image.data(), 0x80, person.field_80);
    write_u32(image.data(), 0x84, person.field_84);
    write_u32(image.data(), 0x88, person.field_88);
    write_u32(image.data(), 0x8C, person.field_8c);
    write_u32(image.data(), 0x90, person.field_90);
    write_u32(image.data(), 0x94, person.field_94);
    write_u32(image.data(), 0x98, person.field_98);
    write_u8(image.data(), 0x9C, person.field_9c);
    if (person.tail_flag) {
        write_u32(image.data(), 0xA4, 1);
        write_u32(image.data(), 0xAC, person.field_ac);
        write_u32(image.data(), 0xB0, person.field_b0);
    }
    return image;
}

std::uint32_t AiTypeSlotTable::read(std::size_t slot, std::size_t at) const noexcept {
    return read_u32(bytes.data(), slot * kAiTypeSlotBytes + at);
}

std::uint32_t AiTypeSlotTable::count(std::size_t slot) const noexcept {
    return read(slot, kAiTypeSlotCountOffset);
}

std::uint32_t AiTypeSlotTable::previous_count(std::size_t slot) const noexcept {
    return read(slot, kAiTypeSlotPreviousOffset);
}

std::uint32_t AiTypeSlotTable::group_word(std::size_t slot, std::size_t word) const noexcept {
    return read(slot, word);
}

void AiTypeSlotTable::set_group_record(std::size_t slot, std::uint32_t word0, std::uint32_t word2,
                                       std::uint32_t word3) {
    write_u32(bytes.data(), slot * kAiTypeSlotBytes + 0x00, word0);
    write_u32(bytes.data(), slot * kAiTypeSlotBytes + 0x30, word2);
    write_u32(bytes.data(), slot * kAiTypeSlotBytes + 0x34, word3);
}

void load_type_slots(const std::array<std::array<std::uint32_t, 8>, 64>& group_records,
                     AiTypeSlotTable& table) {
    for (std::size_t slot = 0; slot < kAiTypeSlotCount; ++slot)
        table.set_group_record(slot, group_records[slot][0], group_records[slot][2],
                               group_records[slot][3]);
}

namespace {

void refresh_impl(const std::vector<AiActorImage>* group0, const std::vector<AiActorImage>* group1,
                  const std::vector<AiActorImage>* group3, AiTypeSlotTable& table) {
    for (std::size_t slot = 0; slot < kAiTypeSlotCount; ++slot) {
        write_u32(table.bytes.data(), slot * kAiTypeSlotBytes + kAiTypeSlotCountOffset, 0);
        write_u32(table.bytes.data(), slot * kAiTypeSlotBytes + kAiTypeSlotAuxOffset, 0);
    }
    const std::vector<AiActorImage>* order[] = {group1, group3, group0};
    for (const auto* group : order) {
        for (const auto& record : *group) {
            const std::int32_t type = static_cast<std::int32_t>(read_u32(record.data(), 0x70)) >> 16;
            if (type < 0 || type >= static_cast<std::int32_t>(kAiTypeSlotCount))
                throw AiError("AI live-type index is outside the original 64 slots");
            const std::size_t at = static_cast<std::size_t>(type) * kAiTypeSlotBytes + kAiTypeSlotCountOffset;
            write_u32(table.bytes.data(), at, read_u32(table.bytes.data(), at) + 1);
        }
    }
    write_u32(table.bytes.data(), kAiTypeSlotCountOffset, 0);
    for (std::size_t slot = 0; slot < kAiTypeSlotCount; ++slot) {
        const std::uint32_t value = table.count(slot);
        write_u32(table.bytes.data(), slot * kAiTypeSlotBytes + kAiTypeSlotPreviousOffset, value);
    }
}

} // namespace

void refresh_live_type_counts(const std::vector<AiActorImage>& group0,
                              const std::vector<AiActorImage>& group1,
                              const std::vector<AiActorImage>& group3,
                              AiTypeSlotTable& table) {
    refresh_impl(&group0, &group1, &group3, table);
}

void refresh_live_type_counts(const std::vector<AiActorState>& group0,
                              const std::vector<AiActorState>& group1,
                              const std::vector<AiActorState>& group3,
                              AiTypeSlotTable& table) {
    std::vector<AiActorImage> images0, images1, images3;
    for (const auto& state : group0) images0.push_back(ai_actor_image(state));
    for (const auto& state : group1) images1.push_back(ai_actor_image(state));
    for (const auto& state : group3) images3.push_back(ai_actor_image(state));
    refresh_impl(&images0, &images1, &images3, table);
}

AiResetSelection select_actor_reset_targets(const std::vector<AiActorState>& group3,
                                            const std::vector<AiActorState>& group0) {
    AiResetSelection selection;
    const auto select = [](const std::vector<AiActorState>& group, std::vector<std::size_t>& out) {
        for (std::size_t i = 0; i < group.size(); ++i) {
            if (group[i].definition_index != 0 && (group[i].field_6c & 2) == 0) out.push_back(i);
        }
    };
    select(group3, selection.group3);
    select(group0, selection.group0);
    return selection;
}

} // namespace dfre
