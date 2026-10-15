#include "dfre/menu.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
std::string quoted(std::string_view value) {
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if(c=='"' || c=='\\') out << '\\' << char(c);
        else if(c<32) out << "\\u" << std::hex << std::setfill('0') << std::setw(4) << unsigned(c);
        else out << char(c);
    }
    out << '"'; return out.str();
}
std::uint64_t hash(const std::vector<std::uint32_t>& frame) {
    std::uint64_t result=0xcbf29ce484222325ull;
    for(auto pixel:frame)for(unsigned shift:{0u,8u,16u,24u}) result=(result^((pixel>>shift)&255))*0x100000001b3ull;
    return result;
}
void ppm(const std::string& filename,const std::vector<std::uint32_t>& frame) {
    std::ofstream file(filename,std::ios::binary);file << "P6\n640 480\n255\n";
    for(auto pixel:frame)for(unsigned shift:{16u,8u,0u})file.put(char(pixel>>shift));
    if(!file)throw dfre::AssetError("Cannot write menu image");
}
}

int main(int argc,char** argv) {
    try {
        if(argc<3)throw dfre::AssetError("menu_probe all GAME_FOLDER | render GAME_FOLDER MENU OUTPUT.ppm | font GAME_FOLDER FONT");
        const std::string command=argv[1];const dfre::AssetStore assets(argv[2]);dfre::MenuRuntime runtime(assets);
        if(command=="all" && argc==3) {
            std::cout << "{\"menus\":[";bool first=true;unsigned failures=0;
            for(const auto& name:runtime.definitions().menu_order) {
                if(!first)std::cout << ',';first=false;std::cout << "{\"name\":" << quoted(name);
                try {runtime.select_menu(name);const auto& frame=runtime.render();std::cout << ",\"pixels\":" << frame.size()
                    << ",\"fnv1a64\":\"" << std::hex << hash(frame) << std::dec << '"';}
                catch(const std::exception& error){++failures;std::cout << ",\"error\":" << quoted(error.what());}
                std::cout << '}';
            }
            std::cout << "],\"failures\":" << failures << ",\"buttons\":" << runtime.definitions().buttons.size()
                << ",\"strings\":" << runtime.strings().size() << "}\n";return failures?1:0;
        }
        if(command=="render" && argc==5) {
            runtime.select_menu(argv[3]);ppm(argv[4],runtime.render());
            std::cout << "{\"menu\":" << quoted(argv[3]) << ",\"output\":" << quoted(argv[4]) << "}\n";return 0;
        }
        if(command=="font" && argc==4) {
            const auto& atlas=runtime.font_atlas(argv[3]);std::cout << "{\"baseline\":" << atlas.baseline
                << ",\"glyph_count\":" << unsigned(atlas.glyph_count) << ",\"glyphs\":[";
            for(unsigned i=0;i<256;++i){if(i)std::cout << ',';const auto& g=atlas.glyphs[i];
                std::cout << '[' << g.x << ',' << g.width << ',' << unsigned(g.height) << ',' << unsigned(g.valid) << ',' << g.y << ']';}
            std::cout << "]}\n";return 0;
        }
        throw dfre::AssetError("Invalid menu_probe arguments");
    } catch(const std::exception& error) {std::cerr << "menu_probe: " << error.what() << '\n';return 1;}
}
