#pragma once

#include "Common.h"

#include <string_view>

namespace apex {

// ------------------------------------------------------------------------------
// Forward declaration of the platform-specific implementation struct.
//
// Uses PIMPL (Pointer to Implementation) idiom to hide platform-specific details from the public interface. The actual definition of Impl will be in the corresponding .cpp file and will contain all the platform-specific members and logic.
// ------------------------------------------------------------------------------
struct WindowImpl;

// ------------------------------------------------------------------------------
// A platform window.
//
// Constructed via the static Create() factory function; check the returned optional for success. The constructor is private since you can't signal failure without exceptions (which we don't want to use).
// ------------------------------------------------------------------------------
class Window {
public:
    // ---- Creation/Destruction ----
    // Factory function to create a Window instance.
    // Caller owns the returned pointer and is responsible for deleting it when done.
    static Window* Create(std::string_view title, u32 width, u32 height);

    ~Window();

    // Non-copyable and non-movable to prevent accidental copying or moving of the window instance, which could lead to resource management issues.
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    // ---- Operations ----
    // Process pending OS events. Call once per frame.
    void PollEvents();

    // Returns true if the window should close (e.g. user clicked the close button/ALT-F4).
    bool ShouldClose() const;

    // ---- Queries ----

    u32 GetWidth() const;
    u32 GetHeight() const;

private:
    // Private constructor: Clients use Create() instead.
    Window();

    // Owned Platform-specific state. Defined in Window.cpp
    WindowImpl* m_impl = nullptr;
};

}