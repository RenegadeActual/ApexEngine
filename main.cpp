#define _WIN32_WINNT 0x0A00 // Target Windows 10 or later

#include <windows.h>
#include <cstdio>
// ------------------------------------------------------------------------------
// The window procedure function that handles messages sent to the window.
// Parameters:
// - hwnd: Handle to the window receiving the message.
// - msg: The message identifier.
// - wParam: Additional message information (varies by message).
// - lParam: Additional message information (varies by message).
// Returns:
// - LRESULT: The result of message processing, depends on the message.
// ------------------------------------------------------------------------------
LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            // user clicks the close button
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            // the window is being destroyed. Post a quit message to end the message loop.
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ------------------------------------------------------------------------------
// The entry point of the application.
// ------------------------------------------------------------------------------
int main() {
    // Get the HINSTANCE of the application, which is needed for window creation and registration.
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    const wchar_t* CLASS_NAME = L"ApexEngineWindowClass";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW; // Redraw on horizontal or vertical resize
    wc.lpfnWndProc = WndProc; // Set the window procedure function
    wc.hInstance = hInstance; // Set the instance handle
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); // Use the default arrow cursor
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Set the background color to the default window color (required but useless since Vulkan will render over it)
    wc.lpszClassName = CLASS_NAME; // Set the window class name

    if (!RegisterClassExW(&wc)) {
        std::printf("Failed to register window class. Error code: %lu\n", GetLastError());
        return 1;
    }

    // Create the window with the specified class name, title, and styles.
    HWND hwnd = CreateWindowExW(
        0, // Optional window styles (0 for no extended styles)
        CLASS_NAME, // Window class name
        L"Apex Engine", // Window title
        WS_OVERLAPPEDWINDOW, // Window style (overlapped window with title bar and borders)
        CW_USEDEFAULT, CW_USEDEFAULT, // Initial position (x, y) - use default values
        1280, 720, // Initial size (width, height)
        nullptr, // Parent window handle (nullptr for top-level window)
        nullptr, // Menu handle (nullptr for no menu)
        hInstance, // Instance handle
        nullptr // Additional application data (nullptr for no additional data)
    );

    if (hwnd == nullptr) {
        std::printf("CreateWindowExW failed. Error code: %lu\n", GetLastError());
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);

    // Main message loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam; // Return the exit code from PostQuitMessage
}