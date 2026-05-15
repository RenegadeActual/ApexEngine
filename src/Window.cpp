#include "Window.h"

#include "Assert.h"
#include "Input.h"
#include "Log.h"

#define _WIN32_WINNT 0x0A00 // Target Windows 10 or later
#include <windows.h>
#include <windowsx.h> // for GET_X_LPARAM and GET_Y_LPARAM macros

#include <cstdio>
#include <new>

namespace apex {

// ------------------------------------------------------------------------------
// The platform-specific implementation of the Window class for Windows OS.
// ------------------------------------------------------------------------------
struct WindowImpl {
    HWND hwnd = nullptr;
    u32 width = 0;
    u32 height = 0;
    bool shouldClose = false;
};

// ------------------------------------------------------------------------------
// File-local state
// ------------------------------------------------------------------------------
static const wchar_t* kWindowClassName = L"ApexEngineWindowClass";
static bool g_classRegistered = false;

// ------------------------------------------------------------------------------
// Translate Win32 virtual key codes to our platform-independent Key enum.
// This is used in the WndProc when we receive keyboard messages.
// ------------------------------------------------------------------------------
static Key TranslateVirtualKey(WPARAM vk) {
    switch (vk) {
    // Letters:
    case 'A':
        return Key::A;
    case 'B':
        return Key::B;
    case 'C':
        return Key::C;
    case 'D':
        return Key::D;
    case 'E':
        return Key::E;
    case 'F':
        return Key::F;
    case 'G':
        return Key::G;
    case 'H':
        return Key::H;
    case 'I':
        return Key::I;
    case 'J':
        return Key::J;
    case 'K':
        return Key::K;
    case 'L':
        return Key::L;
    case 'M':
        return Key::M;
    case 'N':
        return Key::N;
    case 'O':
        return Key::O;
    case 'P':
        return Key::P;
    case 'Q':
        return Key::Q;
    case 'R':
        return Key::R;
    case 'S':
        return Key::S;
    case 'T':
        return Key::T;
    case 'U':
        return Key::U;
    case 'V':
        return Key::V;
    case 'W':
        return Key::W;
    case 'X':
        return Key::X;
    case 'Y':
        return Key::Y;
    case 'Z':
        return Key::Z;

    // Digits (top row):
    case '0':
        return Key::Num0;
    case '1':
        return Key::Num1;
    case '2':
        return Key::Num2;
    case '3':
        return Key::Num3;
    case '4':
        return Key::Num4;
    case '5':
        return Key::Num5;
    case '6':
        return Key::Num6;
    case '7':
        return Key::Num7;
    case '8':
        return Key::Num8;
    case '9':
        return Key::Num9;

    // Function keys:
    case VK_F1:
        return Key::F1;
    case VK_F2:
        return Key::F2;
    case VK_F3:
        return Key::F3;
    case VK_F4:
        return Key::F4;
    case VK_F5:
        return Key::F5;
    case VK_F6:
        return Key::F6;
    case VK_F7:
        return Key::F7;
    case VK_F8:
        return Key::F8;
    case VK_F9:
        return Key::F9;
    case VK_F10:
        return Key::F10;
    case VK_F11:
        return Key::F11;
    case VK_F12:
        return Key::F12;

    // Arrows:
    case VK_LEFT:
        return Key::Left;
    case VK_RIGHT:
        return Key::Right;
    case VK_UP:
        return Key::Up;
    case VK_DOWN:
        return Key::Down;

    // Modifiers:
    case VK_LSHIFT:
        return Key::LeftShift;
    case VK_RSHIFT:
        return Key::RightShift;
    case VK_LCONTROL:
        return Key::LeftCtrl;
    case VK_RCONTROL:
        return Key::RightCtrl;
    case VK_LMENU:
        return Key::LeftAlt;
    case VK_RMENU:
        return Key::RightAlt;

    // Specials:
    case VK_SPACE:
        return Key::Space;
    case VK_RETURN:
        return Key::Enter;
    case VK_ESCAPE:
        return Key::Escape;
    case VK_TAB:
        return Key::Tab;
    case VK_BACK:
        return Key::Backspace;

    default:
        return Key::Unknown; // Invalid key
    }
}

// ------------------------------------------------------------------------------
// Clear all input states. Called when window loses focus to prevent "stuck keys" when the user
// alt-tabs away with a key pressed.
// ------------------------------------------------------------------------------
static void ClearInputState() {
    for (u32 i = 0; i < static_cast<u32>(Key::Count); ++i) {
        Input::Get().OnKeyUp(static_cast<Key>(i));
    }
    for (u32 i = 0; i < static_cast<u32>(MouseButton::Count); ++i) {
        Input::Get().OnMouseButtonUp(static_cast<MouseButton>(i));
    }
}

// ------------------------------------------------------------------------------
// The window procedure function that handles messages sent to the window.
// ------------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // first message a window receives is WM_NCCREATE, which is sent before CreateWindowExW returns.
    // We can use this to associate the WindowImpl with the HWND.
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // For all other messages, we can retrieve the WindowImpl from the HWND.
    WindowImpl* impl = reinterpret_cast<WindowImpl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (impl == nullptr) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_CLOSE:
        impl->shouldClose = true;
        return 0;
    case WM_SIZE:
        impl->width = LOWORD(lParam);
        impl->height = HIWORD(lParam);
        return 0;
    case WM_DESTROY:
        return 0;

    // Keyboard input:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        const bool wasDownBefore =
            (lParam & (1 << 30)) != 0; // check the "previous key state" bit to prevent auto-repeats
                                       // from generating multiple "pressed" events
        if (!wasDownBefore) {
            Key key = TranslateVirtualKey(wParam);
            if (key != Key::Unknown) {
                Input::Get().OnKeyDown(key);
            }
        }
        return 0;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        Key key = TranslateVirtualKey(wParam);
        if (key != Key::Unknown) {
            Input::Get().OnKeyUp(key);
        }
        return 0;
    }

    // ----- Mouse Input -----
    case WM_LBUTTONDOWN:
        SetCapture(hwnd); // capture the mouse to continue receiving events even if the cursor
                          // leaves the window
        Input::Get().OnMouseButtonDown(MouseButton::Left);
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        Input::Get().OnMouseButtonUp(MouseButton::Left);
        return 0;
    case WM_RBUTTONDOWN:
        SetCapture(hwnd);
        Input::Get().OnMouseButtonDown(MouseButton::Right);
        return 0;
    case WM_RBUTTONUP:
        ReleaseCapture();
        Input::Get().OnMouseButtonUp(MouseButton::Right);
        return 0;
    case WM_MBUTTONDOWN:
        SetCapture(hwnd);
        Input::Get().OnMouseButtonDown(MouseButton::Middle);
        return 0;
    case WM_MBUTTONUP:
        ReleaseCapture();
        Input::Get().OnMouseButtonUp(MouseButton::Middle);
        return 0;

    // ----- Mouse movement -----
    case WM_MOUSEMOVE: {
        // GET_X_LPARAM and GET_Y_LAPARAM handles the sign-extension for negative coordinates, which
        // can happen when the cursor leaves the window in the top or left direction.
        const i32 x = GET_X_LPARAM(lParam);
        const i32 y = GET_Y_LPARAM(lParam);
        Input::Get().OnMouseMove(x, y);
        return 0;
    }

    // ----- Mouse wheel -----
    case WM_MOUSEWHEEL: {
        // The wheel delta is in the high word of wParam, signed
        // standard wheel notch is 120 units, so we covert to "notches"
        const i16 raw = static_cast<i16>(HIWORD(wParam));
        const f32 delta = static_cast<f32>(raw) / 120.0f;
        Input::Get().OnMouseWheel(delta);
        return 0;
    }

    // ----- Focus Loss -----
    case WM_KILLFOCUS:
        // Window lost focus, clear all input states
        ClearInputState();
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ------------------------------------------------------------------------------
// Registers the window class if it hasn't been registered yet. This is required before creating any
// windows of that class.
// ------------------------------------------------------------------------------
static bool RegisterWindowClass() {
    if (g_classRegistered) {
        return true;
    }

    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;           // Redraw on horizontal
    wc.lpfnWndProc = WndProc;                     // Set the window procedure function
    wc.hInstance = hInstance;                     // Set the instance handle
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); // Use the default arrow cursor
    wc.hbrBackground = reinterpret_cast<HBRUSH>(
        COLOR_WINDOW + 1); // Set the background color to the default window color (required but
                           // useless since Vulkan will render over it)
    wc.lpszClassName = kWindowClassName; // Set the window class name

    if (RegisterClassExW(&wc) == 0) {
        LOG_ERROR("Window", "RegisterClassExW failed. Error code: %lu\n", GetLastError());
        return false;
    }

    g_classRegistered = true;
    return true;
}

// ------------------------------------------------------------------------------
// Window member function definitions
// ------------------------------------------------------------------------------

Window::Window() = default;

Window::~Window() {
    if (m_impl != nullptr) {
        if (m_impl->hwnd != nullptr) {
            DestroyWindow(m_impl->hwnd);
        }
        delete m_impl;
        m_impl = nullptr;
    }
}

// factory function to create a Window instance. Returns nullptr on failure.
Window* Window::Create(std::string_view title, u32 width, u32 height) {
    // Step 1: make sure the window class is registered.
    if (!RegisterWindowClass()) {
        return nullptr;
    }

    // Step 2: allocate the Window object on the heap.
    // 'new(std::nothrow)' returns nullptr on allocation failure instead of
    // throwing (which we've disabled anyway).
    Window* window = new (std::nothrow) Window();
    if (window == nullptr) {
        return nullptr;
    }

    // Step 3: allocate the platform-specific state.
    window->m_impl = new (std::nothrow) WindowImpl();
    if (window->m_impl == nullptr) {
        delete window;
        return nullptr;
    }

    window->m_impl->width = width;
    window->m_impl->height = height;

    // Step 4: convert the title to wide characters for Win32.
    // For now we just truncate to ASCII (good enough for our use).
    wchar_t titleBuffer[256] = {};
    {
        const size_t copyLen = (title.size() < 255) ? title.size() : 255;
        for (size_t i = 0; i < copyLen; ++i) {
            titleBuffer[i] = static_cast<wchar_t>(title[i]);
        }
        titleBuffer[copyLen] = L'\0';
    }

    // Step 5: ask Windows to create the actual OS window.
    // We pass window->m_impl as the last parameter; this arrives at our
    // WndProc as part of the WM_NCCREATE message, where we stash it for
    // future lookups.
    HWND hwnd = CreateWindowExW(0,
                                kWindowClassName,
                                titleBuffer,
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                static_cast<int>(width),
                                static_cast<int>(height),
                                nullptr,
                                nullptr,
                                GetModuleHandleW(nullptr),
                                window->m_impl);

    if (hwnd == nullptr) {
        LOG_ERROR("Window", "CreateWindowExW failed. Error: %lu\n", GetLastError());
        delete window;
        return nullptr;
    }

    window->m_impl->hwnd = hwnd;

    // Step 6: show the window (CreateWindowExW makes it but it starts hidden).
    ShowWindow(hwnd, SW_SHOW);

    return window;
}

// process any messages that have arrived since the last call. Non-blocking.
void Window::PollEvents() {
    APEX_ASSERT(m_impl != nullptr, "Window has no platform impl");
    MSG msg = {};
    while (PeekMessageW(&msg, m_impl->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

bool Window::ShouldClose() const {
    APEX_ASSERT(m_impl != nullptr, "Window has no platform impl");
    return m_impl->shouldClose;
}

u32 Window::GetWidth() const {
    APEX_ASSERT(m_impl != nullptr, "Window has no platform impl");
    return m_impl->width;
}

u32 Window::GetHeight() const {
    APEX_ASSERT(m_impl != nullptr, "Window has no platform impl");
    return m_impl->height;
}

} // namespace apex