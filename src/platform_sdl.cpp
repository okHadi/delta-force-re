#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "dfre/platform.hpp"

#include <stdexcept>
#include <utility>

namespace dfre {
namespace {
[[noreturn]] void fail() { throw std::runtime_error(SDL_GetError()); }

int virtual_key(SDL_Keycode key) {
    if (key >= SDLK_a && key <= SDLK_z) return 'A' + static_cast<int>(key - SDLK_a);
    if (key >= SDLK_0 && key <= SDLK_9) return '0' + static_cast<int>(key - SDLK_0);
    if (key >= SDLK_F1 && key <= SDLK_F12) return 112 + static_cast<int>(key - SDLK_F1);
    switch (key) {
    case SDLK_RETURN: return 13;
    case SDLK_ESCAPE: return 27;
    case SDLK_SPACE: return 32;
    case SDLK_TAB: return 9;
    case SDLK_BACKSPACE: return 8;
    case SDLK_LEFT: return 37;
    case SDLK_UP: return 38;
    case SDLK_RIGHT: return 39;
    case SDLK_DOWN: return 40;
    case SDLK_DELETE: return 46;
    case SDLK_HOME: return 36;
    case SDLK_END: return 35;
    case SDLK_PAGEUP: return 33;
    case SDLK_PAGEDOWN: return 34;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return 16;
    case SDLK_LCTRL: case SDLK_RCTRL: return 17;
    case SDLK_LALT: case SDLK_RALT: return 18;
    default: return 0;
    }
}

class SdlWindow final : public GameWindow {
public:
    SdlWindow(const std::string& title, int width, int height) : width_(width), height_(height) {
        SDL_SetMainReady();
        if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER)) fail();
        try {
            window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
            if (!window_) fail();
            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
            if (!renderer_) fail();
            texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING, width, height);
            if (!texture_) fail();
            if (SDL_RenderSetLogicalSize(renderer_, width, height)) fail();
            SDL_StartTextInput();
        } catch (...) { close(); throw; }
    }

    ~SdlWindow() override { close(); }

    std::vector<InputEvent> poll() override {
        std::vector<InputEvent> result;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: result.push_back({InputKind::quit}); break;
            case SDL_KEYDOWN: case SDL_KEYUP:
                if (!event.key.repeat) {
                    const auto key = virtual_key(event.key.keysym.sym);
                    if (key) result.push_back({event.type == SDL_KEYDOWN ? InputKind::key_down : InputKind::key_up, key});
                }
                break;
            case SDL_TEXTINPUT: {
                InputEvent text{InputKind::text_input};
                text.text = event.text.text;
                result.push_back(std::move(text));
                break;
            }
            case SDL_MOUSEMOTION:
                result.push_back({InputKind::pointer_move, 0, event.motion.x, event.motion.y}); break;
            case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT)
                    result.push_back({event.type == SDL_MOUSEBUTTONDOWN ? InputKind::pointer_down : InputKind::pointer_up,
                                      0, event.button.x, event.button.y});
                break;
            default: break;
            }
        }
        return result;
    }

    void present(const std::vector<std::uint32_t>& argb) override {
        if (argb.size() != static_cast<std::size_t>(width_) * height_)
            throw std::invalid_argument("Frame size differs from the game window");
        if (SDL_UpdateTexture(texture_, nullptr, argb.data(), width_ * 4) ||
            SDL_RenderClear(renderer_) || SDL_RenderCopy(renderer_, texture_, nullptr, nullptr)) fail();
        SDL_RenderPresent(renderer_);
    }

    std::uint32_t ticks() const override { return SDL_GetTicks(); }
    void yield() const override { SDL_Delay(0); }

private:
    void close() noexcept {
        SDL_StopTextInput();
        SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER);
    }

    int width_, height_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
};
} // namespace

std::unique_ptr<GameWindow> create_game_window(const std::string& title, int width, int height) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
        throw std::invalid_argument("Invalid game window size");
    return std::make_unique<SdlWindow>(title, width, height);
}
} // namespace dfre
