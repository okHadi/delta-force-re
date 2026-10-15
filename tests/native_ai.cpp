#include "dfre/ai.hpp"
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void check(bool yes, const char* message) {
    if (!yes) throw std::runtime_error(message);
}

dfre::MissionObject object(std::uint32_t type_id, std::uint32_t flags) {
    dfre::MissionObject record;
    record.raw.fill(0);
    std::memcpy(record.raw.data() + 0x00, &type_id, 4);
    std::memcpy(record.raw.data() + 0x0C, &flags, 4);
    return record;
}

void put32(dfre::MissionObject& record, std::size_t at, std::uint32_t value) {
    std::memcpy(record.raw.data() + at, &value, 4);
}

void put16(dfre::MissionObject& record, std::size_t at, std::uint16_t value) {
    std::memcpy(record.raw.data() + at, &value, 2);
}

std::uint32_t get32(const dfre::AiActorImage& image, std::size_t at) {
    std::uint32_t value;
    std::memcpy(&value, image.data() + at, 4);
    return value;
}

std::uint32_t get32(const dfre::AiPersonImage& image, std::size_t at) {
    std::uint32_t value;
    std::memcpy(&value, image.data() + at, 4);
    return value;
}

dfre::AiItemDefinitionTable table(bool person = false) {
    return dfre::AiItemDefinitionTable({
        {100000, 0, 0, 0, 0, 0},
        {3012, static_cast<std::uint8_t>(person ? 4 : 0), 7, 8, 9, 10},
    });
}
} // namespace

int main() {
    try {
        // Definition lookup: first match, index 0 fallback (original 0x4828C0).
        {
            auto defs = table();
            check(defs.find_index(3012) == 1, "Definition lookup missed a match");
            check(defs.find_index(9999) == 0, "Definition lookup fallback changed");
            check(defs.find_index(100000) == 0, "Null definition lookup changed");
        }

        // Single-player acceptance flags (original 0x449736-0x44979D).
        {
            dfre::AiActorFactory sp(table(), {});
            dfre::AiMissionConfig mp_cfg;
            mp_cfg.multiplayer = true;
            mp_cfg.rank = 5;
            dfre::AiActorFactory mp(table(), mp_cfg);
            check(sp.convert(object(3012, 0x00), 1).active, "Plain object rejected in single player");
            check(sp.convert(object(3012, 0x40), 1).active, "Single-player object rejected in single player");
            check(!sp.convert(object(3012, 0x80), 1).active, "Multiplayer-only object kept in single player");
            check(!sp.convert(object(3012, 0x10), 1).active, "Ranked object kept in single player");
            check(mp.convert(object(3012, 0x80), 1).active, "Multiplayer object rejected in multiplayer");
            check(!mp.convert(object(3012, 0x40), 1).active, "Single-player object kept in multiplayer");
            check(mp.convert(object(3012, 0x10), 1).active, "Ranked object rejected at accepted rank");
            auto high = object(3012, 0x10);
            high.raw[0x45] = 6;
            check(!mp.convert(high, 1).active, "Ranked object kept above rank");
            auto low = object(3012, 0x20);
            low.raw[0x44] = 4;
            check(!mp.convert(low, 1).active, "Low-rank object kept in multiplayer");
            auto ok = object(3012, 0x20);
            ok.raw[0x44] = 5;
            check(mp.convert(ok, 1).active, "Equal-rank object rejected in multiplayer");
        }

        // Record fields and angle conversion (original 0x4497A3-0x449859).
        {
            dfre::AiActorFactory factory(table(), {});
            auto record = object(3012, 0);
            put32(record, 0x08, 0x1234);
            put32(record, 0x10, 0x11223344);
            put32(record, 0x14, 0x55667788);
            put32(record, 0x18, 0x99AABBCC);
            put16(record, 0x34, 0);
            put16(record, 0x36, 90);
            record.raw[0x43] = 0x21;
            record.raw[0x48] = 0x22;
            record.raw[0x4C] = 0x23;
            const auto actor = factory.convert(record, 0xAA);
            check(actor.active, "Converted object is inactive");
            check(actor.definition_index == 1, "Definition index changed");
            check(actor.object_id == 0x1234, "Object id changed");
            check(actor.position[0] == 0x11223344 && actor.position[1] == 0x55667788 &&
                      actor.position[2] == 0x99AABBCC,
                  "Position copy changed");
            check(actor.angle_yaw == 0x40000000, "Yaw conversion changed");
            check(actor.angle_pitch == 0x40000000, "Pitch conversion changed");
            check(actor.field_70 == 0x21 && actor.field_72 == 0x22 && actor.field_76 == 0x23,
                  "Raw byte fields changed");
            check(actor.field_fc == 0, "Animation slot changed");
            check(actor.field_10c == 9 && actor.field_110 == 8, "Definition fields changed");
            check(!actor.person_slot, "Person created without the definition flag");
            check(factory.animation_counter() == 11, "Animation counter step changed");

            auto wrap = object(3012, 0);
            put16(wrap, 0x34, 180);
            check(factory.convert(wrap, 1).angle_yaw == 0xC0000000, "Yaw wrap changed");
            put16(wrap, 0x34, static_cast<std::uint16_t>(-32768));
            put16(wrap, 0x36, static_cast<std::uint16_t>(-90));
            const auto wrapped = factory.convert(wrap, 1);
            check(wrapped.angle_pitch == 0xC0000000, "Pitch sign changed");
            const std::int32_t numerator = 90 - (-32768);
            const std::int32_t shifted = static_cast<std::int32_t>(static_cast<std::uint32_t>(numerator) << 16);
            check(wrapped.angle_yaw == (static_cast<std::uint32_t>(shifted / 360) << 16),
                  "Yaw overflow path changed");
        }

        // Person record values (original 0x449881-0x449B42).
        {
            dfre::AiActorFactory factory(table(true), {});
            auto record = object(3012, 0x0F);
            record.raw[0x0D] = 0x07;
            record.raw[0x04] = 1;
            put32(record, 0x08, 77);
            put32(record, 0x10, 11);
            put32(record, 0x14, 22);
            put32(record, 0x18, 33);
            put16(record, 0x34, 0);
            put32(record, 0x20, 7);
            put32(record, 0x24, 7);
            put32(record, 0x28, 0);
            put32(record, 0x2C, 1);
            put32(record, 0x30, 3);
            put16(record, 0x38, 256);
            put16(record, 0x3A, 256);
            put16(record, 0x3C, 0);
            put16(record, 0x3E, 1);
            put16(record, 0x40, 3);
            record.raw[0x42] = 0x55;
            record.raw[0x46] = 0x66;
            record.raw[0x47] = 0x67;
            record.raw[0x49] = 0x69;
            record.raw[0x4A] = 0x6A;
            record.raw[0x4B] = 255;
            const auto actor = factory.convert(record, 0x12345678, "GRANT");
            check(actor.person_slot && *actor.person_slot == 0, "Person slot changed");
            const auto& person = factory.people().slots()[0];
            check(person.owner == 0x12345678, "Person owner changed");
            check(person.name == "GRANT", "Person name changed");
            check(person.flags24 == 0x3F && person.flags25 == 0x01, "Person flags changed");
            check(person.field_64 == 0x66 && person.field_68 == 0x67 && person.field_9c == 0x55,
                  "Person raw bytes changed");
            check(person.field_70 == 936228, "Percent 16.16 changed");
            check(person.field_74 == 4587, "Field 74 changed");
            check(person.field_78 == 0x10000, "Field 78 default changed");
            check(person.field_80 == 0x10000 && person.field_84 == 0x30000, "Field 80/84 changed");
            check(person.field_7c == 181, "Field 7C changed");
            check(person.field_98 == 46603, "Field 98 changed");
            check(person.field_88 == 15872 && person.field_8c == 0x136 && person.field_90 == 62,
                  "Field 88/8C/90 changed");
            check(person.field_94 == 186, "Field 94 changed");
            check(person.tail_flag && person.field_ac == 0x69 && person.field_b0 == 0x6A,
                  "Tail fields changed");
            check(person.position[0] == 11 && person.position[1] == 22 && person.position[2] == 33,
                  "Person position copy changed");
            check(person.angle_yaw == 0x40000000, "Person yaw copy changed");
        }

        // Percent edge cases: zero default, exact reverse-divide form, golden
        // divisor values from original x86, and a dense comparison that catches
        // any reduced-precision or forward-formula change.
        {
            const auto make_factory = [] {
                return std::make_unique<dfre::AiActorFactory>(table(true), dfre::AiMissionConfig{});
            };
            auto factory = make_factory();
            std::size_t used = 0;
            const auto percent = [&factory, &used, &make_factory](std::int32_t value) {
                if (used == dfre::kAiPersonSlotCount) {
                    factory = make_factory();
                    used = 0;
                }
                auto record = object(3012, 0);
                put32(record, 0x20, static_cast<std::uint32_t>(value));
                const auto actor = factory->convert(record, 1);
                ++used;
                return factory->people().slots()[*actor.person_slot].field_70;
            };
            check(percent(0) == 0x10000, "Percent zero default changed");
            // Golden values proven against original 449978-44999D.
            const struct { std::int32_t in; std::uint32_t out; } golden[] = {
                {1, 6553600}, {2, 3276800}, {3, 2184533}, {4, 1638400}, {5, 1310720},
                {7, 936228}, {25, 262144}, {50, 131072}, {100, 65536}, {1024, 6400},
                {512, 12800}, {625, 10485}, {65536, 100}, {6553600, 1}, {127, 51603},
                {150, 43690}, {0x7FFFFFFF, 0}, {std::int32_t(-2147483647 - 1), 0},
                {-1, 0xFF9C0000u},
            };
            for (const auto& entry : golden)
                check(percent(entry.in) == entry.out, "Percent golden value changed");
            // Every divisor of 6553600 must return the exact quotient (the only
            // values where x87 rounding could cross the truncation boundary).
            for (int a = 0; a <= 18; ++a) {
                for (int b = 0; b <= 2; ++b) {
                    std::int64_t divisor = 1;
                    for (int i = 0; i < a; ++i) divisor *= 2;
                    for (int i = 0; i < b; ++i) divisor *= 5;
                    const auto exact = static_cast<std::uint32_t>(6553600 / divisor);
                    check(percent(static_cast<std::int32_t>(divisor)) == exact,
                          "Percent divisor value changed");
                    check(percent(static_cast<std::int32_t>(-divisor)) ==
                              static_cast<std::uint32_t>(-static_cast<std::int32_t>(exact)),
                          "Percent negative divisor value changed");
                }
            }
            for (std::int32_t value = -200000; value <= 200000; ++value) {
                if (value == 0) continue;
                const auto expected = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(6553600 / static_cast<std::int64_t>(value)));
                check(percent(value) == expected, "Percent dense value changed");
            }
        }

        // Special item ids (original 0x449B42-0x449B88).
        {
            dfre::AiActorFactory factory(table(), {});
            auto turret = object(0x1775, 0);
            check(factory.convert(turret, 1).field_7c == 0x8000, "0x1775 default changed");
            put32(turret, 0x1C, 2);
            check(factory.convert(turret, 1).field_7c == 0x20000, "0x1775 value changed");
            auto marker = object(0x177D, 0);
            marker.raw[0x4A] = 0x7F;
            check(factory.convert(marker, 1).field_7c == 0x7F, "0x177D value changed");
            auto spawn = object(0x1776, 0);
            check(factory.convert(spawn, 1).field_7c == 0x10000, "0x1776 default changed");
            put32(spawn, 0x1C, 3);
            check(factory.convert(spawn, 1).field_7c == 0x30000, "0x1776 value changed");
            check(factory.convert(object(3012, 0), 1).field_7c == 0, "Plain object got a 0x7C value");
        }

        // Record images carry the exact written fields.
        {
            dfre::AiActorFactory factory(table(true), {});
            auto record = object(3012, 0);
            record.raw[0x04] = 1;
            put32(record, 0x08, 0x99);
            put16(record, 0x34, 90);
            const auto actor = factory.convert(record, 7, "S");
            const auto actor_bytes = dfre::ai_actor_image(actor);
            check(get32(actor_bytes, 0x00) == 1 && get32(actor_bytes, 0x04) == 1,
                  "Actor image definition fields changed");
            check(get32(actor_bytes, 0x08) == 0x99, "Actor image object id changed");
            check(get32(actor_bytes, 0x0C) == 0, "Actor image person slot changed");
            check(get32(actor_bytes, 0x30) == 0, "Actor image yaw changed");
            check(actor_bytes[0x6C] == 0, "Actor image state byte changed");
            const auto person_bytes = dfre::ai_person_image(factory.people().slots()[0]);
            check(get32(person_bytes, 0x00) == 7, "Person image owner changed");
            check(std::memcmp(person_bytes.data() + 0x04, "S\0", 2) == 0, "Person image name changed");
            check(!dfre::ai_actor_image(dfre::AiActorState{}).at(0x00), "Rejected object image is not zeroed");
        }

        // Person pool capacity (original 0x44A0C0 returns 0 when full).
        {
            dfre::AiPersonPool pool;
            for (std::size_t i = 0; i < dfre::kAiPersonSlotCount; ++i) {
                check(pool.allocate(1) == i, "Person pool allocation order changed");
            }
            check(!pool.allocate(1), "Full person pool allocated");
        }

        // Loader copies and live-type refresh (original 0x449E62-0x449E98, 0x44A110).
        {
            std::array<std::array<std::uint32_t, 8>, 64> groups{};
            for (std::size_t k = 0; k < 64; ++k) {
                groups[k][0] = 0x100 + static_cast<std::uint32_t>(k);
                groups[k][2] = 0x200 + static_cast<std::uint32_t>(k);
                groups[k][3] = 0x300 + static_cast<std::uint32_t>(k);
            }
            dfre::AiTypeSlotTable slots;
            dfre::load_type_slots(groups, slots);
            for (std::size_t k = 0; k < 64; ++k) {
                check(slots.group_word(k, 0x00) == 0x100 + k, "Loader word 0 copy changed");
                check(slots.group_word(k, 0x30) == 0x200 + k, "Loader word 2 copy changed");
                check(slots.group_word(k, 0x34) == 0x300 + k, "Loader word 3 copy changed");
            }
            const auto image = [](std::uint32_t type, std::uint8_t state) {
                dfre::AiActorImage record{};
                std::memcpy(record.data() + 0x70, &type, 4);
                record[0x6C] = state;
                record[0x00] = 1;
                return record;
            };
            std::vector<dfre::AiActorImage> group0{image(0x00020000, 0), image(0x00010000, 0)};
            std::vector<dfre::AiActorImage> group1{image(0x00030000, 0), image(0x00030000, 0)};
            std::vector<dfre::AiActorImage> group3{image(0x00010000, 0), image(0x00000000, 0)};
            dfre::refresh_live_type_counts(group0, group1, group3, slots);
            check(slots.count(0) == 0 && slots.previous_count(0) == 0, "Type 0 count not forced to zero");
            check(slots.count(1) == 2 && slots.previous_count(1) == 2, "Type 1 count changed");
            check(slots.count(2) == 1 && slots.previous_count(2) == 1, "Type 2 count changed");
            check(slots.count(3) == 2 && slots.previous_count(3) == 2, "Type 3 count changed");
            for (std::size_t k = 4; k < 64; ++k)
                check(slots.count(k) == 0 && slots.previous_count(k) == 0, "Stale type slot count");
            check(slots.bytes[0x38] == 0, "Aux slot byte not cleared");
            check(slots.group_word(1, 0x30) == 0x201, "Refresh touched loader fields");
            std::vector<dfre::AiActorImage> bad{image(0x80000000, 0)};
            bool threw = false;
            try {
                dfre::refresh_live_type_counts(group0, group1, bad, slots);
            } catch (const dfre::AiError&) {
                threw = true;
            }
            check(threw, "Out-of-range live type accepted");
        }

        // Reset scan (original 0x487E10).
        {
            dfre::AiActorState live;
            live.active = true;
            live.definition_index = 3;
            dfre::AiActorState held = live;
            held.field_6c = 2;
            dfre::AiActorState freed = live;
            freed.definition_index = 0;
            const std::vector<dfre::AiActorState> group3{live, held, freed};
            const std::vector<dfre::AiActorState> group0{held, live};
            const auto selection = dfre::select_actor_reset_targets(group3, group0);
            check(selection.group3 == std::vector<std::size_t>{0}, "Group 3 reset selection changed");
            check(selection.group0 == std::vector<std::size_t>{1}, "Group 0 reset selection changed");
        }

        std::cout << "native_ai checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
