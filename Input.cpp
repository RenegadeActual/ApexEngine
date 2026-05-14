#include "Input.h"
#include <new>

namespace apex {

// ---------------------------------------------------------------------------
// Out-of-class definition for the static singleton pointer.
// Without this, the linker would complain that s_instance is declared but
// never defined.
// ---------------------------------------------------------------------------
Input* Input::s_instance = nullptr;

// ---------------------------------------------------------------------------
// Lifecycle.
// ---------------------------------------------------------------------------

bool Input::Init() {
    if (s_instance != nullptr) {
        // Already initialized. Could be an error or just a duplicate call;
        // we treat it as benign and return success.
        return true;
    }

    s_instance = new(std::nothrow) Input();
    if (s_instance == nullptr) {
        return false;
    }

    return true;
}

void Input::Shutdown() {
    delete s_instance;
    s_instance = nullptr;
}

Input& Input::Get() {
    // Note: dereferencing s_instance if it's null is undefined behavior.
    // We rely on the caller having called Init() first. In a more defensive
    // engine we might assert here.
    return *s_instance;
}

// ---------------------------------------------------------------------------
// Per-frame snapshot.
// ---------------------------------------------------------------------------

void Input::NewFrame() {
    // Copy current state into "last frame" state. After this point, queries
    // like WasKeyPressed compare current vs. snapshotted state.
    for (size_t i = 0; i < kKeyCount; ++i) {
        m_keysDownLastFrame[i] = m_keysDown[i];
    }
    for (size_t i = 0; i < kMouseCount; ++i) {
        m_mouseDownLastFrame[i] = m_mouseDown[i];
    }

    m_mouseXLastFrame = m_mouseX;
    m_mouseYLastFrame = m_mouseY;

    // Mouse wheel deltas don't carry across frames — they're cleared each
    // frame and only the events arriving during the frame contribute.
    m_mouseWheelDelta = 0.0f;
}

// ---------------------------------------------------------------------------
// Event ingestion. The platform layer (Window.cpp) calls these in response
// to OS messages.
// ---------------------------------------------------------------------------

void Input::OnKeyDown(Key key) {
    const size_t i = static_cast<size_t>(key);
    if (i < kKeyCount) {
        m_keysDown[i] = true;
    }
}

void Input::OnKeyUp(Key key) {
    const size_t i = static_cast<size_t>(key);
    if (i < kKeyCount) {
        m_keysDown[i] = false;
    }
}

void Input::OnMouseButtonDown(MouseButton button) {
    const size_t i = static_cast<size_t>(button);
    if (i < kMouseCount) {
        m_mouseDown[i] = true;
    }
}

void Input::OnMouseButtonUp(MouseButton button) {
    const size_t i = static_cast<size_t>(button);
    if (i < kMouseCount) {
        m_mouseDown[i] = false;
    }
}

void Input::OnMouseMove(i32 x, i32 y) {
    m_mouseX = x;
    m_mouseY = y;
}

void Input::OnMouseWheel(f32 delta) {
    m_mouseWheelDelta += delta;
}

// ---------------------------------------------------------------------------
// Queries.
// ---------------------------------------------------------------------------

bool Input::IsKeyDown(Key key) const {
    const size_t i = static_cast<size_t>(key);
    return (i < kKeyCount) && m_keysDown[i];
}

bool Input::WasKeyPressed(Key key) const {
    const size_t i = static_cast<size_t>(key);
    return (i < kKeyCount) && m_keysDown[i] && !m_keysDownLastFrame[i];
}

bool Input::WasKeyReleased(Key key) const {
    const size_t i = static_cast<size_t>(key);
    return (i < kKeyCount) && !m_keysDown[i] && m_keysDownLastFrame[i];
}

bool Input::IsMouseButtonDown(MouseButton button) const {
    const size_t i = static_cast<size_t>(button);
    return (i < kMouseCount) && m_mouseDown[i];
}

bool Input::WasMouseButtonPressed(MouseButton button) const {
    const size_t i = static_cast<size_t>(button);
    return (i < kMouseCount) && m_mouseDown[i] && !m_mouseDownLastFrame[i];
}

bool Input::WasMouseButtonReleased(MouseButton button) const {
    const size_t i = static_cast<size_t>(button);
    return (i < kMouseCount) && !m_mouseDown[i] && m_mouseDownLastFrame[i];
}

i32 Input::GetMouseX() const { return m_mouseX; }
i32 Input::GetMouseY() const { return m_mouseY; }

i32 Input::GetMouseDeltaX() const { return m_mouseX - m_mouseXLastFrame; }
i32 Input::GetMouseDeltaY() const { return m_mouseY - m_mouseYLastFrame; }

f32 Input::GetMouseWheelDelta() const { return m_mouseWheelDelta; }

} // namespace apex