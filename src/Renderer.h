#pragma once

#include "Common.h"

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

    /// Renders a single frame. Call once per frame after Init and before Shutdown.
    void DrawFrame();

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
    bool RecreateSwapchain();

    // ---- Image Views ----
    bool CreateImageViews();
    void DestroyImageViews();

    // ---- Render Pass ----
    bool CreateRenderPass();
    void DestroyRenderPass();

    // ---- Framebuffers ----
    bool CreateFramebuffers();
    void DestroyFramebuffers();

    // ---- Pipeline Layout
    bool CreatePipelineLayout();
    void DestroyPipelineLayout();

    // ---- Graphics Pipeline ----
    bool CreateGraphicsPipeline();
    void DestroyGraphicsPipeline();

    // ---- Command Pool ----
    bool CreateCommandPool();
    void DestroyCommandPool();

    // ---- Command Buffer ----
    bool CreateCommandBuffer();

    // ---- Synchronization Objects ----
    bool CreateSyncObjects();
    void DestroySyncObjects();

    void RecordCommandBuffer(u32 imageIndex);

    static Renderer* s_instance;

    /// Connection to the Vulkan loader.
    VkInstance m_instance = VK_NULL_HANDLE;

    /// Validation-layer callback handle. Debug builds only.
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    /// Selected GPU.
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    /// Queue-family index used for graphics and presentation.
    uint32_t m_graphicsQueueFamilyIndex = UINT32_MAX;

    /// Logical device. Used for nearly all resource creation.
    VkDevice m_device = VK_NULL_HANDLE;

    /// Queue for graphics command submission. Also handles presentation.
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;

    /// Borrowed pointer to the window being presented to.
    Window* m_window = nullptr;

    /// Vulkan handle to the window's drawable area.
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    /// Ring of presentable images.
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;

    /// Pixel format of the swapchain images.
    VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED;

    /// Dimensions of the swapchain images, in pixels.
    VkExtent2D m_swapchainExtent = {0, 0};

    /// Image handles owned by the swapchain. Cleared on DestroySwapchain.
    std::vector<VkImage> m_swapchainImages;

    /// One view per swapchain image. Used as render-pass color attachments.
    std::vector<VkImageView> m_swapchainImageViews;

    /// Render pass that describes how the color attachment is loaded, written, and stored.
    VkRenderPass m_renderPass = VK_NULL_HANDLE;

    /// One framebuffer per swapchain image view.
    std::vector<VkFramebuffer> m_swapchainFramebuffers;

    /// Pipeline layout. Declares uniforms and push constants for graphics pipelines
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    /// Graphics Pipeline that draws the triangle
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;

    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;

    /// Signaled when rendering is complete and the image is ready to present.
    /// One per swapchain image; indexed by acquired image index.
    std::vector<VkSemaphore> m_renderFinishedSemaphores;

    VkFence m_inFlightFence = VK_NULL_HANDLE;
};

} // namespace apex
