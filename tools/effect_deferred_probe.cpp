// Probe for the deferred translucent pass. Reads a script from stdin and
// prints state dumps that tools/check_effect_deferred_original.py compares
// against the original executable.
//
// Commands, one per line:
//   frame W H LEFT TOP RIGHT BOTTOM DEPTH_MODE
//   material HEX_ADDR TABLE_A TABLE_B HEX_BYTES
//   face FLAGS MATERIAL_ADDR X0 Y0 Z0 U0 V0 W0 X1 Y1 Z1 U1 V1 W1 X2 Y2 Z2 U2 V2 W2
//   draw IMAGE_BASE IMAGE_HEX
//   run
#include "dfre/effect_deferred.hpp"
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
std::vector<std::uint8_t> hex_bytes(const std::string& text) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i + 1 < text.size(); i += 2)
        bytes.push_back(std::uint8_t(std::strtoul(text.substr(i, 2).c_str(), nullptr, 16)));
    return bytes;
}
int main() {
    std::uint32_t width = 0, height = 0, depth_mode = 0;
    dfre::DeferredFrame frame;
    std::vector<std::tuple<std::uint32_t, std::array<dfre::DeferredVertex, 3>, std::uint32_t>> faces;
    std::vector<std::tuple<std::uint32_t, std::vector<std::uint8_t>, std::uint32_t, std::uint32_t>> materials;
    bool do_draw = false;
    std::uint32_t texture_base = 0;
    std::vector<std::uint8_t> texture_bytes, table_bytes;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream stream(line);
        std::string command;
        stream >> command;
        stream >> std::hex;
        if (command == "frame") {
            std::int64_t left = 0, top = 0, right = 0, bottom = 0;
            stream >> width >> height >> left >> top >> right >> bottom >> depth_mode;
            stream >> frame.target_address >> frame.distance_scale >> frame.mode_flag >>
                frame.table_a_address >> frame.table_b_address;
            frame.clip_left = std::int32_t(left);
            frame.clip_top = std::int32_t(top);
            frame.clip_right = std::int32_t(right);
            frame.clip_bottom = std::int32_t(bottom);
            frame.depth_mode = depth_mode;
            frame.colour_row.clear();
            for (std::uint32_t row = 0; row <= std::uint32_t(bottom); ++row)
                frame.colour_row.push_back(row * width);
        } else if (command == "material") {
            std::uint32_t address = 0, table_a = 0, table_b = 0;
            std::string hex;
            stream >> address >> table_a >> table_b >> hex;
            materials.emplace_back(address, hex_bytes(hex), table_a, table_b);
        } else if (command == "face") {
            std::uint32_t flags = 0, material = 0;
            stream >> flags >> material;
            std::array<dfre::DeferredVertex, 3> vertices{};
            for (auto& vertex : vertices)
                for (auto& value : vertex.field) {
                    std::int64_t field = 0;
                    stream >> field;
                    value = std::int32_t(field >= 0x80000000LL ? field - 0x100000000LL : field);
                }
            faces.emplace_back(flags, vertices, material);
        } else if (command == "draw") {
            std::string image_hex;
            stream >> texture_base >> image_hex;
            texture_bytes = hex_bytes(image_hex);
            do_draw = true;
        } else if (command == "run") {
            dfre::DeferredPass pass;
            for (const auto& [address, bytes, table_a, table_b] : materials)
                pass.add_material(address, bytes.data(), bytes.size(), table_a, table_b);
            pass.reset(frame);
            std::size_t kept = 0;
            for (const auto& [flags, vertices, material] : faces)
                kept += pass.add_face(flags, vertices, material);
            pass.flush();
            std::cout << "kept " << std::hex << kept << '\n';
            const auto& state = pass.state();
            auto dump = [&](const char* label, std::size_t from, std::size_t to) {
                std::cout << label << ' ' << std::hex << from << ' ' << to << ' '
                          << dfre::DeferredPass::state_base << ' ';
                for (std::size_t address = from; address < to; ++address)
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << unsigned(state[address - dfre::DeferredPass::state_base]);
                std::cout << '\n';
            };
            const auto& raw = pass.state();
            const auto read_word = [&](std::size_t address) {
                std::uint32_t value = 0;
                for (std::size_t i = 0; i < 4; ++i)
                    value |= std::uint32_t(raw[address - dfre::DeferredPass::state_base + i]) << (i * 8);
                return value;
            };
            const auto record_cursor = read_word(0xbc5c64);
            const auto node_cursor = read_word(0xbc5c60);
            dump("records", dfre::DeferredPass::record_pool_address, record_cursor);
            dump("nodes", dfre::DeferredPass::node_pool_address, node_cursor);
            dump("globals", 0xbc59d0, 0xbc5c68);
            dump("segments", dfre::DeferredPass::segment_pool_address,
                dfre::DeferredPass::segment_pool_address + pass.segment_count() * 0x20);
            const auto& plan = pass.draw_spans();
            std::cout << "counts " << std::hex << pass.record_count() << ' ' << pass.node_count() << ' '
                      << pass.segment_count() << ' ' << pass.span_records() << ' ' << pass.span_pixels()
                      << ' ' << pass.record_visits() << ' ' << pass.slot_visits() << '\n';
            std::cout << "plan " << std::hex << plan.size() << '\n';
            for (const auto& span : plan)
                std::cout << "span " << std::hex << span.span.pixel << ' ' << span.span.length << ' ' << span.span.u << ' '
                          << span.span.v << ' ' << span.span.depth << ' ' << span.span.shade << ' '
                          << span.span.du << ' ' << span.span.dv << ' ' << span.span.ddepth << ' '
                          << span.span.dshade << ' ' << span.face_flags << ' ' << span.row << ' '
                          << span.record << '\n';
            if (do_draw) {
                std::vector<std::uint32_t> colour(std::size_t(width) * height, 0x20202020u);
                std::vector<std::uint16_t> depths(std::size_t(width) * height, 0xffffu);
                const auto resolver = [&](const dfre::DeferredSpan& span) {
                    auto context = span.context;
                    const auto texture = std::uintptr_t(context.texture);
                    const auto table = std::uintptr_t(context.blend_table);
                    context.texture = texture_bytes.empty() ? nullptr :
                        texture_bytes.data() + (texture - texture_base);
                    context.blend_table = texture_bytes.empty() ? nullptr :
                        reinterpret_cast<const std::uint32_t*>(
                            texture_bytes.data() + (table + 0x8000 - texture_base));
                    context.colour_plane = colour.data();
                    context.depth_plane = depths.data();
                    return context;
                };
                const auto writes = pass.draw(width, height, resolver);
                std::cout << "writes " << std::hex << writes << '\n';
                std::cout << "colour";
                for (auto value : colour) std::cout << ' ' << std::hex << value;
                std::cout << '\n' << "depth";
                for (auto value : depths) std::cout << ' ' << std::hex << value;
                std::cout << '\n';
            }
        }
    }
    return 0;
}
