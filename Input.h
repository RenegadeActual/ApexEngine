#pragma once

#include "Common.h"

namespace apex {

// ---------------------------------------------------------------------------
// Key codes used by the engine. These are platform-independent — Win32's
// virtual key codes are translated to these values in the platform layer.
//
// Add new keys as needed. Keep the Count sentinel at the very end.
// ---------------------------------------------------------------------------
enum class Key : u32 {
    Unknown = 0,

    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Digits (top row, not numpad)
    Num0, Num1, Num2, Num3, Num4,
    Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1, F2, F3, F4, F5, F6,
    F7, F8, F9, F10, F11, F12,

    // Arrows
    Left, Right, Up, Down,

    // Modifiers
    LeftShift,  RightShift,
    LeftCtrl,   RightCtrl,
    LeftAlt,    RightAlt,

    // Common specials
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,

    // Always last. Equals the number of keys above. Used to size arrays.
    Count
};

// ---------------------------------------------------------------------------
// Mouse buttons.
// ---------------------------------------------------------------------------
enum class MouseButton : u32 {
    Left,
    Right,
    Middle,

    Count
};

// ---------------------------------------------------------------------------
// Input system. Singleton — accessed via Input::Get() after Init() has been
// called.
//
// Lifecycle:
//   Input::Init();                              // once at program start
//   ... main loop ...
//       Input::Get().NewFrame();                // once per frame
//       window->PollEvents();                   // feeds events to Input
//       // ... game code queries Input::Get() ...
//   Input::Shutdown();                          // once at program end
// ---------------------------------------------------------------------------
class Input {
public:
    // ---- Lifecycle ----
    static bool   Init();
    static void   Shutdown();
    static Input& Get();

    // Non-copyable, non-movable. Singletons are unique by nature.
    Input(const Input&)            = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&)                 = delete;
    Input& operator=(Input&&)      = delete;

    // ---- Per-frame ----

    // Snapshot current state to "last frame" state. Call once per frame
    // before processing OS events. After this, WasKeyPressed and similar
    // queries reflect "what changed this frame."
    void NewFrame();

    // ---- Event ingestion (called by the platform layer) ----

    void OnKeyDown(Key key);
    void OnKeyUp(Key key);
    void OnMouseButtonDown(MouseButton button);
    void OnMouseButtonUp(MouseButton button);
    void OnMouseMove(i32 x, i32 y);
    void OnMouseWheel(f32 delta);

    // ---- Queries (used by game code) ----

    bool IsKeyDown(Key key) const;
    bool WasKeyPressed(Key key) const;       // not down last frame, down this frame
    bool WasKeyReleased(Key key) const;      // down last frame, not down this frame

    bool IsMouseButtonDown(MouseButton button) const;
    bool WasMouseButtonPressed(MouseButton button) const;
    bool WasMouseButtonReleased(MouseButton button) const;

    i32 GetMouseX() const;
    i32 GetMouseY() const;
    i32 GetMouseDeltaX() const;
    i32 GetMouseDeltaY() const;
    f32 GetMouseWheelDelta() const;

private:
    Input()  = default;
    ~Input() = default;

    static constexpr size_t kKeyCount   = static_cast<size_t>(Key::Count);
    static constexpr size_t kMouseCount = static_cast<size_t>(MouseButton::Count);

    // Key state — current frame and previous frame.
    bool m_keysDown[kKeyCount]          = {};
    bool m_keysDownLastFrame[kKeyCount] = {};

    // Mouse button state — current frame and previous frame.
    bool m_mouseDown[kMouseCount]          = {};
    bool m_mouseDownLastFrame[kMouseCount] = {};

    // Mouse position (absolute, in window client coordinates).
    i32 m_mouseX = 0;
    i32 m_mouseY = 0;

    // Mouse position at start of frame, for delta computation.
    i32 m_mouseXLastFrame = 0;
    i32 m_mouseYLastFrame = 0;

    // Accumulated mouse wheel scroll since last NewFrame.
    f32 m_mouseWheelDelta = 0.0f;

    // The one-and-only instance.
    static Input* s_instance;
};

} // namespace apex