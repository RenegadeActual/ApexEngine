#include "Renderer.h"

#include "Assert.h"
#include "Common.h"
#include "Log.h"
#include "Window.h"
// clang-format off
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <vulkan/vulkan_win32.h>
// clang-format on
#include <cstring>
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
        delete s_instance;
        s_instance = nullptr;
        return false;
    }

    if (!s_instance->CreateDebugMessenger()) {
        s_instance->DestroyInstance();
        delete s_instance;
        s_instance = nullptr;
        return false;
    }

    if (!s_instance->CreateSurface()) {
        s_instance->DestroyDebugMessenger();
        s_instance->DestroyInstance();
        delete s_instance;
        s_instance = nullptr;
        return false;
    }

    if (!s_instance->PickPhysicalDevice()) {
        s_instance->DestroySurface();
        s_instance->DestroyDebugMessenger();
        s_instance->DestroyInstance();
        delete s_instance;
        s_instance = nullptr;
        return false;
    }

    if (!s_instance->CreateDevice()) {
        s_instance->DestroySurface();
        s_instance->DestroyDebugMessenger();
        s_instance->DestroyInstance();
        delete s_instance;
        s_instance = nullptr;
        return false;
    }
    if (!s_instance->CreateSwapchain()) {
        s_instance->DestroyDevice();
        s_instance->DestroySurface();
        s_instance->DestroyDebugMessenger();
        s_instance->DestroyInstance();
        delete s_instance;
        s_instance = nullptr;
        return false;
    }

    return true;
}

void Renderer::Shutdown() {
    if (s_instance == nullptr) {
        return; // not initialized
    }
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

} // namespace apex