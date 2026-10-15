#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dfre {

enum class InputKind { quit, key_down, key_up, text_input, pointer_move, pointer_down, pointer_up,
    pointer_delta, focus_lost };

struct InputEvent {
    InputKind kind;
    int key = 0; // Windows virtual key value.
    int x = 0;
    int y = 0;
    std::string text{}; // UTF-8 text from the active game window.
    unsigned button = 0; // Pointer button: 1 left, 2 right, 3 middle. Zero is a legacy left event.
    bool extended = false; // Original key-ring flag 0x80 for an extended key.
};

class GameWindow {
public:
    virtual ~GameWindow() = default;
    virtual std::vector<InputEvent> poll() = 0;
    virtual void present(const std::vector<std::uint32_t>& argb) = 0;
    virtual std::uint32_t ticks() const = 0;
    virtual void yield() const = 0;
    virtual bool active() const { return true; }
    virtual void capture_pointer(bool) {} // Relative game input; menus use absolute coordinates.
};

// Frames contain width * height pixels in 0xAARRGGBB order.
std::unique_ptr<GameWindow> create_game_window(const std::string& title, int width, int height);

} // namespace dfre
