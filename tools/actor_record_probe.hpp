#pragma once
#include "dfre/actor_behavior.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace dfre_probe {
inline std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> fields;
    std::stringstream stream(text);
    std::string field;
    while (std::getline(stream, field, separator)) fields.push_back(field);
    return fields;
}

inline std::uint32_t hex_u32(const std::string& text) {
    const std::string digits = text.rfind("0x", 0) == 0 ? text.substr(2) : text;
    std::uint32_t value = 0;
    for (char c : digits) {
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else throw std::runtime_error("bad hex");
        value = value * 16 + static_cast<std::uint32_t>(digit);
    }
    return value;
}

inline std::int64_t signed_number(const std::string& text) {
    std::size_t at = 0;
    bool negative = false;
    if (!text.empty() && text[0] == '-') {
        negative = true;
        at = 1;
    }
    if (at == text.size()) throw std::runtime_error("empty number");
    std::int64_t value = 0;
    for (; at < text.size(); ++at) {
        if (text[at] < '0' || text[at] > '9') throw std::runtime_error("bad number");
        value = value * 10 + (text[at] - '0');
    }
    return negative ? -value : value;
}

inline std::uint32_t number(const std::string& text) {
    const auto value = signed_number(text);
    if (value < 0) throw std::runtime_error("expected unsigned");
    return static_cast<std::uint32_t>(value);
}

inline std::vector<std::uint8_t> from_hex(const std::string& text) {
    if (text.size() % 2 != 0) throw std::runtime_error("odd hex");
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>(hex_u32(text.substr(i, 2))));
    return out;
}

inline std::string to_hex(const std::uint8_t* data, std::size_t size) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0xF]);
    }
    return out;
}

inline std::uint16_t rd16(const std::vector<std::uint8_t>& data, std::size_t at) {
    return static_cast<std::uint16_t>(data[at] | (static_cast<unsigned>(data[at + 1]) << 8));
}

inline std::uint32_t rd32(const std::vector<std::uint8_t>& data, std::size_t at) {
    std::uint32_t value;
    std::memcpy(&value, data.data() + at, 4);
    return value;
}

struct Parsed {
    dfre::AiActorState actor;
    dfre::AiPersonState person;
    dfre::BehaviorActorExtra actor_extra;
    dfre::BehaviorPersonExtra person_extra;
};

inline Parsed parse_state(const std::string& actor_hex, const std::string& person_hex) {
    const auto actor = from_hex(actor_hex);
    const auto person = from_hex(person_hex);
    if (actor.size() != dfre::kAiActorRecordBytes || person.size() != dfre::kAiPersonRecordBytes)
        throw std::runtime_error("bad record image size");
    Parsed out;
    out.actor.active = rd32(actor, 0) != 0 || rd32(actor, 8) != 0 || rd32(actor, 0x24) != 0;
    out.actor.definition_index = rd32(actor, 0);
    out.actor.object_id = rd32(actor, 8);
    out.actor.field_10 = rd32(actor, 0x10);
    out.actor.field_14 = rd32(actor, 0x14);
    out.actor.field_18 = rd32(actor, 0x18);
    for (std::size_t axis = 0; axis < 3; ++axis)
        out.actor.position[axis] = rd32(actor, 0x24 + axis * 4);
    out.actor.angle_yaw = rd32(actor, 0x30);
    out.actor.angle_pitch = rd32(actor, 0x34);
    out.actor.field_6c = actor[0x6C];
    out.actor.field_70 = rd16(actor, 0x70);
    out.actor.field_72 = rd16(actor, 0x72);
    out.actor.field_76 = rd16(actor, 0x76);
    out.actor.field_78 = rd32(actor, 0x78);
    out.actor.field_7c = rd32(actor, 0x7C);
    out.actor.field_fc = rd32(actor, 0xFC);
    out.actor.field_104 = rd32(actor, 0x104);
    out.actor.field_108 = rd32(actor, 0x108);
    out.actor.field_10c = rd32(actor, 0x10C);
    out.actor.field_110 = rd32(actor, 0x110);
    out.actor_extra.field_6d = actor[0x6D];
    out.actor_extra.field_74 = rd16(actor, 0x74);
    out.actor_extra.field_84 = rd32(actor, 0x84);
    out.actor_extra.field_94 = rd32(actor, 0x94);
    out.actor_extra.field_f8 = rd32(actor, 0xF8);

    out.person.owner = rd32(person, 0);
    out.person.flags24 = person[0x24];
    out.person.flags25 = person[0x25];
    for (std::size_t axis = 0; axis < 3; ++axis)
        out.person.position[axis] = rd32(person, 0x54 + axis * 4);
    out.person.angle_yaw = rd32(person, 0x60);
    out.person.field_64 = rd32(person, 0x64);
    out.person.field_68 = rd32(person, 0x68);
    out.person.field_70 = rd32(person, 0x70);
    out.person.field_74 = rd32(person, 0x74);
    out.person.field_78 = rd32(person, 0x78);
    out.person.field_7c = rd32(person, 0x7C);
    out.person.field_80 = rd32(person, 0x80);
    out.person.field_84 = rd32(person, 0x84);
    out.person.field_88 = rd32(person, 0x88);
    out.person.field_8c = rd32(person, 0x8C);
    out.person.field_90 = rd32(person, 0x90);
    out.person.field_94 = rd32(person, 0x94);
    out.person.field_98 = rd32(person, 0x98);
    out.person.field_9c = person[0x9C];
    out.person.field_ac = rd32(person, 0xAC);
    out.person.field_b0 = rd32(person, 0xB0);
    out.person_extra.field_28 = rd32(person, 0x28);
    out.person_extra.field_2c = rd32(person, 0x2C);
    out.person_extra.field_30 = rd32(person, 0x30);
    out.person_extra.field_34 = rd32(person, 0x34);
    out.person_extra.field_38 = rd32(person, 0x38);
    out.person_extra.field_3c = rd32(person, 0x3C);
    out.person_extra.field_40 = rd32(person, 0x40);
    out.person_extra.field_44 = rd32(person, 0x44);
    out.person_extra.field_48 = rd32(person, 0x48);
    out.person_extra.field_4c = rd32(person, 0x4C);
    out.person_extra.field_50 = rd32(person, 0x50);
    out.person_extra.field_6c = rd32(person, 0x6C);
    out.person_extra.field_a0 = rd32(person, 0xA0);
    out.person_extra.field_a4 = rd32(person, 0xA4);
    return out;
}

} // namespace dfre_probe
