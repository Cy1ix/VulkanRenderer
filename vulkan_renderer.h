#pragma once

#include "vulkan_context.h"
#include "camera.h"
#include "mesh.h"
#include "shader.h"
#include "texture.h"
#include "material.h"
#include "skybox.h"
#include "utils/ui_overlay.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <vector>
#include <chrono>

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    bool initialize(int width = 800, int height = 600, const std::string& title = "Vulkan Renderer");
    void run();
    void cleanup();

    bool loadModel(const std::string& obj_path);

    bool createDefaultSkyBox();

    bool loadTexture(const std::string& texture_path);

private:
    bool initWindow();
    bool initVulkan();
    bool createRenderPass();
    bool createGraphicsPipeline();
    bool createSkyBoxPipeline();
    bool createFramebuffers();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createDepthResources();

    void mainLoop();
    void drawFrame();
    void updateUniformBuffer();
    void updateSkyBoxUniforms();
    void recreateSwapChain();

    uint32_t findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);
    
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void mouseCallback(GLFWwindow* window, double x_pos, double y_pos);
    static void scrollCallback(GLFWwindow* window, double x_offset, double y_offset);
    void processInput();
    
    GLFWwindow* window_;
    int window_width_;
    int window_height_;
    std::string window_title_;
    bool framebuffer_resized_;
    
    std::shared_ptr<VulkanContext> context_;
    
    vk::RenderPass render_pass_;
    vk::DescriptorSetLayout descriptor_set_layout_;
    vk::PipelineLayout pipeline_layout_;
    vk::Pipeline graphics_pipeline_;
    
    vk::PipelineLayout skybox_pipeline_layout_;
    vk::Pipeline skybox_pipeline_;

    std::vector<vk::Framebuffer> swap_chain_framebuffers_;
    std::vector<vk::CommandBuffer> command_buffers_;
    
    vk::Image depth_image_;
    vk::DeviceMemory depth_image_memory_;
    vk::ImageView depth_image_view_;
    
    std::vector<vk::Semaphore> image_available_semaphores_;
    std::vector<vk::Semaphore> render_finished_semaphores_;
    std::vector<vk::Fence> in_flight_fences_;
    uint32_t current_frame_;
    
    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Mesh> mesh_;
    std::unique_ptr<Shader> shader_;
    std::unique_ptr<Material> material_;
    
    std::unique_ptr<SkyBox> skybox_;
    std::unique_ptr<Shader> skybox_shader_;

    std::unique_ptr<UIOverlay> ui_overlay_;
    
    std::chrono::steady_clock::time_point last_time_;
    float delta_time_;
    
    bool first_mouse_;
    double last_x_, last_y_;

    static const int MAX_FRAMES_IN_FLIGHT = 2;
};