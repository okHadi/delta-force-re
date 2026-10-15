#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dfre {

enum class InputKind { quit, key_down, key_up, text_input, pointer_move, pointer_down, pointer_up };

struct InputEvent {
    InputKind kind;
    int key = 0; // Windows virtual key value.
    int x = 0;
    int y = 0;
    std::string text; // UTF-8 text from the active game window.
};

class GameWindow {
public:
    virtual ~GameWindow() = default;
    virtual std::vector<InputEvent> poll() = 0;
    virtual void present(const std::vector<std::uint32_t>& argb) = 0;
    virtual std::uint32_t ticks() const = 0;
    virtual void yield() const = 0;
};

// Frames contain width * height pixels in 0xAARRGGBB order.
std::unique_ptr<GameWindow> create_game_window(const std::string& title, int width, int height);

} // namespace dfre
