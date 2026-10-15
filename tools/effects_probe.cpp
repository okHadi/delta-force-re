#include "dfre/effects.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
int main(int argc, char** argv) {
    const std::string command = argc > 1 ? argv[1] : "";
    if (command == "route") {
        std::uint32_t face_flags, material_flags, mode, enabled;
        while (std::cin >> face_flags >> material_flags >> mode >> enabled) {
            const dfre::EffectRouteInput input{face_flags, material_flags, mode != 0, enabled != 0};
            const auto route = dfre::route_effect_face(input);
            std::cout << dfre::effect_route_name(route) << ' '
                      << dfre::effect_pass_name(dfre::effect_pass_of(face_flags)) << ' '
                      << std::hex << (face_flags & dfre::effect_face_flag_mask) << std::dec << '\n';
        }
        return std::cin.eof() ? 0 : 2;
    }
    if (command == "flags") {
        std::uint32_t material_flags;
        while (std::cin >> material_flags)
            std::cout << std::hex << dfre::effect_face_flags(material_flags) << std::dec << '\n';
        return std::cin.eof() ? 0 : 2;
    }
    if (command == "span-map") {
        std::uint32_t face_flags;
        while (std::cin >> face_flags)
            std::cout << dfre::effect_span_selection_name(dfre::effect_span_selection(face_flags)) << '\n';
        return std::cin.eof() ? 0 : 2;
    }
    if (command == "span32") {
        std::uint32_t test, face_flags, bx, by;
        while (std::cin >> test >> face_flags >> bx >> by) {
            if (bx > 8 || by > 8) return 2;
            dfre::EffectSpan span;
            std::cin >> span.pixel >> span.length >> span.u >> span.v >> span.depth >> span.shade
                     >> span.du >> span.dv >> span.ddepth >> span.dshade;
            if (!std::cin) return 2;
            constexpr std::uint32_t width = 64, height = 8;
            if (span.pixel < 0 || span.length < 0 ||
                std::uint64_t(span.pixel) + std::uint64_t(span.length) > std::uint64_t(width) * height) return 2;
            std::vector<std::uint32_t> colours(width * height);
            std::vector<std::uint16_t> depths(width * height);
            for (std::size_t i = 0; i < colours.size(); ++i) {
                colours[i] = std::uint32_t(i) * 1234567u ^ 0xabcdef90u;
                depths[i] = std::uint16_t(std::uint32_t(i) * 317u + test * 17u);
            }
            std::vector<std::uint8_t> texture(std::size_t(1) << (bx + by));
            for (std::size_t i = 0; i < texture.size(); ++i) texture[i] = std::uint8_t(i * 37 + test * 13);
            std::vector<std::uint32_t> table(256);
            for (std::size_t i = 0; i < table.size(); ++i) table[i] = std::uint32_t(i) * 0x01010101u ^ 0xffa5c3e7u;
            dfre::EffectSpanContext context;
            context.colour = 0x00112233u ^ (test * 0x040404u);
            context.column_mask = (std::uint32_t(1) << bx) - 1;
            context.row_mask = (std::uint32_t(1) << by) - 1;
            context.row_shift = bx;
            context.depth_plane = depths.data();
            context.colour_plane = colours.data();
            context.texture = texture.data();
            context.blend_table = table.data();
            const auto selection = dfre::effect_span_selection(face_flags);
            if (selection == dfre::EffectSpanSelection::stateful_previous) return 2;
            const auto mode = selection == dfre::EffectSpanSelection::no_write ? dfre::EffectSpanMode::no_write :
                selection == dfre::EffectSpanSelection::alpha_blend ? dfre::EffectSpanMode::alpha_blend :
                dfre::EffectSpanMode::half_blend;
            dfre::draw_effect_span32(span, mode, context, width, height);
            for (std::size_t i = 0; i < colours.size(); ++i) std::cout << colours[i] << ' ' << depths[i] << ' ';
            std::cout << '\n';
        }
        return std::cin.eof() ? 0 : 2;
    }
    if (command == "plan") {
        // Feed faces and print the original drain order for the queues.
        dfre::EffectQueues queues;
        std::uint32_t face_flags, material_flags, mode;
        std::size_t index = 0;
        while (std::cin >> face_flags >> material_flags >> mode) {
            queues.set_effect_mode(mode != 0);
            queues.add(face_flags, material_flags, index++);
        }
        if (!std::cin.eof()) return 2;
        const auto deferred_plan = queues.deferred_plan();
        for (const auto& step : deferred_plan.steps)
            std::cout << "op " << (step.op == dfre::EffectDrainOp::plain ? "plain" : "deferred_span")
                      << ' ' << std::hex << step.face->flags << std::dec << '\n';
        std::cout << "flush " << deferred_plan.flushes << '\n';
        for (const auto* face : queues.plain_plan())
            std::cout << "plain " << std::hex << face->flags << std::dec << '\n';
        return 0;
    }
    if (command == "sprite" || command == "prefix") {
        if (argc != 3) return 2;
        std::ifstream input(argv[2], std::ios::binary);
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (!input && bytes.empty()) return 2;
        if (command == "sprite") {
            const auto file = dfre::decode_effect_sprite(bytes);
            std::cout << "magic " << file.magic << '\n' << "marker " << unsigned(file.marker) << '\n'
                      << "header_bytes " << file.header.size() << '\n'
                      << "texture_count " << file.texture_count << '\n';
            for (const auto& texture : file.textures)
                std::cout << "texture " << texture.name << ' ' << texture.sequence << ' '
                          << std::hex << texture.flags << std::dec << ' ' << texture.width << ' ' << texture.height
                          << ' ' << texture.pixels.size() << ' ' << texture.offset << '\n';
            for (const auto& material : file.materials)
                std::cout << "material " << material.name << ' ' << std::hex << material.flags << std::dec
                          << ' ' << unsigned(material.timer) << ' ' << unsigned(material.frame) << ' '
                          << material.offset << '\n';
            std::cout << "texture_end " << file.texture_end << '\n'
                      << "unknown " << file.unknown_begin << ' ' << file.unknown_end << '\n'
                      << "file_size " << file.file_size << '\n';
        } else {
            const auto file = dfre::decode_model_prefix(bytes);
            std::cout << "name " << file.name << '\n' << "version " << unsigned(file.version) << '\n'
                      << "lod_count " << file.lod_count << '\n' << "texture_count " << file.textures.size() << '\n';
            for (const auto& texture : file.textures)
                std::cout << "texture " << texture.name << ' ' << texture.sequence << ' '
                          << std::hex << texture.flags << std::dec << ' ' << texture.width << ' ' << texture.height
                          << ' ' << texture.pixels.size() << ' ' << texture.offset << '\n';
            std::cout << "lod_header_size " << file.lod_header.size() << '\n'
                      << "declared_lod_size " << file.declared_lod_size << '\n'
                      << "counts " << file.vertex_count << ' ' << file.normal_count << ' ' << file.face_count
                      << ' ' << file.mesh_count << ' ' << file.hardpoint_count << ' ' << file.material_count << '\n'
                      << "lod_data_offset " << file.lod_data_offset << '\n'
                      << "available_lod_bytes " << file.available_lod_bytes << '\n'
                      << "truncated " << (file.truncated ? 1 : 0) << '\n'
                      << "file_size " << file.file_size << '\n';
        }
        return 0;
    }
    std::cerr << "usage: effects_probe route|flags|span-map|span32|plan|sprite FILE|prefix FILE\n";
    return 2;
}
