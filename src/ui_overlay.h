#pragma once

#include "vulkan_context.h"
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <memory>
#include <string>

class UIOverlay {
public:
    UIOverlay(std::shared_ptr<VulkanContext> context, GLFWwindow* window);
    ~UIOverlay();

    bool initialize(vk::RenderPass render_pass);
    void cleanup();

    void updatePerformanceData(float fps, float frame_time);

    void render(vk::CommandBuffer command_buffer);

    void handleResize();

    void update(float delta_time);

    float getCurrentFPS() const { return fps_; }
    float getAverageFrameTime() const { return average_frame_time_; }

private:
    std::shared_ptr<VulkanContext> context_;
    GLFWwindow* window_;

    vk::DescriptorPool imgui_descriptor_pool_;

    float current_fps_;
    float current_frame_time_;

    std::string gpu_name_;

    static const int FRAME_TIME_HISTORY_SIZE = 100;
    float frame_time_history_[FRAME_TIME_HISTORY_SIZE];
    int frame_time_history_index_;

    float fps_;
    float average_frame_time_;
    int frame_count_;
    float time_accumulator_;

    bool createDescriptorPool();
    void collectGPUInfo();
    void renderPerformanceWindow();
    void setupImGuiStyle();
};
