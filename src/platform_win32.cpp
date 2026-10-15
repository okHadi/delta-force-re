#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "dfre/platform.hpp"

#include <stdexcept>
#include <utility>

namespace dfre {
namespace {

class WindowsWindow final : public GameWindow {
public:
    WindowsWindow(const std::string& title, int width, int height) : width_(width), height_(height) {
        const auto instance = GetModuleHandleW(nullptr);
        WNDCLASSW type{};
        type.lpfnWndProc = procedure;
        type.hInstance = instance;
        type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        type.lpszClassName = L"DeltaForceREWindow";
        if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Cannot register the game window");
        constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        RECT bounds{0, 0, width, height};
        if (!AdjustWindowRect(&bounds, style, FALSE))
            throw std::runtime_error("Cannot size the game window");
        const std::wstring caption(title.begin(), title.end());
        handle_ = CreateWindowExW(0, type.lpszClassName, caption.c_str(), style,
                                 CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
                                 bounds.bottom - bounds.top, nullptr, nullptr, instance, this);
        if (!handle_) throw std::runtime_error("Cannot open the game window");
        bitmap_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_.bmiHeader.biWidth = width;
        bitmap_.bmiHeader.biHeight = -height;
        bitmap_.bmiHeader.biPlanes = 1;
        bitmap_.bmiHeader.biBitCount = 32;
        bitmap_.bmiHeader.biCompression = BI_RGB;
        ShowWindow(handle_, SW_SHOW);
    }

    ~WindowsWindow() override { release_pointer();if (handle_) DestroyWindow(handle_); }

    std::vector<InputEvent> poll() override {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) events_.push_back({InputKind::quit});
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if(pointer_active_) {
            const int dx=pointer_x_-width_/2,dy=pointer_y_-height_/2;
            if(dx || dy)events_.push_back({InputKind::pointer_delta,0,dx,dy});
            center_pointer();
        }
        std::vector<InputEvent> result;
        result.swap(events_);
        return result;
    }

    void present(const std::vector<std::uint32_t>& argb) override {
        if (argb.size() != static_cast<std::size_t>(width_) * height_)
            throw std::invalid_argument("Frame size differs from the game window");
        frame_ = argb;
        const auto dc = GetDC(handle_);
        if (!dc) throw std::runtime_error("Cannot access the game display");
        const auto result = draw(dc);
        ReleaseDC(handle_, dc);
        if (result == static_cast<int>(GDI_ERROR)) throw std::runtime_error("Cannot draw the game frame");
    }

    std::uint32_t ticks() const override { return GetTickCount(); }
    void yield() const override { Sleep(0); }
    bool active() const override { return active_; }
    void capture_pointer(bool enabled) override {
        capture_requested_=enabled;
        if(!enabled)release_pointer();
        else if(GetFocus()==handle_)activate_pointer();
    }

private:
    void center_pointer() noexcept {
        POINT center{width_/2,height_/2};
        if(ClientToScreen(handle_,&center))SetCursorPos(center.x,center.y);
        pointer_x_=width_/2;pointer_y_=height_/2;
    }

    void activate_pointer() noexcept {
        if(pointer_active_)return;
        POINT first{0,0},last{width_,height_};
        if(!ClientToScreen(handle_,&first) || !ClientToScreen(handle_,&last))return;
        RECT bounds{first.x,first.y,last.x,last.y};
        if(!ClipCursor(&bounds))return;
        pointer_active_=true;
        SetCapture(handle_);SetCursor(nullptr);center_pointer();
    }

    void release_pointer() noexcept {
        if(!pointer_active_)return;
        pointer_active_=false;
        ClipCursor(nullptr);
        if(GetCapture()==handle_)ReleaseCapture();
        SetCursor(LoadCursorW(nullptr,MAKEINTRESOURCEW(32512)));
    }

    void pointer_button(InputKind kind,unsigned button,int x,int y) {
        const auto mask=1u<<(button-1);
        if(kind==InputKind::pointer_down) {mouse_buttons_|=mask;SetCapture(handle_);}
        else {mouse_buttons_&=~mask;if(!mouse_buttons_ && !pointer_active_ && GetCapture()==handle_)ReleaseCapture();}
        InputEvent event{kind,0,x,y};event.button=button;events_.push_back(std::move(event));
    }

    int draw(HDC dc) const {
        if (frame_.empty()) return 0;
        return StretchDIBits(dc, 0, 0, width_, height_, 0, 0, width_, height_,
                             frame_.data(), &bitmap_, DIB_RGB_COLORS, SRCCOPY);
    }

    static LRESULT CALLBACK procedure(HWND handle, UINT message, WPARAM value, LPARAM position) {
        auto* self = reinterpret_cast<WindowsWindow*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<WindowsWindow*>(reinterpret_cast<CREATESTRUCTW*>(position)->lpCreateParams);
            SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(handle, message, value, position);
        const int x = static_cast<short>(LOWORD(position));
        const int y = static_cast<short>(HIWORD(position));
        switch (message) {
        case WM_CLOSE: self->events_.push_back({InputKind::quit}); return 0;
        case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
            if (message==WM_KEYUP || message==WM_SYSKEYUP || !(position & (1L << 30))) {
                InputEvent event{message==WM_KEYDOWN || message==WM_SYSKEYDOWN?InputKind::key_down:InputKind::key_up,static_cast<int>(value)};
                event.extended=(position & (1L<<24))!=0;
                self->events_.push_back(std::move(event));
            }
            if(message==WM_SYSKEYDOWN || message==WM_SYSKEYUP)return DefWindowProcW(handle,message,value,position);
            return 0;
        case WM_SETFOCUS: self->active_=true;if(self->capture_requested_)self->activate_pointer();return 0;
        case WM_KILLFOCUS:
            self->active_=false;
            self->release_pointer();if(GetCapture()==handle)ReleaseCapture();self->mouse_buttons_=0;
            self->events_.push_back({InputKind::focus_lost});return 0;
        case WM_CAPTURECHANGED:
            if(self->pointer_active_ && reinterpret_cast<HWND>(position)!=handle) {
                self->release_pointer();self->mouse_buttons_=0;
                self->events_.push_back({InputKind::focus_lost});
            }
            return 0;
        case WM_SETCURSOR:
            if(self->pointer_active_){SetCursor(nullptr);return TRUE;}
            return DefWindowProcW(handle,message,value,position);
        case WM_CHAR: {
            const auto unit = static_cast<wchar_t>(value);
            if (unit >= 0xd800 && unit <= 0xdbff) { self->surrogate_ = unit; return 0; }
            wchar_t units[2]{unit, 0};
            int count = 1;
            if (unit >= 0xdc00 && unit <= 0xdfff) {
                if (!self->surrogate_) return 0;
                units[0] = self->surrogate_;
                units[1] = unit;
                count = 2;
            }
            self->surrogate_ = 0;
            if (unit < 32 || unit == 127) return 0;
            char text[8];
            const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, units, count,
                                                    text, sizeof(text), nullptr, nullptr);
            if (length) {
                InputEvent event{InputKind::text_input};
                event.text.assign(text, static_cast<std::size_t>(length));
                self->events_.push_back(std::move(event));
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            self->pointer_x_=x;self->pointer_y_=y;
            self->events_.push_back({InputKind::pointer_move, 0, x, y});return 0;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            self->pointer_button(InputKind::pointer_down,message==WM_LBUTTONDOWN?1:message==WM_RBUTTONDOWN?2:3,x,y);return 0;
        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
            self->pointer_button(InputKind::pointer_up,message==WM_LBUTTONUP?1:message==WM_RBUTTONUP?2:3,x,y);return 0;
        case WM_PRINTCLIENT: self->draw(reinterpret_cast<HDC>(value)); return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            self->draw(BeginPaint(handle, &paint));
            EndPaint(handle, &paint);
            return 0;
        }
        default: return DefWindowProcW(handle, message, value, position);
        }
    }

    HWND handle_ = nullptr;
    int width_, height_;
    BITMAPINFO bitmap_{};
    std::vector<std::uint32_t> frame_;
    std::vector<InputEvent> events_;
    wchar_t surrogate_ = 0;
    bool active_=true;
    bool capture_requested_=false,pointer_active_=false;
    int pointer_x_=0,pointer_y_=0;
    unsigned mouse_buttons_=0;
};
} // namespace

std::unique_ptr<GameWindow> create_game_window(const std::string& title, int width, int height) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
        throw std::invalid_argument("Invalid game window size");
    return std::make_unique<WindowsWindow>(title, width, height);
}
} // namespace dfre
