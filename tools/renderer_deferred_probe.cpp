// Probe for the renderer deferred bridge: the join of the exact DeferredPass
// to the model face/mesh draw path. Reads a script from stdin and prints
// state and target dumps that tools/check_renderer_deferred_original.py
// compares against the original executable.
//
// Commands, one per line, numbers hex:
//   frame W H LEFT TOP RIGHT BOTTOM
//   mode 0|1                     set_effect_mode (0x46bfa0)
//   enable 0|1                   draw enable (0xad3d7c bit 0)
//   steps DU DV DDZ DDS          initial frame chunk steps
//   material ADDR TABLE_A TABLE_B BITSX BITSY SEED TINY HEXBYTES
//   route FLAGS MATFLAGS ADDR X0 Y0 Z0 U0 V0 S0 X1... X2...
//   addface FLAGS ADDR V0 V1 V2  (6 fields per vertex)
//   flush
//   draw W H                     draw_spans (0x46d190)
//   mixed W H L T R B            draw_mixed (0x46be50)
//   plain W H L T R B            draw_no_context (0x46bf60)
//   effect W H L T R B           draw_deferred_effect (0x46bf10)
//   all W H L T R B              draw_all (0x485610 tail)
//   plan                         dump the patched span plan
//   counts                       dump pass counters and frame state
//   queuecount                   dump the four queue sizes
#include "dfre/renderer.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
std::vector<std::uint8_t> hex_bytes(const std::string& text) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i + 1 < text.size(); i += 2)
        bytes.push_back(std::uint8_t(std::strtoul(text.substr(i, 2).c_str(), nullptr, 16)));
    return bytes;
}
std::uint32_t hex32(const std::string& text) {
    return std::uint32_t(std::strtoul(text.c_str(), nullptr, 16));
}
std::int32_t hex_signed32(const std::string& text) {
    const auto value = std::uint32_t(std::strtoul(text.c_str(), nullptr, 16));
    return std::int32_t(value);
}
int main() {
    dfre::ModelDeferredBridge bridge;
    std::uint32_t width = 0, height = 0;
    std::map<std::uint32_t, dfre::RasterTexture32> textures;
    std::map<std::uint32_t, std::vector<std::uint32_t>> shades;
    std::string line;
    auto plane = [&](std::uint32_t w, std::uint32_t h) { return std::size_t(w) * h; };
    auto print_planes = [&](const std::vector<std::uint32_t>& colour,
        const std::vector<std::uint16_t>& depth) {
        std::cout << "colour";
        for (auto value : colour) std::cout << ' ' << std::hex << value;
        std::cout << '\n' << "depth";
        for (auto value : depth) std::cout << ' ' << std::hex << value;
        std::cout << '\n';
    };
    while (std::getline(std::cin, line)) {
        std::istringstream stream(line);
        std::string command;
        stream >> command;
        stream >> std::hex;
        if (command == "frame") {
            std::uint32_t left = 0, top = 0, right = 0, bottom = 0;
            stream >> width >> height >> left >> top >> right >> bottom;
            bridge.reset(dfre::RasterClip{std::int32_t(left), std::int32_t(top),
                std::int32_t(right), std::int32_t(bottom)}, width, height);
            std::cout << "frame\n";
        } else if (command == "mode") {
            std::uint32_t on = 0;
            stream >> on;
            bridge.set_effect_mode(on != 0);
            std::cout << "mode\n";
        } else if (command == "enable") {
            std::uint32_t on = 0;
            stream >> on;
            bridge.set_draw_enabled(on != 0);
            std::cout << "enable\n";
        } else if (command == "steps") {
            std::string du, dv, ddz, dds;
            stream >> du >> dv >> ddz >> dds;
            const std::array<std::int32_t, 4> steps{hex_signed32(du), hex_signed32(dv),
                hex_signed32(ddz), hex_signed32(dds)};
            bridge.set_chunk_steps(steps);
            std::cout << "steps\n";
        } else if (command == "material") {
            std::uint32_t address = 0, table_a = 0, table_b = 0, bits_x = 0, bits_y = 0, seed = 0;
            std::string tiny, hex;
            stream >> address >> table_a >> table_b >> bits_x >> bits_y >> seed >> tiny >> hex;
            const auto bytes = hex_bytes(hex);
            auto& texture = textures[address];
            texture.width_bits = std::uint8_t(bits_x);
            texture.height_bits = std::uint8_t(bits_y);
            texture.uniform_shade = 0;
            texture.pixels.resize(std::size_t(1) << (bits_x + bits_y));
            for (std::size_t i = 0; i < texture.pixels.size(); ++i)
                texture.pixels[i] = std::uint8_t(i * 37 + seed * 13);
            auto& shade = shades[address];
            shade.resize(64 * 256);
            for (std::size_t i = 0; i < shade.size(); ++i)
                shade[i] = std::uint32_t(i) * 0x01010101u ^ 0xffa5c3e7u;
            texture.shade_colours = shade;
            dfre::RasterMaterial32 binding;
            binding.texture = &texture;
            binding.shade_colours = &shade;
            binding.flat_index = bytes.size() > 0x14 ? bytes[0x14] : 0;
            binding.tiny_colour = hex32(tiny);
            bridge.add_material(address, bytes.data(), bytes.size(), table_a, table_b, binding);
            std::cout << "material\n";
        } else if(command=="direct") {
            std::uint32_t flags=0,address=0;stream>>flags>>address;
            std::array<dfre::RasterVertex,3> vertices{};
            for(auto& vertex:vertices){
                std::string x,y,z,u,v,shade;stream>>x>>y>>z>>u>>v>>shade;
                vertex={hex_signed32(x),hex_signed32(y),hex_signed32(z),hex_signed32(u),hex_signed32(v),hex_signed32(shade)};
            }
            std::vector<std::uint32_t> colour(plane(width,height));
            std::vector<std::uint16_t> depth(plane(width,height));
            for(std::size_t i=0;i<colour.size();++i){colour[i]=std::uint32_t(i)*1234567u^0xabcdef90u^1u;depth[i]=std::uint16_t(i*317u+17u);}
            const auto writes=bridge.draw_direct(flags,address,vertices,{0,0,std::int32_t(width)-1,std::int32_t(height)-1},width,height,colour,depth);
            std::cout<<"writes "<<std::hex<<writes<<'\n';print_planes(colour,depth);
        } else if (command == "route" || command == "addface") {
            std::uint32_t flags = 0, material_flags = 0, address = 0;
            stream >> flags;
            if (command == "route") stream >> material_flags;
            stream >> address;
            std::array<dfre::RasterVertex, 3> vertices{};
            for (auto& vertex : vertices) {
                std::string x, y, z, u, v, s;
                stream >> x >> y >> z >> u >> v >> s;
                vertex = {hex_signed32(x), hex_signed32(y), hex_signed32(z),
                    hex_signed32(u), hex_signed32(v), hex_signed32(s)};
            }
            if (command == "route") {
                const auto route = bridge.queue_face(flags, material_flags, address, vertices);
                std::cout << "route " << dfre::effect_route_name(route) << ' ' << std::hex << flags << '\n';
            } else {
                std::array<dfre::DeferredVertex, 3> fields{};
                for (std::size_t k = 0; k < 3; ++k)
                    fields[k].field = {vertices[k].x, vertices[k].y, vertices[k].depth,
                        vertices[k].u, vertices[k].v, vertices[k].shade};
                std::cout << "kept " << std::hex << bridge.add_face(flags, fields, address) << '\n';
            }
        } else if (command == "flush") {
            bridge.flush();
            std::cout << "flushed\n";
        } else if (command == "draw" || command == "mixed" || command == "plain" || command == "effect" || command == "all") {
            std::uint32_t w = 0, h = 0;
            dfre::RasterClip clip{};
            stream >> w >> h;
            if (command != "draw") {
                std::uint32_t left = 0, top = 0, right = 0, bottom = 0;
                stream >> left >> top >> right >> bottom;
                clip = dfre::RasterClip{std::int32_t(left), std::int32_t(top),
                    std::int32_t(right), std::int32_t(bottom)};
            }
            std::vector<std::uint32_t> colour(plane(w,h));
            std::vector<std::uint16_t> depth(plane(w,h));
            for(std::size_t i=0;i<colour.size();++i){
                colour[i]=std::uint32_t(i)*1234567u^0xabcdef90u^1u;
                depth[i]=std::uint16_t(i*317u+17u);
            }
            std::size_t writes = 0;
            if (command == "draw") writes = bridge.draw_spans(w, h, colour, depth);
            else if (command == "mixed") writes = bridge.draw_mixed(clip, w, h, colour, depth);
            else if (command == "plain") writes = bridge.draw_no_context(clip, w, h, colour, depth);
            else if (command == "effect") writes = bridge.draw_deferred_effect(clip, w, h, colour, depth);
            else writes = bridge.draw_all(clip, w, h, colour, depth);
            std::cout << "writes " << std::hex << writes << '\n';
            print_planes(colour, depth);
        } else if (command == "plan") {
            const auto& plan = bridge.plan();
            std::cout << "plan " << std::hex << plan.size() << '\n';
            for (const auto& span : plan)
                std::cout << "span " << std::hex << span.span.pixel << ' ' << span.span.length << ' '
                          << span.span.u << ' ' << span.span.v << ' ' << span.span.depth << ' '
                          << span.span.shade << ' ' << span.span.du << ' ' << span.span.dv << ' '
                          << span.span.ddepth << ' ' << span.span.dshade << ' ' << span.face_flags << ' '
                          << span.row << ' ' << span.record << '\n';
        } else if (command == "counts") {
            const auto& pass = bridge.pass();
            const auto& selector = bridge.selector();
            const auto& steps = bridge.chunk_steps();
            std::cout << "counts " << std::hex << pass.record_count() << ' ' << pass.node_count() << ' '
                      << pass.segment_count() << ' ' << pass.span_records() << ' ' << pass.span_pixels() << ' '
                      << pass.record_visits() << ' ' << pass.slot_visits() << ' ' << selector.key << ' '
                      << dfre::deferred_span_kernel_name(selector.kernel) << ' '
                      << std::uint32_t(steps[0]) << ' ' << std::uint32_t(steps[1]) << ' '
                      << std::uint32_t(steps[2]) << ' ' << std::uint32_t(steps[3]) << '\n';
        } else if (command == "queuecount") {
            std::cout << "queues " << std::hex << bridge.queue(dfre::EffectRoute::deferred).size() << ' '
                      << bridge.queue(dfre::EffectRoute::deferred_effect).size() << ' '
                      << bridge.queue(dfre::EffectRoute::effect_plain).size() << ' '
                      << bridge.queue(dfre::EffectRoute::deferred_no_context).size() << '\n';
        }
    }
    return 0;
}
