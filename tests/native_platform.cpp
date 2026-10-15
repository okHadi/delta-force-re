#include "dfre/platform.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define SDL_MAIN_HANDLED
#include <SDL.h>
#endif

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void inject() {
#ifdef _WIN32
    auto handle = FindWindowW(L"DeltaForceREWindow", L"DFRE platform test");
    check(handle != nullptr, "Missing test window");
    PostMessageW(handle, WM_KEYDOWN, VK_RETURN, 1);
    PostMessageW(handle, WM_KEYDOWN, VK_RETURN, 1L << 30);
    PostMessageW(handle, WM_KEYUP, VK_RETURN, 0);
    PostMessageW(handle, WM_MOUSEMOVE, 0, MAKELPARAM(12, 23));
    PostMessageW(handle, WM_LBUTTONDOWN, 0, MAKELPARAM(12, 23));
    PostMessageW(handle, WM_LBUTTONUP, 0, MAKELPARAM(12, 23));
    PostMessageW(handle, WM_CLOSE, 0, 0);
#else
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_RETURN;
    check(SDL_PushEvent(&event) == 1, "Cannot queue key down");
    event.key.repeat = 1;
    check(SDL_PushEvent(&event) == 1, "Cannot queue repeated key");
    event.type = SDL_KEYUP;
    event.key.repeat = 0;
    check(SDL_PushEvent(&event) == 1, "Cannot queue key up");
    event = {};
    event.type = SDL_MOUSEMOTION;
    event.motion.x = 12; event.motion.y = 23;
    check(SDL_PushEvent(&event) == 1, "Cannot queue pointer move");
    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 12; event.button.y = 23;
    check(SDL_PushEvent(&event) == 1, "Cannot queue pointer down");
    event.type = SDL_MOUSEBUTTONUP;
    check(SDL_PushEvent(&event) == 1, "Cannot queue pointer up");
    event.type = SDL_QUIT;
    check(SDL_PushEvent(&event) == 1, "Cannot queue close");
#endif
}
} // namespace

int main() {
    try {
        bool rejected = false;
        try { dfre::create_game_window("DFRE platform test", 0, 48); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Invalid window size accepted");
        auto window = dfre::create_game_window("DFRE platform test", 64, 48);
        const std::vector<std::uint32_t> frame(64 * 48, 0xff123456);
        window->present(frame);
        rejected = false;
        try { window->present({0xff000000}); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Invalid frame size accepted");
        window->poll();
        inject();
        const auto events = window->poll();
        for (const auto kind : {dfre::InputKind::key_down, dfre::InputKind::key_up,
                               dfre::InputKind::pointer_move, dfre::InputKind::pointer_down,
                               dfre::InputKind::pointer_up, dfre::InputKind::quit}) {
            const auto found = std::find_if(events.begin(), events.end(), [&](const auto& event) {
                if (event.kind != kind) return false;
                if (kind == dfre::InputKind::key_down || kind == dfre::InputKind::key_up)
                    return event.key == 13;
                if (kind == dfre::InputKind::quit) return true;
                return event.x == 12 && event.y == 23;
            });
            check(found != events.end(), "Missing input event or incorrect coordinates");
        }
        check(std::count_if(events.begin(), events.end(), [](const auto& event) {
            return event.kind == dfre::InputKind::key_down && event.key == 13;
        }) == 1, "Repeated key down was not filtered");
        window->yield();
        window->ticks();
        window.reset();
        auto second = dfre::create_game_window("DFRE platform test", 64, 48);
        second->present(frame);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
