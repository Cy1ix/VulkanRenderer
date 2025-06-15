#include "ui_overlay.h"
#include "utils/logger.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <algorithm>

UIOverlay::UIOverlay(std::shared_ptr<VulkanContext> context, GLFWwindow* window)
    : context_(context), window_(window),
    current_fps_(0.0f), current_frame_time_(0.0f), frame_time_history_index_(0),
    fps_(0.0f), average_frame_time_(0.0f), frame_count_(0), time_accumulator_(0.0f) {
    
    std::fill(frame_time_history_, frame_time_history_ + FRAME_TIME_HISTORY_SIZE, 0.0f);
}

UIOverlay::~UIOverlay() {
    cleanup();
}

bool UIOverlay::initialize(vk::RenderPass render_pass) {
    if (!createDescriptorPool()) {
        LOGE("Failed to create ImGui descriptor pool");
        return false;
    }
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    setupImGuiStyle();
    
    if (!ImGui_ImplGlfw_InitForVulkan(window_, true)) {
        LOGE("Failed to initialize ImGui GLFW implementation");
        return false;
    }
    
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = context_->getInstance();
    init_info.PhysicalDevice = context_->getPhysicalDevice();
    init_info.Device = context_->getDevice();
    init_info.QueueFamily = 0;
    init_info.Queue = context_->getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imgui_descriptor_pool_;
    init_info.RenderPass = render_pass;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = context_->getSwapChainImages().size();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        LOGE("Failed to initialize ImGui Vulkan implementation");
        return false;
    }
    
    vk::CommandBuffer command_buffer = context_->beginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture();
    context_->endSingleTimeCommands(command_buffer);
    
    collectGPUInfo();

    LOGI("UI Overlay initialized successfully");
    return true;
}

void UIOverlay::cleanup() {
    if (context_) {
        auto device = context_->getDevice();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (imgui_descriptor_pool_) {
            device.destroyDescriptorPool(imgui_descriptor_pool_);
        }
    }
}

void UIOverlay::updatePerformanceData(float fps, float frame_time) {
    current_fps_ = fps;
    current_frame_time_ = frame_time;
    
    frame_time_history_[frame_time_history_index_] = frame_time;
    frame_time_history_index_ = (frame_time_history_index_ + 1) % FRAME_TIME_HISTORY_SIZE;
}

void UIOverlay::render(vk::CommandBuffer command_buffer) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    renderPerformanceWindow();
    
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
}

void UIOverlay::handleResize() {
    ImGui_ImplVulkan_SetMinImageCount(context_->getSwapChainImages().size());
}

void UIOverlay::update(float delta_time) {
    frame_count_++;
    time_accumulator_ += delta_time;

    if (time_accumulator_ >= 1.0f) {
        fps_ = static_cast<float>(frame_count_) / time_accumulator_;
        average_frame_time_ = (time_accumulator_ / static_cast<float>(frame_count_)) * 1000.0f;

        frame_count_ = 0;
        time_accumulator_ = 0.0f;
    }
}

bool UIOverlay::createDescriptorPool() {
    auto device = context_->getDevice();

    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        { vk::DescriptorType::eSampler, 1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage, 1000 },
        { vk::DescriptorType::eStorageImage, 1000 },
        { vk::DescriptorType::eUniformTexelBuffer, 1000 },
        { vk::DescriptorType::eStorageTexelBuffer, 1000 },
        { vk::DescriptorType::eUniformBuffer, 1000 },
        { vk::DescriptorType::eStorageBuffer, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eStorageBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment, 1000 }
    };

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();

    try {
        imgui_descriptor_pool_ = device.createDescriptorPool(pool_info);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create ImGui descriptor pool: {}", e.what());
        return false;
    }
}

void UIOverlay::collectGPUInfo() {
    auto physical_device = context_->getPhysicalDevice();
    vk::PhysicalDeviceProperties props = physical_device.getProperties();

    gpu_name_ = props.deviceName.data();
}

void UIOverlay::renderPerformanceWindow() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;
    
    const float PAD = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 window_pos;
    window_pos.x = work_pos.x + PAD;
    window_pos.y = work_pos.y + PAD;

    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.8f);

    if (ImGui::Begin("Performance", nullptr, window_flags)) {
        ImGui::Text("GPU: %s", gpu_name_.c_str());
        ImGui::Separator();
        
        ImGui::Text("FPS: %.1f", current_fps_);
        ImGui::Text("Frame Time: %.2f ms", current_frame_time_);
        
        ImVec4 fps_color;
        if (current_fps_ >= 60.0f) {
            fps_color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        }
        else if (current_fps_ >= 30.0f) {
            fps_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        }
        else {
            fps_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        }

    	ImGui::Separator();
        ImGui::Text("Status");

        float min_time = *std::min_element(frame_time_history_, frame_time_history_ + FRAME_TIME_HISTORY_SIZE);
        float max_time = *std::max_element(frame_time_history_, frame_time_history_ + FRAME_TIME_HISTORY_SIZE);

        if (max_time - min_time < 1.0f) {
            max_time = min_time + 1.0f;
        }

        ImGui::PlotLines("Frame Time (ms)", frame_time_history_, FRAME_TIME_HISTORY_SIZE,
            frame_time_history_index_, nullptr, min_time, max_time,
            ImVec2(250, 80));
    }
    ImGui::End();
}

void UIOverlay::setupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    
    ImGui::StyleColorsDark();
    
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.9f);
    colors[ImGuiCol_Text] = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
}
