#include "Input.h"
#include "Log.h"
#include "Window.h"

#include <cstdio>

int main() {
    if (!apex::Log::Init()) {
        std::fprintf(stderr, "Failed to initialize logging. \n");
        return 1;
    }

    LOG_INFO("Engine", "Starting ApexEngine...");

    apex::Log::Get().SetCategoryLevel("Input", apex::LogLevel::Debug);
    apex::Log::Get().SetCategoryLevel("Engine", apex::LogLevel::Debug);

    // initialize input system
    if (!apex::Input::Init()) {
        LOG_FATAL("Engine", "Failed to initialize input system.\n");
        return 1;
    }

    apex::Window* window = apex::Window::Create("Apex Engine - Alpha", 1280, 720);
    if (window == nullptr) {
        LOG_FATAL("Engine", "Failed to create window.\n");
        return 1;
    }

    LOG_INFO("Engine", "Initialization complete. Entering main loop.");

    while (!window->ShouldClose()) {
        apex::Input::Get().NewFrame();
        window->PollEvents();

        apex::Input& input = apex::Input::Get();

        if (input.WasKeyPressed(apex::Key::Space)) {
            LOG_DEBUG("Input", "Space was pressed.");
        }
        if (input.WasKeyReleased(apex::Key::Space)) {
            LOG_DEBUG("Input", "Space was released.");
        }
        if (input.WasKeyPressed(apex::Key::Escape)) {
            LOG_DEBUG("Engine", "Escape was pressed - exiting.");
            break; // exit the loop if Escape was pressed
        }
        if (input.WasMouseButtonPressed(apex::MouseButton::Left)) {
            LOG_DEBUG("Input",
                      "Left mouse button was pressed at ({}, {}).",
                      input.GetMouseX(),
                      input.GetMouseY());
        }

        // Eventually: renderer.DrawFrame();
    }

    delete window;
    apex::Input::Shutdown();
    LOG_INFO("Engine", "Goodbye!");
    apex::Log::Shutdown();
    return 0;
}