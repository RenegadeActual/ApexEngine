#include "Input.h"
#include "Log.h"
#include "MaterialDatabase.h"
#include "Renderer.h"
#include "Window.h"

#include <cstdio>
#include <memory>

int main() {
    if (!apex::Log::Init()) {
        std::fprintf(stderr, "Failed to initialize logging. \n");
        return 1;
    }

    LOG_INFO("Engine", "Starting ApexEngine...");

    apex::Log::Get().SetCategoryLevel("Input", apex::LogLevel::Debug);
    apex::Log::Get().SetCategoryLevel("Engine", apex::LogLevel::Debug);

    // initialize material database
    if (!apex::MaterialDatabase::Init()) {
        LOG_FATAL("Game", "Failed to initialize material database.");
        return 1;
    }

    // sanity check
    if (const auto* h = apex::MaterialDatabase::Get().GetElement("H")) {
        LOG_INFO("MaterialDB",
                 "Loaded {} ({}): Z={}, mass={}",
                 h->name,
                 h->symbol,
                 h->atomicNumber,
                 h->atomicMass);
    }

    // initialize input system
    if (!apex::Input::Init()) {
        LOG_FATAL("Engine", "Failed to initialize input system.\n");
        return 1;
    }

    std::unique_ptr<apex::Window> window(apex::Window::Create("Apex Engine - Alpha", 1280, 720));
    if (!window) {
        LOG_FATAL("Engine", "Failed to create window.\n");
        return 1;
    }

    if (!apex::Renderer::Init(window.get())) {
        LOG_FATAL("Engine", "Failed to initialize renderer.\n");
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

    apex::Renderer::Shutdown();

    // Destroy the window before tearing down Input. The window's destructor
    // can dispatch a final WM_KILLFOCUS via DestroyWindow, which calls into
    // Input::Get(); doing this in the wrong order would assert.
    window.reset();
    apex::Input::Shutdown();
    LOG_INFO("Engine", "Goodbye!");
    apex::Log::Shutdown();
    return 0;
}