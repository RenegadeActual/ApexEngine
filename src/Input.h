#pragma once

#include "Common.h"

/// @file Input.h
/// @brief Polled keyboard and mouse input system.

namespace apex {

/// Platform-independent key codes.
///
/// The platform layer translates OS-specific virtual key codes (Win32's
/// `VK_*` values, etc.) into these. Add new keys as needed; keep
/// @ref Count as the last value so it correctly sizes state arrays.
enum class Key : u32 {
    Unknown = 0, ///< No key / unrecognized key code.

    // Letters
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,

    // Digits (top row, not numpad)
    Num0,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,

    // Function keys
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,

    // Arrows
    Left,
    Right,
    Up,
    Down,

    // Modifiers
    LeftShift,
    RightShift,
    LeftCtrl,
    RightCtrl,
    LeftAlt,
    RightAlt,

    // Common specials
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,

    Count ///< Sentinel; equals the number of real keys above. Used to size state arrays.
};

/// Mouse buttons supported by the input system.
enum class MouseButton : u32 {
    Left,   ///< Primary (left) mouse button.
    Right,  ///< Secondary (right) mouse button.
    Middle, ///< Middle / wheel-click button.

    Count ///< Sentinel; equals the number of buttons above.
};

/// Polled keyboard and mouse input system. Singleton.
///
/// Game code asks "is key X down?" or "was key X pressed this frame?"
/// rather than subscribing to event callbacks. This fits the per-frame
/// game loop and keeps input-handling logic in one place.
///
/// **Lifecycle:**
/// @code
/// apex::Input::Init();                       // once at program start
/// while (running) {
///     apex::Input::Get().NewFrame();         // once per frame
///     window->PollEvents();                  // feeds events to Input
///     // ... game code queries Input::Get() ...
/// }
/// apex::Input::Shutdown();                   // once at program end
/// @endcode
///
/// The platform layer (Window.cpp) calls the `On*` event-ingestion
/// methods in response to OS messages. Game code uses the `Is*` / `Was*`
/// query methods.
class Input {
public:
    // ---- Lifecycle ----

    /// Initialize the input singleton. Call once at program start.
    /// @return true on success; false if allocation failed.
    static bool Init();

    /// Tear down the input singleton. Call once at program end.
    static void Shutdown();

    /// Access the singleton instance.
    /// @pre @ref Init has been called and has not been followed by @ref Shutdown.
    ///      Violating the precondition triggers an assertion in debug builds.
    static Input& Get();

    /// @cond
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(Input&&) = delete;
    /// @endcond

    // ---- Per-frame ----

    /// Snapshot the current state into "last frame" state.
    ///
    /// Call once per frame, before @ref Window::PollEvents. After this,
    /// @ref WasKeyPressed and similar queries reflect "what changed this
    /// frame." Also clears the per-frame mouse-wheel accumulator.
    void NewFrame();

    // ---- Event ingestion (called by the platform layer) ----

    /// Mark @p key as currently pressed. Called by the platform layer.
    void OnKeyDown(Key key);

    /// Mark @p key as currently released. Called by the platform layer.
    void OnKeyUp(Key key);

    /// Mark @p button as currently pressed. Called by the platform layer.
    void OnMouseButtonDown(MouseButton button);

    /// Mark @p button as currently released. Called by the platform layer.
    void OnMouseButtonUp(MouseButton button);

    /// Record the current cursor position in window client coordinates.
    void OnMouseMove(i32 x, i32 y);

    /// Accumulate a mouse-wheel delta for the current frame.
    /// @param delta Notches scrolled this event. Positive = scrolled up.
    void OnMouseWheel(f32 delta);

    // ---- Queries (used by game code) ----

    /// True if @p key is currently held down.
    bool IsKeyDown(Key key) const;

    /// True if @p key transitioned from up to down this frame.
    bool WasKeyPressed(Key key) const;

    /// True if @p key transitioned from down to up this frame.
    bool WasKeyReleased(Key key) const;

    /// True if @p button is currently held down.
    bool IsMouseButtonDown(MouseButton button) const;

    /// True if @p button transitioned from up to down this frame.
    bool WasMouseButtonPressed(MouseButton button) const;

    /// True if @p button transitioned from down to up this frame.
    bool WasMouseButtonReleased(MouseButton button) const;

    /// Current cursor X position in window client coordinates.
    i32 GetMouseX() const;

    /// Current cursor Y position in window client coordinates.
    i32 GetMouseY() const;

    /// Change in cursor X position since the start of this frame.
    i32 GetMouseDeltaX() const;

    /// Change in cursor Y position since the start of this frame.
    i32 GetMouseDeltaY() const;

    /// Mouse-wheel delta accumulated this frame. Positive = scrolled up.
    /// One physical notch is typically ±1.0.
    f32 GetMouseWheelDelta() const;

private:
    Input() = default;
    ~Input() = default;

    static constexpr size_t kKeyCount = static_cast<size_t>(Key::Count);
    static constexpr size_t kMouseCount = static_cast<size_t>(MouseButton::Count);

    bool m_keysDown[kKeyCount] = {};          ///< Current per-key down state.
    bool m_keysDownLastFrame[kKeyCount] = {}; ///< Snapshot at start of frame.

    bool m_mouseDown[kMouseCount] = {};          ///< Current per-button down state.
    bool m_mouseDownLastFrame[kMouseCount] = {}; ///< Snapshot at start of frame.

    i32 m_mouseX = 0; ///< Current cursor X (window client coords).
    i32 m_mouseY = 0; ///< Current cursor Y (window client coords).

    i32 m_mouseXLastFrame = 0; ///< Cursor X at start of frame.
    i32 m_mouseYLastFrame = 0; ///< Cursor Y at start of frame.

    f32 m_mouseWheelDelta = 0.0f; ///< Wheel notches accumulated this frame.

    static Input* s_instance; ///< The one-and-only instance.
};

} // namespace apex
