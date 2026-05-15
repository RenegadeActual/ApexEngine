#pragma once

#include <vector>
#include <vulkan/vulkan.h>

/// @file Renderer.h
/// @brief Vulkan-based rendering subsystem.

namespace apex {

class Window;

class Renderer {
public:
    static bool Init(Window* window);
    static void Shutdown();
    static Renderer& Get();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

private:
    Renderer() = default;
    ~Renderer() = default;

    bool CreateInstance();
    void DestroyInstance();

    bool CreateDebugMessenger();
    void DestroyDebugMessenger();

    bool PickPhysicalDevice();

    bool CreateDevice();
    void DestroyDevice();

    bool CreateSurface();
    void DestroySurface();

    bool CreateSwapchain();
    void DestroySwapchain();

    static Renderer* s_instance;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamilyIndex = UINT32_MAX;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;

    Window* m_window = nullptr;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent = {0, 0};
    std::vector<VkImage> m_swapchainImages;
};

} // namespace apex