#include "dfre/item_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace dfre {
namespace {

// 0x49BE20 folds ASCII A-Z to a-z on both sides and returns the difference.
bool ascii_equal_ignore_case(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i];
        char y = b[i];
        if (x >= 'A' && x <= 'Z') x = char(x + 32);
        if (y >= 'A' && y <= 'Z') y = char(y + 32);
        if (x != y) return false;
    }
    return true;
}

// The original copies token strings until NUL with no bound. Keep that exact
// inside the 0x13C record; the clamp only stops writes past the record end.
void write_record_string(std::uint8_t* record, std::size_t offset, std::string_view value) noexcept {
    const std::size_t room = runtime_item_size - offset;
    const std::size_t length = std::min(value.size(), room);
    if (length) std::memcpy(record + offset, value.data(), length);
    if (length < room) record[offset + length] = 0;
}

// 0x482084: atoi << 16, signed divide by 360 (IDIV truncates toward zero),
// then arithmetic shift right 6 (SAR, floor division by 64).
std::uint32_t turn_rate_value(std::int32_t value) noexcept {
    const std::int32_t numerator = std::int32_t(std::uint32_t(value) << 16);
    const std::int32_t quotient = numerator / 360;
    if (quotient >= 0) return std::uint32_t(quotient >> 6);
    // Floor of a negative quotient divided by 64 without relying on a signed
    // shift; INT32_MIN is divisible by 64 so the plain shift is exact there.
    if (quotient == std::numeric_limits<std::int32_t>::min()) return std::uint32_t(quotient / 64);
    return std::uint32_t(-((-quotient + 63) / 64));
}

// 0x4820DA..0x482125: atof(value) * 65536.0, rounded toward zero by the
// 0x49BAE6 frndint helper, then stored by FISTP. The truncated value is
// integer-valued; FISTP writes the x87 integer indefinite 0x80000000 when it
// does not fit a 32-bit integer (and for non-finite inputs).
std::int32_t axle_dist_value(double value) noexcept {
    const double truncated = std::trunc(value * 65536.0);
    if (!(truncated >= -2147483648.0 && truncated <= 2147483647.0)) return std::int32_t(0x80000000);
    return std::int32_t(truncated);
}

std::string_view first_value(const ItemField& field) noexcept {
    return field.values.empty() ? std::string_view() : std::string_view(field.values.front());
}

std::size_t free_ambient_slot(const RuntimeItemDefinition& item) noexcept {
    for (std::size_t slot = 0; slot < runtime_item_ambient_slots; ++slot)
        if (item.ambient[slot][0] == 0) return slot;
    return runtime_item_ambient_slots;
}

} // namespace

std::uint8_t original_item_attrib_flag(std::string_view word) noexcept {
    if (ascii_equal_ignore_case(word, "good")) return item_flag_good;
    if (ascii_equal_ignore_case(word, "evil")) return item_flag_evil;
    if (ascii_equal_ignore_case(word, "aidata")) return item_flag_aidata;
    if (ascii_equal_ignore_case(word, "fo")) return item_flag_fo;
    if (ascii_equal_ignore_case(word, "movecb")) return item_flag_movecb;
    if (ascii_equal_ignore_case(word, "powerup")) return item_flag_powerup;
    if (ascii_equal_ignore_case(word, "flesh")) return item_flag_flesh;
    if (ascii_equal_ignore_case(word, "NoAK")) return item_flag_noak;
    return 0;
}

RuntimeItemType original_runtime_item_type(std::string_view word) noexcept {
    if (ascii_equal_ignore_case(word, "person")) return RuntimeItemType::person;
    if (ascii_equal_ignore_case(word, "vehicle")) return RuntimeItemType::vehicle;
    if (ascii_equal_ignore_case(word, "building")) return RuntimeItemType::building;
    if (ascii_equal_ignore_case(word, "decoration")) return RuntimeItemType::decoration;
    if (ascii_equal_ignore_case(word, "marker")) return RuntimeItemType::marker;
    if (ascii_equal_ignore_case(word, "object")) return RuntimeItemType::object;
    if (ascii_equal_ignore_case(word, "effect")) return RuntimeItemType::effect;
    return RuntimeItemType::unset;
}

std::int32_t original_item_atoi(std::string_view text) noexcept {
    std::size_t at = 0;
    while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\n' ||
                                text[at] == '\v' || text[at] == '\f' || text[at] == '\r'))
        ++at;
    bool negative = false;
    if (at < text.size() && (text[at] == '+' || text[at] == '-')) {
        negative = text[at] == '-';
        ++at;
    }
    std::uint32_t value = 0;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
        value = value * 10u + std::uint32_t(text[at] - '0');
        ++at;
    }
    return negative ? std::int32_t(0u - value) : std::int32_t(value);
}

double original_item_atof(std::string_view text) {
    // 0x49CAAB parses the decimal strtod grammar only: leading whitespace, one
    // sign, digits with one point and an optional exponent. "inf", "nan" and
    // hex forms return 0; overflow returns +/-HUGE_VAL. The accepted prefix is
    // handed to strtod, so the value is correctly rounded like the CRT pair.
    std::size_t at = 0;
    while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\n' ||
                                text[at] == '\v' || text[at] == '\f' || text[at] == '\r'))
        ++at;
    const std::size_t begin = at;
    if (at < text.size() && (text[at] == '+' || text[at] == '-')) ++at;
    std::size_t digits = 0;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
        ++at;
        ++digits;
    }
    if (at < text.size() && text[at] == '.') {
        ++at;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            ++at;
            ++digits;
        }
    }
    if (digits == 0) return 0.0;
    std::size_t end = at;
    if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
        std::size_t exponent = at + 1;
        if (exponent < text.size() && (text[exponent] == '+' || text[exponent] == '-')) ++exponent;
        const std::size_t first_exponent_digit = exponent;
        while (exponent < text.size() && text[exponent] >= '0' && text[exponent] <= '9') ++exponent;
        if (exponent > first_exponent_digit) end = exponent;
    }
    const std::string prefix(text.substr(begin, end - begin));
    return std::strtod(prefix.c_str(), nullptr);
}

std::vector<RuntimeItemDefinition> build_runtime_items(const ItemCatalog& catalog) {
    std::vector<RuntimeItemDefinition> items;
    items.reserve(catalog.entries().size());
    for (const auto& definition : catalog.entries()) {
        RuntimeItemDefinition item{}; // original 0x49A620 memset of the new table
        auto* bytes = reinterpret_cast<std::uint8_t*>(&item);
        // 0x481C44: names of 30 or more bytes stop at the 30th byte.
        const auto name = std::string_view(definition.name).substr(0, runtime_item_name_max);
        std::memcpy(item.name, name.data(), name.size());
        for (const auto& field : definition.fields) {
            const auto value = first_value(field);
            if (field.key == "id") {
                // 0x481D16 SUB EBX,0x186A0: 32-bit wrapping subtraction.
                item.id = std::uint32_t(original_item_atoi(value)) - runtime_item_id_bias;
            } else if (field.key == "type") {
                const auto type = original_runtime_item_type(value);
                if (type != RuntimeItemType::unset) item.type = std::uint32_t(type);
            } else if (field.key == "graphic") {
                write_record_string(bytes, 0x2c, value);
            } else if (field.key == "husk") {
                write_record_string(bytes, 0x3c, value);
            } else if (field.key == "chr_file") {
                write_record_string(bytes, 0x4c, value);
            } else if (field.key == "anim_def") {
                write_record_string(bytes, 0x5c, value);
            } else if (field.key == "ai_function") {
                write_record_string(bytes, 0x7c, value);
            } else if (field.key == "move_function") {
                write_record_string(bytes, 0x88, value);
            } else if (field.key == "hp") {
                item.hp = std::uint32_t(original_item_atoi(value));
                if (field.values.size() > 1)
                    item.hp_secondary = std::uint32_t(original_item_atoi(field.values[1]));
            } else if (field.key == "phrase_set") {
                item.phrase_set = std::uint32_t(original_item_atoi(value));
            } else if (field.key == "face_set") {
                item.face_set = std::uint32_t(original_item_atoi(value));
            } else if (field.key == "acceleration") {
                item.acceleration = std::uint32_t(original_item_atoi(value)) << 10;
            } else if (field.key == "turn_rate") {
                item.turn_rate = turn_rate_value(original_item_atoi(value));
            } else if (field.key == "axle_dist") {
                item.axle_dist = axle_dist_value(original_item_atof(value));
            } else if (field.key == "ambient") {
                // 0x4826E0: first slot whose first byte is zero; 0x482712
                // stops after four slots, so later lines write nothing.
                const std::size_t slot = free_ambient_slot(item);
                if (slot < runtime_item_ambient_slots) {
                    write_record_string(bytes, 0xe8 + slot * runtime_item_ambient_string, value);
                    if (field.values.size() > 1)
                        item.ambient_sound[slot] = std::uint32_t(original_item_atoi(field.values[1]));
                }
            } else if (field.key == "interrupt_sound_id") {
                item.interrupt_sound_id = std::uint32_t(original_item_atoi(value));
            } else if (field.key == "attrib:") {
                for (const auto& word : field.values) item.flags |= original_item_attrib_flag(word);
            }
        }
        items.push_back(item);
    }
    return items;
}

std::size_t runtime_item_index(const std::vector<RuntimeItemDefinition>& items, std::uint32_t id) noexcept {
    for (std::size_t index = 0; index < items.size(); ++index)
        if (items[index].id == id) return index;
    return 0;
}

} // namespace dfre
