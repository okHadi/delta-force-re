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
#ifdef _WIN32
void check_display() {
    const auto handle = FindWindowW(L"DeltaForceREWindow", L"DFRE platform test");
    BITMAPINFO bitmap{};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = 64;
    bitmap.bmiHeader.biHeight = -48;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    void* raw = nullptr;
    const auto dc = CreateCompatibleDC(nullptr);
    const auto image = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &raw, nullptr, 0);
    check(dc && image && raw, "Cannot create a display check buffer");
    const auto previous = SelectObject(dc, image);
    const bool printed = PrintWindow(handle, dc, PW_CLIENTONLY) != 0;
    const auto* pixels = static_cast<const std::uint32_t*>(raw);
    const bool correct = std::all_of(pixels, pixels + 64 * 48,
                                     [](auto pixel) { return (pixel & 0xffffffu) == 0x123456; });
    SelectObject(dc, previous);
    DeleteObject(image);
    DeleteDC(dc);
    check(printed && correct, "Displayed pixels differ from the source frame");
}
#endif
void inject() {
#ifdef _WIN32
    auto handle = FindWindowW(L"DeltaForceREWindow", L"DFRE platform test");
    check(handle != nullptr, "Missing test window");
    PostMessageW(handle, WM_KEYDOWN, VK_RETURN, 1);
    PostMessageW(handle, WM_KEYDOWN, VK_RETURN, 1L << 30);
    PostMessageW(handle, WM_KEYUP, VK_RETURN, 0);
    PostMessageW(handle, WM_KEYDOWN, VK_OEM_PLUS, 1);
    PostMessageW(handle, WM_KEYUP, VK_OEM_PLUS, 0);
    PostMessageW(handle, WM_KEYDOWN, VK_NUMPAD8, 1);
    PostMessageW(handle, WM_KEYUP, VK_NUMPAD8, 0);
    PostMessageW(handle, WM_KEYDOWN, VK_UP, 1L << 24);
    PostMessageW(handle, WM_KEYUP, VK_UP, 1L << 24);
    PostMessageW(handle, WM_CHAR, '!', 0);
    PostMessageW(handle, WM_MOUSEMOVE, 0, MAKELPARAM(12, 23));
    PostMessageW(handle, WM_LBUTTONDOWN, 0, MAKELPARAM(12, 23));
    PostMessageW(handle, WM_LBUTTONUP, 0, MAKELPARAM(12, 23));
    PostMessageW(handle, WM_RBUTTONDOWN, 0, MAKELPARAM(12, 23));
    PostMessageW(handle, WM_RBUTTONUP, 0, MAKELPARAM(12, 23));
    PostMessageW(handle, WM_MBUTTONDOWN, 0, MAKELPARAM(12, 23));
    PostMessageW(handle, WM_MBUTTONUP, 0, MAKELPARAM(12, 23));
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
    for(const auto key:{SDLK_EQUALS,SDLK_KP_8,SDLK_UP}) {
        event={};event.type=SDL_KEYDOWN;event.key.keysym.sym=key;event.key.keysym.mod=KMOD_NUM;
        check(SDL_PushEvent(&event)==1,"Cannot queue game key down");
        event.type=SDL_KEYUP;check(SDL_PushEvent(&event)==1,"Cannot queue game key up");
    }
    event = {};
    event.type = SDL_MOUSEMOTION;
    event.motion.x = 12; event.motion.y = 23;
    event.motion.xrel=5;event.motion.yrel=-7;
    check(SDL_PushEvent(&event) == 1, "Cannot queue pointer move");
    event.motion.xrel=-2;event.motion.yrel=3;
    check(SDL_PushEvent(&event)==1,"Cannot queue second pointer move");
    event = {};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 12; event.button.y = 23;
    check(SDL_PushEvent(&event) == 1, "Cannot queue pointer down");
    event.type = SDL_MOUSEBUTTONUP;
    check(SDL_PushEvent(&event) == 1, "Cannot queue pointer up");
    for(const auto button:{SDL_BUTTON_RIGHT,SDL_BUTTON_MIDDLE}) {
        event.type=SDL_MOUSEBUTTONDOWN;event.button.button=button;
        check(SDL_PushEvent(&event)==1,"Cannot queue game button down");
        event.type=SDL_MOUSEBUTTONUP;check(SDL_PushEvent(&event)==1,"Cannot queue game button up");
    }
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
        const std::vector<std::uint32_t> frame(64 * 48, 0x12123456);
        window->present(frame);
#ifdef _WIN32
        check_display();
#endif
        rejected = false;
        try { window->present({0xff000000}); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "Invalid frame size accepted");
        window->poll();
        inject();
        const auto events = window->poll();
        for (const auto kind : {dfre::InputKind::key_down, dfre::InputKind::key_up,
#ifdef _WIN32
                               dfre::InputKind::text_input,
#endif
                               dfre::InputKind::pointer_move, dfre::InputKind::pointer_down,
                               dfre::InputKind::pointer_up, dfre::InputKind::quit}) {
            const auto found = std::find_if(events.begin(), events.end(), [&](const auto& event) {
                if (event.kind != kind) return false;
                if (kind == dfre::InputKind::key_down || kind == dfre::InputKind::key_up)
                    return event.key == 13;
                if (kind == dfre::InputKind::quit) return true;
                if (kind == dfre::InputKind::text_input) return event.text == "!";
                return event.x == 12 && event.y == 23;
            });
            check(found != events.end(), "Missing input event or incorrect coordinates");
        }
        check(std::count_if(events.begin(), events.end(), [](const auto& event) {
            return event.kind == dfre::InputKind::key_down && event.key == 13;
        }) == 1, "Repeated key down was not filtered");
        for(const auto key:{187,104,38})for(const auto kind:{dfre::InputKind::key_down,dfre::InputKind::key_up})
            check(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==kind && event.key==key;}),"Game key code differs");
        check(std::any_of(events.begin(),events.end(),[](const auto& event){return event.kind==dfre::InputKind::key_down && event.key==38 && event.extended;}),"Extended arrow key flag differs");
        for(unsigned button:{1,2,3})for(const auto kind:{dfre::InputKind::pointer_down,dfre::InputKind::pointer_up})
            check(std::any_of(events.begin(),events.end(),[&](const auto& event){return event.kind==kind && event.button==button;}),"Game pointer button differs");
#ifndef _WIN32
        check(std::any_of(events.begin(),events.end(),[](const auto& event){return event.kind==dfre::InputKind::pointer_delta && event.x==3 && event.y==-4;}),"Relative pointer total differs");
        SDL_Event focus{};focus.type=SDL_WINDOWEVENT;focus.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
        check(SDL_PushEvent(&focus)==1,"Cannot queue focus loss");
#else
        PostMessageW(FindWindowW(L"DeltaForceREWindow",L"DFRE platform test"),WM_KILLFOCUS,0,0);
#endif
        const auto lost=window->poll();
        check(std::any_of(lost.begin(),lost.end(),[](const auto& event){return event.kind==dfre::InputKind::focus_lost;}),"Missing focus loss event");
        check(!window->active(),"Focus loss must clear active input");
#ifndef _WIN32
        focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;
        check(SDL_PushEvent(&focus)==1,"Cannot queue focus gain");
#else
        PostMessageW(FindWindowW(L"DeltaForceREWindow",L"DFRE platform test"),WM_SETFOCUS,0,0);
#endif
        window->poll();
        check(window->active(),"Focus gain must restore active input");
        window->capture_pointer(false);
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
