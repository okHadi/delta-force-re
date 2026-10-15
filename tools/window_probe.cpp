#include "dfre/assets.hpp"
#include "dfre/platform.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::cerr << "window_probe GAME_FOLDER PCX_MEMBER [FRAME_LIMIT]\n";
        return 1;
    }
    try {
        const dfre::AssetStore assets(argv[1]);
        const auto image = assets.load_image(argv[2]).image;
        if (image.palette.size() != 768) throw std::runtime_error("Image needs an RGB palette");
        std::vector<std::uint32_t> pixels;
        pixels.reserve(image.pixels.size());
        for (auto index : image.pixels) {
            const auto offset = static_cast<std::size_t>(index) * 3;
            pixels.push_back(0xff000000u | (static_cast<std::uint32_t>(image.palette[offset]) << 16) |
                             (static_cast<std::uint32_t>(image.palette[offset + 1]) << 8) |
                             image.palette[offset + 2]);
        }
        const unsigned long limit = argc == 4 ? std::stoul(argv[3]) : 0;
        auto window = dfre::create_game_window("Delta Force RE", image.width, image.height);
        unsigned long frames = 0;
        for (;;) {
            for (const auto& event : window->poll())
                if (event.kind == dfre::InputKind::quit ||
                    (event.kind == dfre::InputKind::key_down && event.key == 27)) return 0;
            window->present(pixels);
            if (limit && ++frames >= limit) return 0;
            window->yield();
        }
    } catch (const std::exception& error) {
        std::cerr << "window: " << error.what() << '\n';
        return 1;
    }
}
