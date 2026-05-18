#include "Renderer.h"

#include "Assert.h"
#include "Log.h"
#include "Window.h"
// clang-format off
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <vulkan/vulkan_win32.h>
// clang-format on
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <vector>

namespace {

#ifdef NDEBUG
constexpr bool kEnableValidation = false;
#else
constexpr bool kEnableValidation = true;
#endif

const char* const kValidationLayers[] = {
    "VK_LAYER_KHRONOS_validation",
};
const char* const kDeviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
constexpr uint32_t kValidationLayerCount = sizeof(kValidationLayers) / sizeof(kValidationLayers[0]);
constexpr uint32_t kDeviceExtensionCount = sizeof(kDeviceExtensions) / sizeof(kDeviceExtensions[0]);

// Vulkan invokes this when validation needs to tell us something.
VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
              VkDebugUtilsMessageTypeFlagsEXT type,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* pUserData) {
    (void)type;
    (void)pUserData;

    switch (severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        LOG_TRACE("Vulkan", "{}", pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        LOG_INFO("Vulkan", "{}", pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        LOG_WARN("Vulkan", "{}", pCallbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        LOG_ERROR("Vulkan", "{}", pCallbackData->pMessage);
        break;
    default:
        LOG_INFO("Vulkan", "{}", pCallbackData->pMessage);
        break;
    }
    // "log this and continue" do not abort the call on validation errors, even severe ones. The app
    // should be able to handle them gracefully and keep running.
    return VK_FALSE;
}

bool CheckValidationLayerSupport() {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    if (count == 0) {
        return false;
    }

    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());

    for (uint32_t i = 0; i < kValidationLayerCount; ++i) {
        bool found = false;
        for (const auto& layer : available) {
            if (std::strcmp(layer.layerName, kValidationLayers[i]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

uint32_t FindGraphicsQueueFamily(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            return i;
        }
    }
    return UINT32_MAX; // not found
}

// Human readable names for Vulkan Device Types.
const char* PhysicalDeviceTypeString(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    default:
        return "Other";
    }
}

// Returns the directory of the running executable. Used so shader paths
// work no matter what the current working directory is at launch.
std::filesystem::path GetExecutableDirectory() {
    char buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
}

// Reads a file into a byte buffer. Returns empty on failure.
std::vector<char> ReadBinaryFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }
    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

// Wraps SPIR-V bytecode in a VkShaderModule. Returns VK_NULL_HANDLE on
// failure. The caller owns the returned module and must vkDestroyShaderModule it.
VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

VkSurfaceFormatKHR ChooseSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];
}

VkPresentModeKHR ChooseSwapchainPresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (VkPresentModeKHR m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            return m;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // guaranteed to be supported
}

VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                 uint32_t windowWidth,
                                 uint32_t windowHeight) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = {windowWidth, windowHeight};
    if (extent.width < capabilities.minImageExtent.width) {
        extent.width = capabilities.minImageExtent.width;
    } else if (extent.width > capabilities.maxImageExtent.width) {
        extent.width = capabilities.maxImageExtent.width;
    }
    if (extent.height < capabilities.minImageExtent.height) {
        extent.height = capabilities.minImageExtent.height;
    } else if (extent.height > capabilities.maxImageExtent.height) {
        extent.height = capabilities.maxImageExtent.height;
    }
    return extent;
}

} // namespace

namespace apex {

Renderer* Renderer::s_instance = nullptr;

bool Renderer::Init(Window* window) {
    APEX_ASSERT(window != nullptr, "Renderer::Init called with null window");

    if (s_instance != nullptr) {
        return true; // already initialized
    }

    s_instance = new (std::nothrow) Renderer();
    if (s_instance == nullptr) {
        LOG_FATAL("Renderer", "Failed to allocate Renderer.");
        return false;
    }
    s_instance->m_window = window;

    if (!s_instance->CreateInstance()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateDebugMessenger()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateSurface()) {
        Shutdown();
        return false;
    }
    if (!s_instance->PickPhysicalDevice()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateDevice()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateSwapchain()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateImageViews()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateRenderPass()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateFramebuffers()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreatePipelineLayout()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateGraphicsPipeline()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateCommandPool()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateCommandBuffer()) {
        Shutdown();
        return false;
    }
    if (!s_instance->CreateSyncObjects()) {
        Shutdown();
        return false;
    }

    return true;
}

void Renderer::Shutdown() {
    if (s_instance == nullptr) {
        return; // not initialized
    }

    // Make sure the GPU is done with everything before tear down.
    if (s_instance->m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(s_instance->m_device);
    }

    s_instance->DestroySyncObjects();
    s_instance->DestroyCommandPool();
    s_instance->DestroyGraphicsPipeline();
    s_instance->DestroyPipelineLayout();
    s_instance->DestroyFramebuffers();
    s_instance->DestroyRenderPass();
    s_instance->DestroyImageViews();
    s_instance->DestroySwapchain();
    s_instance->DestroyDevice();
    s_instance->DestroySurface();
    s_instance->DestroyDebugMessenger();
    s_instance->DestroyInstance();
    delete s_instance;
    s_instance = nullptr;
}

Renderer& Renderer::Get() {
    APEX_ASSERT(s_instance != nullptr, "Renderer::Get() called before Renderer::Init().");
    return *s_instance;
}

bool Renderer::CreateInstance() {

    // Required Layer Extensions
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    if constexpr (kEnableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Validation Layers
    if constexpr (kEnableValidation) {
        if (!CheckValidationLayerSupport()) {
            LOG_FATAL("Renderer", "Validation layer requested but not available.");
            return false;
        }
    }
    // Application Info - tells the driver who we are.
    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Apex Application";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.pEngineName = "ApexEngine";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Instance creation info - tells the driver what we want.
    VkInstanceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();
    if constexpr (kEnableValidation) {
        createInfo.enabledLayerCount = kValidationLayerCount;
        createInfo.ppEnabledLayerNames = kValidationLayers;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        LOG_FATAL("Renderer", "vkCreateInstance failed (VkResult = {}).", static_cast<int>(result));
        return false;
    }

    LOG_INFO("Renderer", "Vulkan instance created successfully.");
    return true;
}

void Renderer::DestroyInstance() {
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        LOG_INFO("Renderer", "Vulkan instance destroyed.");
    }
}

bool Renderer::CreateDebugMessenger() {
    if constexpr (!kEnableValidation) {
        return true; // validation is disabled, so we don't need a debug messenger
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
    createInfo.pUserData = nullptr;

    // vkCreateDebugUtilsMessengerEXT is an extension function, so we have to load it manually.
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (fn == nullptr) {
        LOG_FATAL("Renderer", "vkCreateDebugUtilsMessengerEXT not found.");
        return false;
    }

    const VkResult result = fn(m_instance, &createInfo, nullptr, &m_debugMessenger);
    if (result != VK_SUCCESS) {
        LOG_FATAL("Renderer",
                  "vkCreateDebugUtilsMessengerEXT failed (VkResult = {}).",
                  static_cast<int>(result));
        return false;
    }

    LOG_INFO("Renderer", "Vulkan debug messenger created.");
    return true;
}

void Renderer::DestroyDebugMessenger() {
    if (m_debugMessenger == VK_NULL_HANDLE) {
        return; // no debug messenger to destroy
    }

    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fn != nullptr) {
        fn(m_instance, m_debugMessenger, nullptr);
    }
    m_debugMessenger = VK_NULL_HANDLE;
    LOG_INFO("Renderer", "Debug messenger destroyed.");
}

bool Renderer::PickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_FATAL("Renderer", "No Vulkan-compatible GPUs found.");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    LOG_INFO("Renderer", "Found {} Vulkan-compatible GPU(s):", deviceCount);

    VkPhysicalDevice best = VK_NULL_HANDLE;
    uint32_t bestQueueFamily = UINT32_MAX;
    bool bestIsDiscrete = false;

    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        LOG_INFO(
            "Renderer", " - {} ({})", props.deviceName, PhysicalDeviceTypeString(props.deviceType));

        const uint32_t graphicsFamily = FindGraphicsQueueFamily(device);
        if (graphicsFamily == UINT32_MAX) {
            continue;
        }

        // Verify this queue family can also present to our window surface, which is required to
        // actually show anything.
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, graphicsFamily, m_surface, &presentSupport);
        if (presentSupport == VK_FALSE) {
            continue;
        }

        const bool isDiscrete = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

        // Prefer discrete GPUs over integrated ones, but fall back to anything with graphics
        // support.
        if (best == VK_NULL_HANDLE || (isDiscrete && !bestIsDiscrete)) {
            best = device;
            bestQueueFamily = graphicsFamily;
            bestIsDiscrete = isDiscrete;
        }
    }

    if (best == VK_NULL_HANDLE) {
        LOG_FATAL("Renderer", "No suitable GPU with graphics support found. (need graphics queue)");
        return false;
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(best, &props);
    LOG_INFO(
        "Renderer", "Selected: {} (graphics queue family {})", props.deviceName, bestQueueFamily);

    m_physicalDevice = best;
    m_graphicsQueueFamilyIndex = bestQueueFamily;
    return true;
}

bool Renderer::CreateDevice() {
    const float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // Opt-in to any device features we want here. For now we don't need any.
    VkPhysicalDeviceFeatures deviceFeatures {};

    VkDeviceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = kDeviceExtensionCount;
    createInfo.ppEnabledExtensionNames = kDeviceExtensions;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    const VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS) {
        LOG_FATAL("Renderer", "vkCreateDevice failed (VkResult = {}).", static_cast<int>(result));
        return false;
    }

    // Retrieve the graphics queue hangdle for later use.
    vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);

    LOG_INFO("Renderer", "Logical device created and graphics queue retrieved.");
    return true;
}

void Renderer::DestroyDevice() {
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
        LOG_INFO("Renderer", "Logical device destroyed.");
    }
}

bool Renderer::CreateSurface() {
    APEX_ASSERT(m_window != nullptr, "CreateSurface called without a window.");

    VkWin32SurfaceCreateInfoKHR createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = static_cast<HWND>(m_window->GetNativeHandle());
    createInfo.hinstance = GetModuleHandleW(nullptr);

    const VkResult result = vkCreateWin32SurfaceKHR(m_instance, &createInfo, nullptr, &m_surface);
    if (result != VK_SUCCESS) {
        LOG_FATAL("Renderer",
                  "vkCreateWin32SurfaceKHR failed (VkResult = {}).",
                  static_cast<int>(result));
        return false;
    }
    LOG_INFO("Renderer", "Window surface created.");
    return true;
}

void Renderer::DestroySurface() {
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
        LOG_INFO("Renderer", "Window surface destroyed.");
    }
}

bool Renderer::CreateSwapchain() {
    APEX_ASSERT(m_window != nullptr, "CreateSwapchain called without a window");

    // 1. Query surface capabilities (image count limits, current extent, etc.)
    VkSurfaceCapabilitiesKHR capabilities {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

    // 2. Query supported formats.
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    if (formatCount == 0) {
        LOG_FATAL("Renderer", "No surface formats available.");
        return false;
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

    // 3. Query supported present modes.
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_physicalDevice, m_surface, &presentModeCount, nullptr);
    if (presentModeCount == 0) {
        LOG_FATAL("Renderer", "No surface present modes available.");
        return false;
    }
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_physicalDevice, m_surface, &presentModeCount, presentModes.data());

    // 4. Pick swapchain parameters via the helpers.
    const VkSurfaceFormatKHR surfaceFormat = ChooseSwapchainSurfaceFormat(formats);
    const VkPresentModeKHR presentMode = ChooseSwapchainPresentMode(presentModes);
    const VkExtent2D extent =
        ChooseSwapchainExtent(capabilities, m_window->GetWidth(), m_window->GetHeight());

    // 5. Ask for minImageCount + 1 to allow triple-buffering when MAILBOX
    // is available. Clamp to maxImageCount, but treat 0 as "no maximum."
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // 6. Build the create-info struct.
    VkSwapchainCreateInfoKHR createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    // 7. Create the swapchain.
    const VkResult result = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain);
    if (result != VK_SUCCESS) {
        LOG_FATAL(
            "Renderer", "vkCreateSwapchainKHR failed (VkResult = {}).", static_cast<int>(result));
        return false;
    }

    // 8. Retrieve image handles. The driver may give us more than we asked
    // for, so query the actual count.
    uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount, nullptr);
    m_swapchainImages.resize(actualImageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount, m_swapchainImages.data());

    // 9. Stash format + extent for later code (image views, render pass).
    m_swapchainImageFormat = surfaceFormat.format;
    m_swapchainExtent = extent;

    LOG_INFO("Renderer",
             "Swapchain created: {}x{}, {} images, present mode {}",
             extent.width,
             extent.height,
             actualImageCount,
             static_cast<int>(presentMode));
    return true;
}

void Renderer::DestroySwapchain() {
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
        m_swapchainImages.clear(); // images are owned by the swapchain
        LOG_INFO("Renderer", "Swapchain destroyed.");
    }
}

bool Renderer::RecreateSwapchain() {
    // TODO: per-WM_SIZE recreation leaves a small black sliver on the leading
    // edge during fast drags — the gap between Windows exposing the new area
    // and the next swapchain present landing. Eliminating it cleanly requires
    // bypassing standard Vulkan WSI presentation (window created with
    // WS_EX_NOREDIRECTIONBITMAP, presenting through DirectComposition or
    // custom DXGI integration). Deferred as a future engine upgrade.

    // If the window is minimized, the surface dimensions are 0x0 and we
    // can't create a swapchain. Skip and try again next frame.
    if (m_window->GetWidth() == 0 || m_window->GetHeight() == 0) {
        return false;
    }

    // Wait for the GPU to finish before destroying anything it might
    // still be reading from.
    vkDeviceWaitIdle(m_device);

    // Tear down the resources that depend on the swapchain's dimensions
    // and format, in reverse order of creation.
    DestroyFramebuffers();
    DestroyImageViews();
    DestroySwapchain();

    // Build them back up. CreateSwapchain re-queries the surface
    // capabilities, so it picks up the new size automatically.
    if (!CreateSwapchain()) {
        return false;
    }
    if (!CreateImageViews()) {
        return false;
    }
    if (!CreateFramebuffers()) {
        return false;
    }

    LOG_INFO("Renderer",
             "Swapchain recreated: {}x{}",
             m_swapchainExtent.width,
             m_swapchainExtent.height);
    return true;
}

bool Renderer::CreateImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImages.size(); i++) {
        VkImageViewCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_swapchainImageFormat;

        // Identify component mapping
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // which subset of image this view covers
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        const VkResult result =
            vkCreateImageView(m_device, &createInfo, nullptr, &m_swapchainImageViews[i]);
        if (result != VK_SUCCESS) {
            LOG_FATAL("Renderer",
                      "vkCreateImageView failed for swapchain image {} (VkResult = {})",
                      i,
                      static_cast<int>(result));
            return false;
        }
    }
    LOG_INFO("Renderer", "Created {} image views.", m_swapchainImageViews.size());
    return true;
}

void Renderer::DestroyImageViews() {
    for (VkImageView view : m_swapchainImageViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, view, nullptr);
        }
    }
    m_swapchainImageViews.clear();
    LOG_INFO("Renderer", "ImageViews destroyed.");
}

bool Renderer::CreateRenderPass() {
    // One color attachment matching the swapchain image format.
    VkAttachmentDescription colorAttachment {};
    colorAttachment.format = m_swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    // CLEAR the image at the start of the pass; STORE it at the end so
    // we can present it.
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // No stencil.
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Initial layout doesn't matter because we're clearing. Final layout
    // must be PRESENT_SRC_KHR so the swapchain can present it.
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Reference to the attachment from the subpass.
    VkAttachmentReference colorAttachmentRef {};
    colorAttachmentRef.attachment = 0; // index into pAttachments below
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // One subpass, writes to the color attachment.
    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // Synchronize image acquisition with the subpass. Without this the
    // subpass might start writing to the image before the presentation
    // engine is done reading it from the previous frame.
    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // work before this pass
    dependency.dstSubpass = 0;                   // our only subpass
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;

    const VkResult result = vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass);
    if (result != VK_SUCCESS) {
        LOG_FATAL(
            "Renderer", "vkCreateRenderPass failed (VkResult = {}).", static_cast<int>(result));
        return false;
    }

    LOG_INFO("Renderer", "Render pass created.");
    return true;
}

void Renderer::DestroyRenderPass() {
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    LOG_INFO("Renderer", "Destroyed Render Pass.");
}

bool Renderer::CreateFramebuffers() {
    m_swapchainFramebuffers.resize(m_swapchainImageViews.size());

    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
        VkImageView attachments[] = {m_swapchainImageViews[i]};

        VkFramebufferCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = m_renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = m_swapchainExtent.width;
        createInfo.height = m_swapchainExtent.height;
        createInfo.layers = 1;

        const VkResult result =
            vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_swapchainFramebuffers[i]);
        if (result != VK_SUCCESS) {
            LOG_FATAL("Renderer",
                      "vkCreateFramebuffer failed for image {} (VkResult = {}).",
                      i,
                      static_cast<int>(result));
            return false;
        }
    }

    LOG_INFO("Renderer", "Created {} framebuffers.", m_swapchainFramebuffers.size());
    return true;
}

void Renderer::DestroyFramebuffers() {
    for (VkFramebuffer framebuffer : m_swapchainFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
    }
    m_swapchainFramebuffers.clear();
    LOG_INFO("Renderer", "Destroyed framebuffers.");
}

bool Renderer::CreatePipelineLayout() {
    VkPipelineLayoutCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.setLayoutCount = 0; // no descriptor sets yet
    createInfo.pSetLayouts = nullptr;
    createInfo.pushConstantRangeCount = 0; // no push constants yet
    createInfo.pPushConstantRanges = nullptr;

    const VkResult result =
        vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout);
    if (result != VK_SUCCESS) {
        LOG_FATAL(
            "Renderer", "vkCreatePipelineLayout failed (VkResult = {}).", static_cast<int>(result));
        return false;
    }
    LOG_INFO("Renderer", "Pipeline layout created.");
    return true;
}

void Renderer::DestroyPipelineLayout() {
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    LOG_INFO("Renderer", "Destroyed Pipeline Layout.");
}

bool Renderer::CreateGraphicsPipeline() {
    // 1. Load SPIR-V from <exe-dir>/shaders/.
    const std::filesystem::path shaderDir = GetExecutableDirectory() / "shaders";
    const std::vector<char> vertCode = ReadBinaryFile(shaderDir / "triangle.vert.spv");
    const std::vector<char> fragCode = ReadBinaryFile(shaderDir / "triangle.frag.spv");
    if (vertCode.empty() || fragCode.empty()) {
        LOG_FATAL("Renderer", "Failed to load shader SPIR-V from {}", shaderDir.string());
        return false;
    }

    // 2. Wrap each blob in a VkShaderModule.
    VkShaderModule vertModule = CreateShaderModule(m_device, vertCode);
    VkShaderModule fragModule = CreateShaderModule(m_device, fragCode);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        LOG_FATAL("Renderer", "vkCreateShaderModule failed.");
        if (vertModule != VK_NULL_HANDLE)
            vkDestroyShaderModule(m_device, vertModule, nullptr);
        if (fragModule != VK_NULL_HANDLE)
            vkDestroyShaderModule(m_device, fragModule, nullptr);
        return false;
    }

    // 3. Tell the pipeline which shader to use at each stage.
    VkPipelineShaderStageCreateInfo vertStage {};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage {};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    const VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

    // 4. Vertex input: empty. Shader hardcodes positions, no buffers.
    VkPipelineVertexInputStateCreateInfo vertexInput {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 0;
    vertexInput.vertexAttributeDescriptionCount = 0;

    // 5. Input assembly: triples of vertices form triangles.
    VkPipelineInputAssemblyStateCreateInfo inputAssembly {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 6. Viewport + scissor are declared as dynamic state (set each
    // frame in RecordCommandBuffer) so the pipeline doesn't bake in
    // a fixed size. Required for resize handling.
    VkPipelineViewportStateCreateInfo viewportState {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Dynamic state — values set at command-buffer record time instead
    // of baked into the pipeline.
    const VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
    dynamicState.pDynamicStates = dynamicStates;

    // 7. Rasterization: fill triangles, no face culling.
    VkPipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 8. Multisample: off.
    VkPipelineMultisampleStateCreateInfo multisample {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 9. Color blend: no blending, write all channels of the one color attachment.
    VkPipelineColorBlendAttachmentState colorBlendAttachment {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlend {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    // 10. The big create-info that ties it all together.
    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    const VkResult result = vkCreateGraphicsPipelines(
        m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline);

    // Shader modules can be destroyed right after pipeline creation; the
    // pipeline keeps its own internal compiled copy.
    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);

    if (result != VK_SUCCESS) {
        LOG_FATAL("Renderer",
                  "vkCreateGraphicsPipelines failed (VkResult = {}).",
                  static_cast<int>(result));
        return false;
    }

    LOG_INFO("Renderer", "Graphics pipeline created.");
    return true;
}

void Renderer::DestroyGraphicsPipeline() {
    if (m_graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        m_graphicsPipeline = VK_NULL_HANDLE;
    }
    LOG_INFO("Renderer", "Destroyed Graphics Pipeline.");
}

bool Renderer::CreateCommandPool() {
    VkCommandPoolCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // RESET_COMMAND_BUFFER_BIT lets us call vkResetCommandBuffer on
    // individual buffers from this pool. Without it, the only way to
    // re-record a buffer is to reset the whole pool.
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;

    const VkResult result = vkCreateCommandPool(m_device, &createInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS) {
        LOG_FATAL(
            "Renderer", "vkCreateCommandPool failed (VkResult = {}).", static_cast<int>(result));
        return false;
    }

    LOG_INFO("Renderer", "Command pool created.");
    return true;
}

void Renderer::DestroyCommandPool() {
    if (m_commandPool != VK_NULL_HANDLE) {
        // Destroying the pool also frees every command buffer allocated
        // from it, including m_commandBuffer.
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
        m_commandBuffer = VK_NULL_HANDLE; // pool teardown invalidated this
    }
}

bool Renderer::CreateCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    // PRIMARY = can be submitted directly to a queue. SECONDARY = can only
    // be called from a primary buffer; useful for reusable sub-sequences.
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    const VkResult result = vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer);
    if (result != VK_SUCCESS) {
        LOG_FATAL("Renderer",
                  "vkAllocateCommandBuffers failed (VkResult = {}).",
                  static_cast<int>(result));
        return false;
    }

    LOG_INFO("Renderer", "Command buffer allocated.");
    return true;
}

bool Renderer::CreateSyncObjects() {
    VkSemaphoreCreateInfo semInfo {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Created in signaled state so the very first frame's wait-for-fence
    // doesn't deadlock. After the first submit, it follows the normal
    // signaled-on-completion lifecycle.
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFence) != VK_SUCCESS) {
        LOG_FATAL("Renderer", "Failed to create sync objects.");
        return false;
    }

    m_renderFinishedSemaphores.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); ++i) {
        if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinishedSemaphores[i]) !=
            VK_SUCCESS) {
            LOG_FATAL("Renderer", "Failed to create per-image render-finished semaphore.");
            return false;
        }
    }

    LOG_INFO("Renderer", "Sync objects created.");
    return true;
}

void Renderer::DestroySyncObjects() {
    if (m_inFlightFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device, m_inFlightFence, nullptr);
        m_inFlightFence = VK_NULL_HANDLE;
    }
    for (VkSemaphore sem : m_renderFinishedSemaphores) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, sem, nullptr);
        }
    }
    m_renderFinishedSemaphores.clear();
    if (m_imageAvailableSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
        m_imageAvailableSemaphore = VK_NULL_HANDLE;
    }
}

void Renderer::RecordCommandBuffer(uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(m_commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR("Renderer", "vkBeginCommandBuffer failed.");
        return;
    }

    // Dark blue clear so the rainbow triangle stands out.
    VkClearValue clearColor {};
    clearColor.color = {{0.0f, 0.0f, 0.05f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchainExtent.width);
    viewport.height = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);

    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchainExtent;
    vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

    vkCmdDraw(m_commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(m_commandBuffer);

    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS) {
        LOG_ERROR("Renderer", "vkEndCommandBuffer failed.");
    }
}

void Renderer::DrawFrame() {
    // 1. Wait for the previous frame's GPU work to finish.
    vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);

    // 2. Acquire the next swapchain image. If the surface is out of date
    // (typically a window resize), recreate the swapchain and skip this
    // frame. SUBOPTIMAL_KHR means the swapchain still works but isn't
    // ideal — we render this frame anyway and recreate after present.
    uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        m_device, m_swapchain, UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR("Renderer",
                  "vkAcquireNextImageKHR failed (VkResult = {}).",
                  static_cast<int>(acquireResult));
        return;
    }

    // Only reset the fence now that we know we'll actually submit.
    vkResetFences(m_device, 1, &m_inFlightFence);

    // 3. Reset and re-record the command buffer for this frame's image.
    vkResetCommandBuffer(m_commandBuffer, 0);
    RecordCommandBuffer(imageIndex);

    // 4. Submit. Wait on image-available before the color-output stage so the
    // vertex shader can start running before the image is ready. Signal
    // render-finished + the in-flight fence when the GPU is done.
    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[imageIndex]};

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFence) != VK_SUCCESS) {
        LOG_ERROR("Renderer", "vkQueueSubmit failed.");
        return;
    }

    // 5. Present. Wait on render-finished so the OS doesn't display a
    // half-rendered frame.
    VkSwapchainKHR swapchains[] = {m_swapchain};

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        RecreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        LOG_ERROR("Renderer",
                  "vkQueuePresentKHR failed (VkResult = {}).",
                  static_cast<int>(presentResult));
    }
}

} // namespace apex