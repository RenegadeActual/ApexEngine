#include "Window.h"
#include "Input.h"

#include <cstdio>

int main() {
    apex::Window* window = apex::Window::Create("Apex Engine - Alpha", 1280, 720);
    if (window == nullptr) {
        std::printf("Failed to create window.\n");
        return 1;
    }

    // initialize input system
    if (!apex::Input::Init()) {
        std::printf("Failed to initialize input system.\n");
        delete window;
        return 1;
    }

    while (!window->ShouldClose()) {
        apex::Input::Get().NewFrame(); // snapshot input state at the start of the frame
        window->PollEvents();

        apex::Input& input = apex::Input::Get(); // cache apex::Input::Get() to prevent multiple calls in the same frame

        if (input.WasKeyPressed(apex::Key::Space)) {
            std::printf("Space was pressed.\n");
        }
        if (input.WasKeyReleased(apex::Key::Space)) {
            std::printf("Space was released.\n");
        }
        if (input.WasKeyPressed(apex::Key::Escape)) {
            std::printf("Escape was pressed - exiting.\n");
            break; // exit the loop if Escape was pressed
        }
        if (input.WasMouseButtonPressed(apex::MouseButton::Left)) {
            std::printf("Left mouse button was pressed at (%d, %d).\n", input.GetMouseX(), input.GetMouseY());
        }

        // Eventually: renderer.DrawFrame();
    }
    apex::Input::Shutdown(); // clean up input system
    delete window;
    return 0;
}