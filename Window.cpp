#include "Window.h"

#define _WIN32_WINNT 0x0A00 // Target Windows 10 or later
#include <windows.h>

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
// The window procedure function that handles messages sent to the window.
// ------------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // first message a window receives is WM_NCCREATE, which is sent before CreateWindowExW returns. We can use this to associate the WindowImpl with the HWND.
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // For all other messages, we can retrieve the WindowImpl from the HWND.
    WindowImpl* impl = reinterpret_cast<WindowImpl*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    
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
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ------------------------------------------------------------------------------
// Registers the window class if it hasn't been registered yet. This is required before creating any windows of that class.
// ------------------------------------------------------------------------------
static bool RegisterWindowClass() {
    if (g_classRegistered) {
        return true;
    }

    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW; // Redraw on horizontal
    wc.lpfnWndProc = WndProc; // Set the window procedure function
    wc.hInstance = hInstance; // Set the instance handle
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); // Use the default arrow cursor
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); // Set the background color to the default window color (required but useless since Vulkan will render over it)
    wc.lpszClassName = kWindowClassName; // Set the window class name

    if (RegisterClassExW(&wc) == 0) {
        std::printf("RegisterClassExW failed. Error code: %lu\n", GetLastError());
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
    Window* window = new(std::nothrow) Window();
    if (window == nullptr) {
        return nullptr;
    }

    // Step 3: allocate the platform-specific state.
    window->m_impl = new(std::nothrow) WindowImpl();
    if (window->m_impl == nullptr) {
        delete window;
        return nullptr;
    }

    window->m_impl->width  = width;
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
    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        titleBuffer,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        static_cast<int>(width),
        static_cast<int>(height),
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        window->m_impl
    );

    if (hwnd == nullptr) {
        std::printf("CreateWindowExW failed. Error: %lu\n", GetLastError());
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
    MSG msg = {};
    while (PeekMessageW(&msg, m_impl->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

bool Window::ShouldClose() const {
    return m_impl->shouldClose;
}

u32 Window::GetWidth() const {
    return m_impl->width;
}

u32 Window::GetHeight() const {
    return m_impl->height;
}

} //namespace apex