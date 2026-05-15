#pragma once

#include <vector>
#include <vulkan/vulkan.h>

/// @file Renderer.h
/// @brief Vulkan-based rendering subsystem.

namespace apex {

class Window;

/// Singleton Vulkan renderer.
///
/// Owns the VkInstance, debug messenger, surface, physical and logical
/// devices, and swapchain. Initialized after the window has been created
/// so that a surface can be built from the window's native handle.
///
/// Lifecycle:
/// @code
/// apex::Renderer::Init(window);
/// // ... main loop ...
/// apex::Renderer::Shutdown();
/// @endcode
class Renderer {
public:
    // ---- Lifecycle ----

    /// Initialize the renderer. Call once at program start after the
    /// window has been created.
    ///
    /// @param window The window the renderer will present to. Must
    ///               outlive the renderer.
    /// @return true on success. Logs fatal and returns false if any
    ///         step fails.
    static bool Init(Window* window);

    /// Tear down the renderer. Releases swapchain, device, surface,
    /// debug messenger, and instance in reverse order of creation.
    static void Shutdown();

    /// Access the singleton instance.
    /// @pre @ref Init has been called and not followed by @ref Shutdown.
    ///      Violating the precondition triggers an assertion in debug builds.
    static Renderer& Get();

    /// @cond
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;
    /// @endcond

private:
    Renderer() = default;
    ~Renderer() = default;

    // ---- Instance ----
    bool CreateInstance();
    void DestroyInstance();

    // ---- Validation ----
    bool CreateDebugMessenger();
    void DestroyDebugMessenger();

    // ---- Devices ----
    bool PickPhysicalDevice();
    bool CreateDevice();
    void DestroyDevice();

    // ---- Surface ----
    bool CreateSurface();
    void DestroySurface();

    // ---- Swapchain ----
    bool CreateSwapchain();
    void DestroySwapchain();

    static Renderer* s_instance;

    VkInstance m_instance = VK_NULL_HANDLE;                     ///< Connection to the Vulkan loader.
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE; ///< Validation-layer callback handle. Debug builds only.
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;         ///< Selected GPU.
    uint32_t m_graphicsQueueFamilyIndex = UINT32_MAX;           ///< Queue-family index used for graphics and presentation.
    VkDevice m_device = VK_NULL_HANDLE;                         ///< Logical device. Used for nearly all resource creation.
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;                   ///< Queue for graphics command submission. Also handles presentation.

    Window* m_window = nullptr;              ///< Borrowed pointer to the window being presented to.
    VkSurfaceKHR m_surface = VK_NULL_HANDLE; ///< Vulkan handle to the window's drawable area.

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;           ///< Ring of presentable images.
    VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED; ///< Pixel format of the swapchain images.
    VkExtent2D m_swapchainExtent = {0, 0};                 ///< Dimensions of the swapchain images, in pixels.
    std::vector<VkImage> m_swapchainImages;                ///< Image handles owned by the swapchain. Cleared on DestroySwapchain.
};

} // namespace apex
