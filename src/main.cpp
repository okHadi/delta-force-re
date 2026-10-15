#include "dfre/game.hpp"

#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int dfre_main(int argc, char** argv) {
    if (argc < 2 || argc % 2 != 0) {
        std::cerr << "dfre GAME_FOLDER [--frames FRAME_LIMIT] [--save-dir SAVE_FOLDER]\n";
        return 1;
    }
    try {
        dfre::GameOptions options;
        options.game_folder = argv[1];
        if (const auto* app_data = std::getenv("LOCALAPPDATA"))
            options.save_folder = std::filesystem::path(app_data) / "DeltaForceRE";
        else options.save_folder = std::filesystem::absolute(argv[0]).parent_path() / "saves";
        for (int i = 2; i < argc; i += 2) {
            if (std::string_view(argv[i]) == "--save-dir") {
                options.save_folder = argv[i + 1];
                continue;
            }
            const std::string_view count(argv[i + 1]);
            const auto parsed = std::from_chars(count.data(), count.data() + count.size(), options.frame_limit);
            if (std::string_view(argv[i]) != "--frames" || parsed.ec != std::errc{} ||
                parsed.ptr != count.data() + count.size() || !options.frame_limit)
                throw std::invalid_argument("Use --frames with a positive frame limit");
        }
        return dfre::run_game(options);
    } catch (const std::exception& error) {
        std::cerr << "dfre: " << error.what() << '\n';
        return 1;
    }
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return dfre_main(__argc, __argv); }
#else
int main(int argc, char** argv) { return dfre_main(argc, argv); }
#endif
