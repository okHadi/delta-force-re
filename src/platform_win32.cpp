#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "dfre/platform.hpp"

#include <stdexcept>

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

    ~WindowsWindow() override { if (handle_) DestroyWindow(handle_); }

    std::vector<InputEvent> poll() override {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) events_.push_back({InputKind::quit});
            TranslateMessage(&message);
            DispatchMessageW(&message);
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

private:
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
        case WM_KEYDOWN:
            if (!(position & (1L << 30))) self->events_.push_back({InputKind::key_down, static_cast<int>(value)});
            return 0;
        case WM_KEYUP: self->events_.push_back({InputKind::key_up, static_cast<int>(value)}); return 0;
        case WM_MOUSEMOVE: self->events_.push_back({InputKind::pointer_move, 0, x, y}); return 0;
        case WM_LBUTTONDOWN:
            SetCapture(handle);
            self->events_.push_back({InputKind::pointer_down, 0, x, y}); return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            self->events_.push_back({InputKind::pointer_up, 0, x, y}); return 0;
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
};
} // namespace

std::unique_ptr<GameWindow> create_game_window(const std::string& title, int width, int height) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
        throw std::invalid_argument("Invalid game window size");
    return std::make_unique<WindowsWindow>(title, width, height);
}
} // namespace dfre
