#pragma once

#include "Common.h"

#include <string_view>

/// @file Window.h
/// @brief Platform window abstraction.

namespace apex {

/// Opaque platform-specific implementation type for Window.
///
/// Forward-declared here so that platform headers (`windows.h` on Win32)
/// stay out of the public interface. The actual definition lives in
/// Window.cpp. This is the PIMPL (Pointer to Implementation) idiom.
struct WindowImpl;

/// A platform window.
///
/// Owns an OS-level window handle and the associated event-loop machinery.
/// Instances are created via the static @ref Create factory function — the
/// constructor is private because, without exceptions, a constructor cannot
/// signal failure to its caller. @ref Create returns nullptr on failure.
///
/// Windows are non-copyable and non-movable; they own platform resources
/// whose lifetimes cannot be safely transferred.
class Window {
public:
    // ---- Creation/Destruction ----

    /// Create a new window.
    ///
    /// @param title  Caption shown in the title bar. UTF-8.
    /// @param width  Initial client-area width in pixels.
    /// @param height Initial client-area height in pixels.
    /// @return       Owning pointer to the new Window, or nullptr on failure.
    ///               The caller is responsible for `delete`-ing it.
    static Window* Create(std::string_view title, u32 width, u32 height);

    /// Destroy the window and release its OS handle.
    ~Window();

    /// @cond
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;
    /// @endcond

    // ---- Operations ----

    /// Process all pending OS events for this window. Non-blocking.
    ///
    /// Call once per frame. Drains the message queue and dispatches
    /// keyboard, mouse, focus, resize, and close events.
    void PollEvents();

    /// True if the user has requested the window be closed.
    ///
    /// Set when the user clicks the close button, presses Alt+F4, or the
    /// OS otherwise asks the window to terminate. The application is
    /// expected to poll this and exit its main loop accordingly.
    bool ShouldClose() const;

    // ---- Queries ----

    /// Current client-area width, in pixels.
    u32 GetWidth() const;

    /// Current client-area height, in pixels.
    u32 GetHeight() const;

    /// Opaque platform-native window handle. Caller is expected to know what type this is.
    void* GetNativeHandle() const;

    /// Window repaint callback. Called by the platform layer when the window needs to be redrawn.
    using RedrawCallback = void (*)();

    /// Register a callback to be called when the window needs to be redrawn in the modal loop
    void SetRedrawCallback(RedrawCallback callback);

    /// true while the user is interactively resizing the window.
    bool IsResizing() const;

private:
    /// Private constructor. Clients use @ref Create instead.
    Window();

    WindowImpl* m_impl = nullptr; ///< Owned platform-specific state. Defined in Window.cpp.
};

} // namespace apex
