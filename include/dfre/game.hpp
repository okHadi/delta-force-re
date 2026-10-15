#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace dfre {

// Values identify the original state tables. Native code uses owned callbacks.
enum class GameScreen : std::uint32_t {
    game = 0x4cece0,
    new_soldier = 0x4cfbc0,
    main_menu = 0x4cf2ec,
    campaign = 0x4d0cd4,
    mission_setup = 0x4d3dec,
    multiplayer = 0x4e6cc0,
    options = 0x4d349c,
    credits = 0x4eb3a0,
    end = 0x4e6784
};

struct GameOptions {
    std::filesystem::path game_folder;
    std::filesystem::path save_folder;
    GameScreen initial = GameScreen::new_soldier;
    std::uint32_t frame_limit = 0; // Zero runs until the game exits.
};

int run_game(const GameOptions& options);

} // namespace dfre
