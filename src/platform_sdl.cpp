#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "dfre/platform.hpp"

#include <stdexcept>
#include <utility>

namespace dfre {
namespace {
[[noreturn]] void fail() { throw std::runtime_error(SDL_GetError()); }

int virtual_key(SDL_Keycode key, SDL_Keymod modifiers) {
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
    case SDLK_EQUALS: return 187;
    case SDLK_MINUS: return 189;
    case SDLK_COMMA: return 188;
    case SDLK_PERIOD: return 190;
    case SDLK_SLASH: return 191;
    case SDLK_SEMICOLON: return 186;
    case SDLK_QUOTE: return 222;
    case SDLK_LEFTBRACKET: return 219;
    case SDLK_RIGHTBRACKET: return 221;
    case SDLK_BACKSLASH: return 220;
    case SDLK_BACKQUOTE: return 192;
    case SDLK_INSERT: return 45;
    case SDLK_PRINTSCREEN: return 44;
    case SDLK_PAUSE: return 19;
    case SDLK_CAPSLOCK: return 20;
    case SDLK_NUMLOCKCLEAR: return 144;
    case SDLK_SCROLLLOCK: return 145;
    case SDLK_LGUI: return 91;
    case SDLK_RGUI: return 92;
    case SDLK_APPLICATION: return 93;
    case SDLK_KP_ENTER: return 13;
    case SDLK_KP_PLUS: return 107;
    case SDLK_KP_MINUS: return 109;
    case SDLK_KP_MULTIPLY: return 106;
    case SDLK_KP_DIVIDE: return 111;
    case SDLK_KP_0: return modifiers & KMOD_NUM ? 96 : 45;
    case SDLK_KP_1: return modifiers & KMOD_NUM ? 97 : 35;
    case SDLK_KP_2: return modifiers & KMOD_NUM ? 98 : 40;
    case SDLK_KP_3: return modifiers & KMOD_NUM ? 99 : 34;
    case SDLK_KP_4: return modifiers & KMOD_NUM ? 100 : 37;
    case SDLK_KP_5: return modifiers & KMOD_NUM ? 101 : 12;
    case SDLK_KP_6: return modifiers & KMOD_NUM ? 102 : 39;
    case SDLK_KP_7: return modifiers & KMOD_NUM ? 103 : 36;
    case SDLK_KP_8: return modifiers & KMOD_NUM ? 104 : 38;
    case SDLK_KP_9: return modifiers & KMOD_NUM ? 105 : 33;
    case SDLK_KP_PERIOD: return modifiers & KMOD_NUM ? 110 : 46;
    default: return 0;
    }
}
bool extended_key(SDL_Keycode key) {
    switch(key) {
    case SDLK_LEFT: case SDLK_UP: case SDLK_RIGHT: case SDLK_DOWN:
    case SDLK_HOME: case SDLK_END: case SDLK_PAGEUP: case SDLK_PAGEDOWN:
    case SDLK_INSERT: case SDLK_DELETE: case SDLK_RCTRL: case SDLK_RALT:
    case SDLK_KP_ENTER: case SDLK_KP_DIVIDE: case SDLK_LGUI: case SDLK_RGUI:
    case SDLK_APPLICATION: case SDLK_PRINTSCREEN: case SDLK_NUMLOCKCLEAR: return true;
    default: return false;
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
            // The original frame high byte stores luminance; display its RGB values directly.
            if (SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE)) fail();
            if (SDL_RenderSetLogicalSize(renderer_, width, height)) fail();
            SDL_StartTextInput();
        } catch (...) { close(); throw; }
    }

    ~SdlWindow() override { close(); }

    std::vector<InputEvent> poll() override {
        std::vector<InputEvent> result;
        std::uint32_t delta_x=0,delta_y=0;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_QUIT: result.push_back({InputKind::quit}); break;
            case SDL_KEYDOWN: case SDL_KEYUP:
                if (!event.key.repeat) {
                    const auto key = virtual_key(event.key.keysym.sym,SDL_Keymod(event.key.keysym.mod));
                    if (key) {
                        InputEvent input{event.type == SDL_KEYDOWN ? InputKind::key_down : InputKind::key_up, key};
                        input.extended=extended_key(event.key.keysym.sym);
                        result.push_back(std::move(input));
                    }
                }
                break;
            case SDL_TEXTINPUT: {
                InputEvent text{InputKind::text_input};
                text.text = event.text.text;
                result.push_back(std::move(text));
                break;
            }
            case SDL_MOUSEMOTION:
                result.push_back({InputKind::pointer_move, 0, event.motion.x, event.motion.y});
                if(accept_motion_){delta_x+=std::uint32_t(event.motion.xrel);delta_y+=std::uint32_t(event.motion.yrel);}break;
            case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT || event.button.button == SDL_BUTTON_MIDDLE) {
                    InputEvent input{event.type == SDL_MOUSEBUTTONDOWN ? InputKind::pointer_down : InputKind::pointer_up,
                        0,event.button.x,event.button.y};
                    input.button=event.button.button==SDL_BUTTON_LEFT?1:event.button.button==SDL_BUTTON_RIGHT?2:3;
                    result.push_back(std::move(input));
                }
                break;
            case SDL_WINDOWEVENT:
                if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
                    SDL_SetRelativeMouseMode(SDL_FALSE);delta_x=delta_y=0;accept_motion_=false;
                    result.push_back({InputKind::focus_lost});
                } else if(event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) {
                    accept_motion_=true;
                    if(capture_requested_ && SDL_SetRelativeMouseMode(SDL_TRUE)!=0)fail();
                }
                break;
            default: break;
            }
        }
        if(delta_x || delta_y)result.push_back({InputKind::pointer_delta,0,std::int32_t(delta_x),std::int32_t(delta_y)});
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
    void capture_pointer(bool enabled) override {
        if(SDL_SetRelativeMouseMode(enabled?SDL_TRUE:SDL_FALSE)!=0)fail();
        capture_requested_=enabled;
    }

private:
    void close() noexcept {
        if(capture_requested_)SDL_SetRelativeMouseMode(SDL_FALSE);
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
    bool capture_requested_=false;
    bool accept_motion_=true;
};
} // namespace

std::unique_ptr<GameWindow> create_game_window(const std::string& title, int width, int height) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
        throw std::invalid_argument("Invalid game window size");
    return std::make_unique<SdlWindow>(title, width, height);
}
} // namespace dfre
