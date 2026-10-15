#include "dfre/item_runtime.hpp"

#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool yes, const char* message) { if (!yes) throw std::runtime_error(message); }

// Builds one catalog from bodies. Each body is a list of field lines; the
// helper adds begin, a unique id and end.
dfre::ItemCatalog catalog_of(std::initializer_list<std::string> bodies) {
    std::string text;
    std::uint32_t id = 100000;
    for (const auto& body : bodies) {
        text += "begin \"item" + std::to_string(id) + "\"\nid " + std::to_string(id) + "\n" + body + "\nend\n";
        ++id;
    }
    return dfre::ItemCatalog(text);
}

} // namespace

int main() {
    try {
        // Geometry and defaults of the original 0x13C record.
        require(dfre::runtime_item_size == 0x13c, "record size");
        require(dfre::runtime_item_name_max == 30, "name limit");
        require(dfre::runtime_item_id_bias == 100000, "id bias");
        require(dfre::runtime_item_ambient_slots == 4, "ambient slots");
        require(dfre::runtime_item_ambient_string == 12, "ambient slot size");
        require(sizeof(dfre::RuntimeItemDefinition) == 0x13c, "struct size");
        require(offsetof(dfre::RuntimeItemDefinition, id) == 0x20, "id offset");
        require(offsetof(dfre::RuntimeItemDefinition, flags) == 0x24, "flags offset");
        require(offsetof(dfre::RuntimeItemDefinition, type) == 0x28, "type offset");
        require(offsetof(dfre::RuntimeItemDefinition, ambient) == 0xe8, "ambient offset");
        require(offsetof(dfre::RuntimeItemDefinition, interrupt_sound_id) == 0x138, "last field offset");

        // Minimal record: name, biased id and type; everything else zero.
        {
            const auto items = dfre::build_runtime_items(
                dfre::ItemCatalog("begin \"null\"\nid 100000\ntype marker\nend\n"));
            require(items.size() == 1, "one record");
            const auto& item = items[0];
            require(std::string(item.name) == "null", "name");
            require(item.id == 0, "id 100000 becomes 0");
            require(item.flags == 0, "default flags");
            require(item.type == std::uint32_t(dfre::RuntimeItemType::marker), "marker type 4");
            require(item.model == 0 && item.husk_model == 0 && item.character == 0 && item.animation == 0,
                    "resource pointers start zero");
            require(item.ai_data == 0 && item.move_data == 0, "unresolved parameters start zero");
            require(item.hp == 0 && item.hp_secondary == 0 && item.effect_count == 0, "parameters start zero");
            require(item.phrase_set == 0 && item.face_set == 0, "phrase and face start zero");
            require(item.acceleration == 0 && item.turn_rate == 0 && item.axle_dist == 0, "motion starts zero");
            require(item.interrupt_sound_id == 0, "interrupt sound starts zero");
            for (std::size_t slot = 0; slot < dfre::runtime_item_ambient_slots; ++slot) {
                require(item.ambient[slot][0] == 0, "ambient slot empty");
                require(item.ambient_sound[slot] == 0, "ambient sound empty");
            }
            require(std::string(item.graphic).empty() && std::string(item.husk).empty() &&
                    std::string(item.chr_file).empty() && std::string(item.anim_def).empty() &&
                    std::string(item.ai_function).empty() && std::string(item.move_function).empty(),
                    "string defaults");
        }

        // Names of 30 or more bytes stop at the 30th byte (0x481C44).
        {
            const auto items = dfre::build_runtime_items(
                dfre::ItemCatalog("begin \"" + std::string(30, 'a') + "\"\nid 100001\ntype object\nend\n"
                                  "begin \"" + std::string(31, 'b') + "\"\nid 100002\ntype object\nend\n"));
            require(items.size() == 2, "two names");
            require(std::string(items[0].name) == std::string(30, 'a'), "30 byte name kept");
            require(std::string(items[1].name) == std::string(30, 'b'), "31 byte name truncated");
            const auto* bytes = dfre::runtime_item_bytes(items[1]);
            require(bytes[0x1e] == 0 && bytes[0x1f] == 0, "name NUL and pad");
            const auto items2 = dfre::build_runtime_items(
                dfre::ItemCatalog("begin \"Peru palm1\"\nid 100003\ntype decoration\nend\n"));
            require(std::string(items2[0].name) == "Peru palm1", "quoted name keeps spaces and case");
        }

        // ID arithmetic: atoi value minus 100000 wraps in 32 bits.
        {
            const auto items = dfre::build_runtime_items(
                dfre::ItemCatalog("begin \"a\"\nid 099999\ntype object\nend\n"
                                  "begin \"b\"\nid 103034\ntype object\nend\n"));
            require(items[0].id == 0xffffffffu, "099999 wraps to -1");
            require(items[1].id == 3034, "biased id");
        }

        // Type words (0x4822D3..0x48249E); unknown words write nothing.
        {
            const auto items = dfre::build_runtime_items(catalog_of({
                "type person", "type vehicle", "type building", "type decoration",
                "type marker", "type object", "type effect", "type unknown", "type OBJECT"}));
            const std::uint32_t expected[9] = {3, 1, 5, 2, 4, 6, 8, 0, 6};
            for (std::size_t i = 0; i < 9; ++i) require(items[i].type == expected[i], "type mapping");
            // ItemCatalog rejects a second type line (items.cpp), so the
            // original keep-previous rule for a later unknown word is not
            // reachable through the parser; see item_runtime-needs.md.
        }

        // attrib: flags, case folding and union across lines.
        {
            const auto all = dfre::build_runtime_items(catalog_of({
                "type marker\nattrib: good evil aidata fo movecb powerup flesh NoAK"}));
            require(all[0].flags == 0xff, "all attrib bits");
            const auto mixed = dfre::build_runtime_items(catalog_of({"type marker\nattrib: AiDaTa noak"}));
            require(mixed[0].flags == 0x84, "case-insensitive attrib words");
            const auto unknown = dfre::build_runtime_items(catalog_of({"type marker\nattrib: other"}));
            require(unknown[0].flags == 0, "unknown attrib word ignored");
            const auto lines = dfre::build_runtime_items(catalog_of({
                "type marker\nattrib: good\nattrib: evil aidata"}));
            require(lines[0].flags == 0x07, "attrib lines union");
        }

        // hp: the second value exists only when a third token is present.
        {
            const auto one = dfre::build_runtime_items(catalog_of({"type person\nhp 40"}));
            require(one[0].hp == 40 && one[0].hp_secondary == 0, "single hp");
            const auto two = dfre::build_runtime_items(catalog_of({"type person\nhp 40 2"}));
            require(two[0].hp == 40 && two[0].hp_secondary == 2, "hp pair");
            const auto keep = dfre::build_runtime_items(catalog_of({"type person\nhp 40 2\nhp 7"}));
            require(keep[0].hp == 7 && keep[0].hp_secondary == 2, "second hp line keeps the stored pair value");
        }

        // Strings keep script case and land on their original offsets.
        {
            const auto items = dfre::build_runtime_items(catalog_of({
                "type person\ngraphic BanA2\nhusk Banax\nchr_file A_boom\nanim_def AniWalk\n"
                "ai_function TREE\nmove_function GnRc"}));
            const auto& item = items[0];
            require(std::string(item.graphic) == "BanA2", "graphic");
            require(std::string(item.husk) == "Banax", "husk");
            require(std::string(item.chr_file) == "A_boom", "chr_file");
            require(std::string(item.anim_def) == "AniWalk", "anim_def");
            require(std::string(item.ai_function) == "TREE", "ai_function");
            require(std::string(item.move_function) == "GnRc", "move_function");
        }

        // A shorter repeated string leaves the earlier tail bytes (no zero fill).
        {
            const auto items = dfre::build_runtime_items(catalog_of({"type object\ngraphic abcdefgh\ngraphic xy"}));
            const auto* bytes = dfre::runtime_item_bytes(items[0]);
            require(std::string(items[0].graphic) == "xy", "last graphic wins");
            const std::uint8_t expected[9] = {'x', 'y', 0, 'd', 'e', 'f', 'g', 'h', 0};
            for (std::size_t i = 0; i < 9; ++i) require(bytes[0x2c + i] == expected[i], "leftover string bytes");
        }

        // The original copies without a bound; a long graphic reaches into husk.
        {
            const auto items = dfre::build_runtime_items(catalog_of({
                "type object\ngraphic abcdefghijklmnopqrst\nhusk husk"}));
            const auto* bytes = dfre::runtime_item_bytes(items[0]);
            // 16 graphic bytes then the later husk write replace the tail.
            require(std::string(items[0].graphic) == "abcdefghijklmnophusk", "unbounded graphic copy");
            require(std::string(items[0].husk) == "husk", "husk after overflow");
            require(bytes[0x3c] == 'h' && bytes[0x3d] == 'u' && bytes[0x3e] == 's' && bytes[0x3f] == 'k',
                    "husk bytes");
            require(bytes[0x40] == 0, "terminator after husk");
        }

        // Numeric conversions.
        {
            const auto items = dfre::build_runtime_items(catalog_of({
                "type vehicle\nacceleration 3\nturn_rate 90\naxle_dist 2.5"}));
            require(items[0].acceleration == 3072, "acceleration << 10");
            require(items[0].turn_rate == 256, "turn rate ((v << 16) / 360) >> 6");
            require(items[0].axle_dist == 163840, "axle dist 2.5 * 65536");
            const auto negative = dfre::build_runtime_items(catalog_of({
                "type vehicle\naxle_dist -0.5\nturn_rate -90\naxle_dist 0.25"}));
            require(negative[0].axle_dist == 16384, "later axle_dist wins");
            require(negative[0].turn_rate == std::uint32_t(-std::int32_t(256)), "negative turn rate floors");
            const auto junk = dfre::build_runtime_items(catalog_of({"type vehicle\naxle_dist junk"}));
            require(junk[0].axle_dist == 0, "junk axle dist is zero");
        }

        // Ambient slots fill in order; sounds land at 0x128 + slot * 4; a fifth
        // line is ignored and never touches the interrupt sound.
        {
            const auto items = dfre::build_runtime_items(catalog_of({
                "type object\nambient tone1 10\nambient tone2 20\nambient tone3 30\nambient tone4 40\n"
                "ambient tone5 50"}));
            const auto& item = items[0];
            require(std::string(item.ambient[0]) == "tone1" && std::string(item.ambient[3]) == "tone4",
                    "four ambient names");
            require(item.ambient_sound[0] == 10 && item.ambient_sound[1] == 20 &&
                    item.ambient_sound[2] == 30 && item.ambient_sound[3] == 40, "ambient sounds");
            require(item.interrupt_sound_id == 0, "fifth ambient line ignored");
            const auto interrupt = dfre::build_runtime_items(catalog_of({
                "type object\nambient tone1 10\ninterrupt_sound_id 7"}));
            require(interrupt[0].interrupt_sound_id == 7, "interrupt sound id");
        }

        // Comments, blank lines and uppercase keys behave like the original.
        {
            const auto items = dfre::build_runtime_items(dfre::ItemCatalog(
                "// comment\nBEGIN \"Case\"\n  id 100001\n\n  type OBJECT\nEND\n"));
            require(items.size() == 1, "uppercase begin");
            require(std::string(items[0].name) == "Case", "uppercase begin name");
            require(items[0].type == std::uint32_t(dfre::RuntimeItemType::object), "uppercase type word");
        }

        // Helper rules.
        require(dfre::original_item_atoi("") == 0, "atoi empty");
        require(dfre::original_item_atoi("  +42xyz") == 42, "atoi leading sign");
        require(dfre::original_item_atoi("-7") == -7, "atoi negative");
        require(dfre::original_item_atoi("abc") == 0, "atoi non-digit");
        require(dfre::original_item_atoi("4295067296") == 100000, "atoi wraps in 32 bits");
        require(dfre::original_item_atof("2.5") == 2.5, "atof decimal");
        require(dfre::original_item_atof("  -1e2") == -100.0, "atof sign and exponent");
        require(dfre::original_item_atof("junk") == 0.0, "atof non-number");
        require(dfre::original_item_atof("") == 0.0, "atof empty");
        require(dfre::original_item_atof(".5") == 0.5, "atof leading point");
        require(dfre::original_item_atof("5.") == 5.0, "atof trailing point");
        require(dfre::original_item_atof("2.3e0") == 2.3, "atof exponent zero");
        require(dfre::original_item_atof("1e") == 1.0, "atof empty exponent");
        require(dfre::original_item_atof("inf") == 0.0, "atof rejects inf");
        require(dfre::original_item_atof("nan") == 0.0, "atof rejects nan");
        require(dfre::original_item_atof("0x10") == 0.0, "atof rejects hex");
        require(dfre::original_item_atof("1e999") == std::numeric_limits<double>::infinity(),
                "atof overflow is infinity");
        require(dfre::original_item_atof("-1e999") == -std::numeric_limits<double>::infinity(),
                "atof negative overflow");
        require(dfre::original_item_attrib_flag("AIDATA") == dfre::item_flag_aidata, "flag case fold");
        require(dfre::original_item_attrib_flag("NoAK") == dfre::item_flag_noak, "NoAK flag");
        require(dfre::original_item_attrib_flag("nope") == 0, "unknown flag");
        require(dfre::original_runtime_item_type("PERSON") == dfre::RuntimeItemType::person, "type case fold");
        require(dfre::original_runtime_item_type("nope") == dfre::RuntimeItemType::unset, "unknown type");

        // 0x4828C0 first-match id scan, index zero fallback.
        {
            const auto items = dfre::build_runtime_items(
                dfre::ItemCatalog("begin \"a\"\nid 100005\ntype object\nend\n"
                                  "begin \"b\"\nid 100007\ntype object\nend\n"));
            require(dfre::runtime_item_index(items, 5) == 0, "first id");
            require(dfre::runtime_item_index(items, 7) == 1, "second id");
            require(dfre::runtime_item_index(items, 99) == 0, "fallback index");
        }

        std::cout << "item runtime checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
