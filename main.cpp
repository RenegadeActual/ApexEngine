#include "Window.h"

#include <cstdio>

int main() {
    apex::Window* window = apex::Window::Create("Apex Engine - Alpha", 1280, 720);
    if (window == nullptr) {
        std::printf("Failed to create window.\n");
        return 1;
    }

    while (!window->ShouldClose()) {
        window->PollEvents();
        // Eventually: renderer.DrawFrame();
    }

    delete window;
    return 0;
}