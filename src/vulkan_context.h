#pragma once

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    bool isComplete() {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
};

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool initialize(GLFWwindow* window);
    void cleanup();

    vk::Instance getInstance() const { return instance_; }
    vk::PhysicalDevice getPhysicalDevice() const { return physical_device_; }
    vk::Device getDevice() const { return device_; }
    vk::Queue getGraphicsQueue() const { return graphics_queue_; }
    vk::Queue getPresentQueue() const { return present_queue_; }
    vk::SurfaceKHR getSurface() const { return surface_; }
    vk::SwapchainKHR getSwapChain() const { return swap_chain_; }
    vk::Format getSwapChainImageFormat() const { return swap_chain_image_format_; }
    vk::Extent2D getSwapChainExtent() const { return swap_chain_extent_; }
    vk::CommandPool getCommandPool() const { return command_pool_; }

    const std::vector<vk::Image>& getSwapChainImages() const { return swap_chain_images_; }
    const std::vector<vk::ImageView>& getSwapChainImageViews() const { return swap_chain_image_views_; }

    vk::CommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(vk::CommandBuffer command_buffer);

    bool recreateSwapChain();

private:
    bool createInstance();
    bool setupDebugMessenger();
    bool createSurface();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapChain();
    bool createImageViews();
    bool createCommandPool();

    bool isDeviceSuitable(vk::PhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);
    bool checkDeviceExtensionSupport(vk::PhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

    void cleanupSwapChain();

    GLFWwindow* window_;
    vk::Instance instance_;
    vk::DebugUtilsMessengerEXT debug_messenger_;
    vk::SurfaceKHR surface_;
    vk::PhysicalDevice physical_device_;
    vk::Device device_;
    vk::Queue graphics_queue_;
    vk::Queue present_queue_;
    vk::SwapchainKHR swap_chain_;
    std::vector<vk::Image> swap_chain_images_;
    vk::Format swap_chain_image_format_;
    vk::Extent2D swap_chain_extent_;
    std::vector<vk::ImageView> swap_chain_image_views_;
    vk::CommandPool command_pool_;

    const std::vector<const char*> validation_layers_ = {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> device_extensions_ = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    #ifdef NDEBUG
        const bool enable_validation_layers_ = false;
    #else
        const bool enable_validation_layers_ = true;
    #endif
};
