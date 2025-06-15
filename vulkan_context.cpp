#include "vulkan_context.h"
#include "utils/logger.h"
#include <set>
#include <algorithm>
#include <limits>

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOGW("Validation layer: {}", pCallbackData->pMessage);
    }
    
    return VK_FALSE;
}

VulkanContext::VulkanContext() : window_(nullptr) {
}

VulkanContext::~VulkanContext() {
    cleanup();
}

bool VulkanContext::initialize(GLFWwindow* window) {
    window_ = window;

    if (!createInstance()) {
        LOGE("Failed to create Vulkan instance");
        return false;
    }

    if (!setupDebugMessenger()) {
        LOGE("Failed to setup debug messenger");
        return false;
    }

    if (!createSurface()) {
        LOGE("Failed to create window surface");
        return false;
    }

    if (!pickPhysicalDevice()) {
        LOGE("Failed to find a suitable GPU");
        return false;
    }

    if (!createLogicalDevice()) {
        LOGE("Failed to create logical device");
        return false;
    }

    if (!createSwapChain()) {
        LOGE("Failed to create swap chain");
        return false;
    }

    if (!createImageViews()) {
        LOGE("Failed to create image views");
        return false;
    }

    if (!createCommandPool()) {
        LOGE("Failed to create command pool");
        return false;
    }

    LOGI("Vulkan context initialized successfully");
    return true;
}

void VulkanContext::cleanup() {
    if (device_) {
        device_.waitIdle();
        
        cleanupSwapChain();
        
        if (command_pool_) {
            device_.destroyCommandPool(command_pool_);
        }
        
        device_.destroy();
    }

    if (instance_) {
        if (enable_validation_layers_ && debug_messenger_) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)instance_.getProcAddr("vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
                func(instance_, debug_messenger_, nullptr);
            }
        }

        if (surface_) {
            instance_.destroySurfaceKHR(surface_);
        }
        
        instance_.destroy();
    }
}

bool VulkanContext::createInstance() {
    vk::ApplicationInfo app_info{};
    app_info.pApplicationName = "Vulkan Renderer";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    vk::InstanceCreateInfo create_info{};
    create_info.pApplicationInfo = &app_info;

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    if (enable_validation_layers_) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    if (enable_validation_layers_) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
        create_info.ppEnabledLayerNames = validation_layers_.data();
    } else {
        create_info.enabledLayerCount = 0;
    }

    try {
        instance_ = vk::createInstance(create_info);
        return true;
    } catch (const std::exception& e) {
        LOGE("Failed to create instance: {}", e.what());
        return false;
    }
}

bool VulkanContext::setupDebugMessenger() {
    if (!enable_validation_layers_) return true;

    vk::DebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                              vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    create_info.pfnUserCallback = debugCallback;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)instance_.getProcAddr("vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        VkResult result = func(instance_, reinterpret_cast<const VkDebugUtilsMessengerCreateInfoEXT*>(&create_info), nullptr, reinterpret_cast<VkDebugUtilsMessengerEXT*>(&debug_messenger_));
        return result == VK_SUCCESS;
    }

    return false;
}

bool VulkanContext::createSurface() {
    VkSurfaceKHR surface;
    VkResult result = glfwCreateWindowSurface(instance_, window_, nullptr, &surface);
    surface_ = surface;
    return result == VK_SUCCESS;
}

bool VulkanContext::pickPhysicalDevice() {
    auto devices = instance_.enumeratePhysicalDevices();

    if (devices.empty()) {
        LOGE("Failed to find GPUs with Vulkan support");
        return false;
    }

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physical_device_ = device;
            break;
        }
    }

    if (!physical_device_) {
        LOGE("Failed to find a suitable GPU");
        return false;
    }

    return true;
}

bool VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physical_device_);

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = {indices.graphics_family.value(), indices.present_family.value()};

    float queue_priority = 1.0f;
    for (uint32_t queue_family : unique_queue_families) {
        vk::DeviceQueueCreateInfo queue_create_info{};
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    vk::PhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;

    vk::DeviceCreateInfo create_info{};
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions_.size());
    create_info.ppEnabledExtensionNames = device_extensions_.data();

    if (enable_validation_layers_) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers_.size());
        create_info.ppEnabledLayerNames = validation_layers_.data();
    } else {
        create_info.enabledLayerCount = 0;
    }

    try {
        device_ = physical_device_.createDevice(create_info);
        graphics_queue_ = device_.getQueue(indices.graphics_family.value(), 0);
        present_queue_ = device_.getQueue(indices.present_family.value(), 0);
        return true;
    } catch (const std::exception& e) {
        LOGE("Failed to create logical device: {}", e.what());
        return false;
    }
}

bool VulkanContext::createSwapChain() {
    SwapChainSupportDetails swap_chain_support = querySwapChainSupport(physical_device_);

    vk::SurfaceFormatKHR surface_format = chooseSwapSurfaceFormat(swap_chain_support.formats);
    vk::PresentModeKHR present_mode = chooseSwapPresentMode(swap_chain_support.present_modes);
    vk::Extent2D extent = chooseSwapExtent(swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount) {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info{};
    create_info.surface = surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = findQueueFamilies(physical_device_);
    uint32_t queue_family_indices[] = {indices.graphics_family.value(), indices.present_family.value()};

    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = vk::SharingMode::eExclusive;
    }

    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = nullptr;

    try {
        swap_chain_ = device_.createSwapchainKHR(create_info);
        swap_chain_images_ = device_.getSwapchainImagesKHR(swap_chain_);
        swap_chain_image_format_ = surface_format.format;
        swap_chain_extent_ = extent;
        return true;
    } catch (const std::exception& e) {
        LOGE("Failed to create swap chain: {}", e.what());
        return false;
    }
}

bool VulkanContext::createImageViews() {
    swap_chain_image_views_.resize(swap_chain_images_.size());

    for (size_t i = 0; i < swap_chain_images_.size(); i++) {
        vk::ImageViewCreateInfo create_info{};
        create_info.image = swap_chain_images_[i];
        create_info.viewType = vk::ImageViewType::e2D;
        create_info.format = swap_chain_image_format_;
        create_info.components.r = vk::ComponentSwizzle::eIdentity;
        create_info.components.g = vk::ComponentSwizzle::eIdentity;
        create_info.components.b = vk::ComponentSwizzle::eIdentity;
        create_info.components.a = vk::ComponentSwizzle::eIdentity;
        create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        try {
            swap_chain_image_views_[i] = device_.createImageView(create_info);
        } catch (const std::exception& e) {
            LOGE("Failed to create image view {}: {}", i, e.what());
            return false;
        }
    }

    return true;
}

bool VulkanContext::createCommandPool() {
    QueueFamilyIndices queue_family_indices = findQueueFamilies(physical_device_);

    vk::CommandPoolCreateInfo pool_info{};
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();

    try {
        command_pool_ = device_.createCommandPool(pool_info);
        return true;
    } catch (const std::exception& e) {
        LOGE("Failed to create command pool: {}", e.what());
        return false;
    }
}

bool VulkanContext::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensions_supported = checkDeviceExtensionSupport(device);
    
    bool swap_chain_adequate = false;
    if (extensions_supported) {
        SwapChainSupportDetails swap_chain_support = querySwapChainSupport(device);
        swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
    }

    return indices.isComplete() && extensions_supported && swap_chain_adequate;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(vk::PhysicalDevice device) {
    QueueFamilyIndices indices;
    auto queue_families = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& queue_family : queue_families) {
        if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphics_family = i;
        }

        vk::Bool32 present_support = device.getSurfaceSupportKHR(i, surface_);
        if (present_support) {
            indices.present_family = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

bool VulkanContext::checkDeviceExtensionSupport(vk::PhysicalDevice device) {
    auto available_extensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> required_extensions(device_extensions_.begin(), device_extensions_.end());

    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(vk::PhysicalDevice device) {
    SwapChainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface_);
    details.formats = device.getSurfaceFormatsKHR(surface_);
    details.present_modes = device.getSurfacePresentModesKHR(surface_);
    return details;
}

vk::SurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats) {
    for (const auto& available_format : available_formats) {
        if (available_format.format == vk::Format::eB8G8R8A8Srgb && 
            available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return available_format;
        }
    }
    return available_formats[0];
}

vk::PresentModeKHR VulkanContext::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes) {
    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == vk::PresentModeKHR::eMailbox) {
            return available_present_mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D VulkanContext::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);

        vk::Extent2D actual_extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actual_extent;
    }
}

vk::CommandBuffer VulkanContext::beginSingleTimeCommands() {
    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandPool = command_pool_;
    alloc_info.commandBufferCount = 1;

    vk::CommandBuffer command_buffer = device_.allocateCommandBuffers(alloc_info)[0];

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    command_buffer.begin(begin_info);
    return command_buffer;
}

void VulkanContext::endSingleTimeCommands(vk::CommandBuffer command_buffer) {
    command_buffer.end();

    vk::SubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    graphics_queue_.submit(submit_info, nullptr);
    graphics_queue_.waitIdle();

    device_.freeCommandBuffers(command_pool_, command_buffer);
}

bool VulkanContext::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    device_.waitIdle();

    cleanupSwapChain();

    return createSwapChain() && createImageViews();
}

void VulkanContext::cleanupSwapChain() {
    for (auto image_view : swap_chain_image_views_) {
        device_.destroyImageView(image_view);
    }
    
    if (swap_chain_) {
        device_.destroySwapchainKHR(swap_chain_);
    }
}
