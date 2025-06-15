#include "vulkan_renderer.h"
#include "model_loader.h"
#include "utils/logger.h"
#include <algorithm>
#include <array>

VulkanRenderer::VulkanRenderer()
    : window_(nullptr), window_width_(800), window_height_(600),
    window_title_("Vulkan Renderer"), framebuffer_resized_(false),
    current_frame_(0), delta_time_(0.0f), first_mouse_(true),
    last_x_(400.0), last_y_(300.0) {
    last_time_ = std::chrono::steady_clock::now();
}

VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

bool VulkanRenderer::initialize(int width, int height, const std::string& title) {
    window_width_ = width;
    window_height_ = height;
    window_title_ = title;

    if (!initWindow()) {
        LOGE("Failed to initialize window");
        return false;
    }

    if (!initVulkan()) {
        LOGE("Failed to initialize Vulkan");
        return false;
    }

    camera_ = std::make_unique<Camera>(glm::vec3(2.0f, 1.5f, 4.0f), glm::vec3(0.0f, 1.0f, 0.0f), -105.0f, -15.0f);

    LOGI("Vulkan Renderer initialized successfully");
    return true;
}

void VulkanRenderer::run() {
    mainLoop();
}

void VulkanRenderer::cleanup() {
    if (context_) {
        ui_overlay_.reset();

        auto device = context_->getDevice();
        device.waitIdle();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (render_finished_semaphores_[i]) device.destroySemaphore(render_finished_semaphores_[i]);
            if (image_available_semaphores_[i]) device.destroySemaphore(image_available_semaphores_[i]);
            if (in_flight_fences_[i]) device.destroyFence(in_flight_fences_[i]);
        }

        if (depth_image_view_) device.destroyImageView(depth_image_view_);
        if (depth_image_) device.destroyImage(depth_image_);
        if (depth_image_memory_) device.freeMemory(depth_image_memory_);

        for (auto framebuffer : swap_chain_framebuffers_) {
            device.destroyFramebuffer(framebuffer);
        }

        if (graphics_pipeline_) device.destroyPipeline(graphics_pipeline_);
        if (pipeline_layout_) device.destroyPipelineLayout(pipeline_layout_);

        if (skybox_pipeline_) device.destroyPipeline(skybox_pipeline_);
        if (skybox_pipeline_layout_) device.destroyPipelineLayout(skybox_pipeline_layout_);

        if (render_pass_) device.destroyRenderPass(render_pass_);
    }

    mesh_.reset();
    shader_.reset();
    material_.reset();
    skybox_.reset();
    skybox_shader_.reset();
    context_.reset();

    if (window_) {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

bool VulkanRenderer::loadModel(const std::string& obj_path) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    if (!ModelLoader::loadOBJ(obj_path, vertices, indices)) {
        LOGE("Failed to load model: {}", obj_path);
        return false;
    }

    mesh_ = std::make_unique<Mesh>(context_);
    if (!mesh_->create(vertices, indices)) {
        LOGE("Failed to create mesh");
        return false;
    }

    LOGI("Model loaded successfully: {}", obj_path);
    return true;
}

bool VulkanRenderer::loadTexture(const std::string& texture_path) {
    if (!material_) {
        LOGE("Material not initialized, cannot load texture");
        return false;
    }
    
    auto texture = std::make_shared<Texture>(context_);

    if (!texture->loadFromFile(texture_path)) {
        LOGW("Failed to load texture from: {}, try to use default texture", texture_path);
        if (!texture->loadFromFile(MODEL_DIR "default_texture.png")) {
            LOGE("Cannot load default texture");
            return false;
        }
        LOGI("Using default texture");
    }
    
    if (!material_->setTexture(texture)) {
        LOGE("Failed to set texture to material");
        return false;
    }
    
    return true;
}

bool VulkanRenderer::createDefaultSkyBox() {
    LOGI("Creating default skybox");
    
    if (!skybox_) {
        skybox_ = std::make_unique<SkyBox>(context_);
        if (!skybox_->initialize()) {
            LOGE("Failed to initialize skybox");
            return false;
        }
    }
    
    if (!skybox_->createDefaultSkyBox()) {
        LOGE("Failed to create default skybox");
        return false;
    }
    
    skybox_shader_ = std::make_unique<Shader>(context_);
    if (!skybox_shader_->loadFromSource(SkyBox::getVertexShaderSource(),
        SkyBox::getFragmentShaderSource())) {
        LOGE("Failed to load skybox shaders");
        return false;
    }

    if (!createSkyBoxPipeline()) {
        LOGE("Failed to create skybox pipeline");
        return false;
    }

    LOGI("Default skybox created successfully");
    return true;
}

bool VulkanRenderer::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window_ = glfwCreateWindow(window_width_, window_height_, window_title_.c_str(), nullptr, nullptr);
    if (!window_) {
        LOGE("Failed to create GLFW window");
        return false;
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetCursorPosCallback(window_, mouseCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    return true;
}

bool VulkanRenderer::initVulkan() {
    context_ = std::make_shared<VulkanContext>();
    if (!context_->initialize(window_)) {
        return false;
    }

    if (!createRenderPass()) {
        return false;
    }

    shader_ = std::make_unique<Shader>(context_);
    if (!shader_->loadFromSource(shader_->getDefaultVertexShader(), shader_->getDefaultFragmentShader())) {
        LOGE("Failed to load default shaders");
        return false;
    }

    material_ = std::make_unique<Material>(context_);
    if (!material_->initialize()) {
        return false;
    }

    if (!createGraphicsPipeline()) {
        return false;
    }

    if (!createDepthResources()) {
        return false;
    }

    if (!createFramebuffers()) {
        return false;
    }

    if (!createCommandBuffers()) {
        return false;
    }

    if (!createSyncObjects()) {
        return false;
    }
    
    ui_overlay_ = std::make_unique<UIOverlay>(context_, window_);
    if (!ui_overlay_->initialize(render_pass_)) {
        LOGE("Failed to initialize UI overlay");
        return false;
    }

    return true;
}

bool VulkanRenderer::createRenderPass() {
    auto device = context_->getDevice();

    vk::AttachmentDescription color_attachment{};
    color_attachment.format = context_->getSwapChainImageFormat();
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentDescription depth_attachment{};
    depth_attachment.format = vk::Format::eD32Sfloat;
    depth_attachment.samples = vk::SampleCountFlagBits::e1;
    depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
    depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.srcAccessMask = vk::AccessFlagBits::eNone;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    std::array<vk::AttachmentDescription, 2> attachments = { color_attachment, depth_attachment };
    vk::RenderPassCreateInfo render_pass_info{};
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    try {
        render_pass_ = device.createRenderPass(render_pass_info);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create render pass: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::createGraphicsPipeline() {
    auto device = context_->getDevice();

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vert_shader_stage_info.module = shader_->getVertexShader();
    vert_shader_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    frag_shader_stage_info.module = shader_->getFragmentShader();
    frag_shader_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

    auto binding_description = Vertex::getBindingDescription();
    auto attribute_descriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(context_->getSwapChainExtent().width);
    viewport.height = static_cast<float>(context_->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{ 0, 0 };
    scissor.extent = context_->getSwapChainExtent();

    vk::PipelineViewportStateCreateInfo viewport_state{};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = vk::CompareOp::eLess;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    vk::PipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    color_blend_attachment.blendEnable = VK_FALSE;

    vk::PipelineColorBlendStateCreateInfo color_blending{};
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = vk::LogicOp::eCopy;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    vk::PipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.setLayoutCount = 1;
    auto descriptor_set_layout = material_->getDescriptorSetLayout();
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

    try {
        pipeline_layout_ = device.createPipelineLayout(pipeline_layout_info);
    }
    catch (const std::exception& e) {
        LOGE("Failed to create pipeline layout: {}", e.what());
        return false;
    }

    std::vector<vk::DynamicState> dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.data();

    vk::GraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline_layout_;
    pipeline_info.renderPass = render_pass_;
    pipeline_info.subpass = 0;

    try {
        auto result = device.createGraphicsPipeline(nullptr, pipeline_info);
        graphics_pipeline_ = result.value;
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create graphics pipeline: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::createSkyBoxPipeline() {
    auto device = context_->getDevice();

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vert_shader_stage_info.module = skybox_shader_->getVertexShader();
    vert_shader_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    frag_shader_stage_info.module = skybox_shader_->getFragmentShader();
    frag_shader_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo shader_stages[] = { vert_shader_stage_info, frag_shader_stage_info };

    auto binding_description = SkyBox::SkyBoxVertex::getBindingDescription();
    auto attribute_descriptions = SkyBox::SkyBoxVertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(context_->getSwapChainExtent().width);
    viewport.height = static_cast<float>(context_->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{ 0, 0 };
    scissor.extent = context_->getSwapChainExtent();

    vk::PipelineViewportStateCreateInfo viewport_state{};
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_FALSE;
    depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    vk::PipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
    color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo color_blending{};
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = vk::LogicOp::eCopy;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    vk::PipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.setLayoutCount = 1;
    auto skybox_descriptor_set_layout = skybox_->getDescriptorSetLayout();
    pipeline_layout_info.pSetLayouts = &skybox_descriptor_set_layout;

    try {
        skybox_pipeline_layout_ = device.createPipelineLayout(pipeline_layout_info);
    }
    catch (const std::exception& e) {
        LOGE("Failed to create skybox pipeline layout: {}", e.what());
        return false;
    }

    vk::GraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = skybox_pipeline_layout_;
    pipeline_info.renderPass = render_pass_;
    pipeline_info.subpass = 0;

    try {
        auto result = device.createGraphicsPipeline(nullptr, pipeline_info);
        skybox_pipeline_ = result.value;
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create skybox graphics pipeline: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::createFramebuffers() {
    auto device = context_->getDevice();
    const auto& swap_chain_image_views = context_->getSwapChainImageViews();

    swap_chain_framebuffers_.resize(swap_chain_image_views.size());

    for (size_t i = 0; i < swap_chain_image_views.size(); i++) {
        std::array<vk::ImageView, 2> attachments = {
            swap_chain_image_views[i],
            depth_image_view_
        };

        vk::FramebufferCreateInfo framebuffer_info{};
        framebuffer_info.renderPass = render_pass_;
        framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = context_->getSwapChainExtent().width;
        framebuffer_info.height = context_->getSwapChainExtent().height;
        framebuffer_info.layers = 1;

        try {
            swap_chain_framebuffers_[i] = device.createFramebuffer(framebuffer_info);
        }
        catch (const std::exception& e) {
            LOGE("Failed to create framebuffer {}: {}", i, e.what());
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::createCommandBuffers() {
    auto device = context_->getDevice();

    command_buffers_.resize(MAX_FRAMES_IN_FLIGHT);

    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = context_->getCommandPool();
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers_.size());

    try {
        command_buffers_ = device.allocateCommandBuffers(alloc_info);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to allocate command buffers: {}", e.what());
        return false;
    }
}

bool VulkanRenderer::createSyncObjects() {
    auto device = context_->getDevice();

    image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences_.resize(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphore_info{};
    vk::FenceCreateInfo fence_info{};
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        try {
            image_available_semaphores_[i] = device.createSemaphore(semaphore_info);
            render_finished_semaphores_[i] = device.createSemaphore(semaphore_info);
            in_flight_fences_[i] = device.createFence(fence_info);
        }
        catch (const std::exception& e) {
            LOGE("Failed to create synchronization objects for frame {}: {}", i, e.what());
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::createDepthResources() {
    auto device = context_->getDevice();
    vk::Format depth_format = vk::Format::eD32Sfloat;

    vk::ImageCreateInfo image_info{};
    image_info.imageType = vk::ImageType::e2D;
    image_info.extent.width = context_->getSwapChainExtent().width;
    image_info.extent.height = context_->getSwapChainExtent().height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = depth_format;
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.initialLayout = vk::ImageLayout::eUndefined;
    image_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.sharingMode = vk::SharingMode::eExclusive;

    try {
        depth_image_ = device.createImage(image_info);
    }
    catch (const std::exception& e) {
        LOGE("Failed to create depth image: {}", e.what());
        return false;
    }

    vk::MemoryRequirements mem_requirements = device.getImageMemoryRequirements(depth_image_);

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    try {
        depth_image_memory_ = device.allocateMemory(alloc_info);
        device.bindImageMemory(depth_image_, depth_image_memory_, 0);
    }
    catch (const std::exception& e) {
        LOGE("Failed to allocate depth image memory: {}", e.what());
        return false;
    }

    vk::ImageViewCreateInfo view_info{};
    view_info.image = depth_image_;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = depth_format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    try {
        depth_image_view_ = device.createImageView(view_info);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create depth image view: {}", e.what());
        return false;
    }
}

void VulkanRenderer::mainLoop() {
    last_time_ = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        auto current_time = std::chrono::steady_clock::now();
        delta_time_ = std::chrono::duration<float>(current_time - last_time_).count();
        last_time_ = current_time;

        if (ui_overlay_) {
            ui_overlay_->update(delta_time_);
        	ui_overlay_->updatePerformanceData(ui_overlay_->getCurrentFPS(), ui_overlay_->getAverageFrameTime());
        }

        processInput();
        drawFrame();
    }

    context_->getDevice().waitIdle();
}

void VulkanRenderer::drawFrame() {
    auto device = context_->getDevice();

    device.waitForFences(1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

    uint32_t image_index;
    vk::Result result = device.acquireNextImageKHR(context_->getSwapChain(), UINT64_MAX,
        image_available_semaphores_[current_frame_], nullptr, &image_index);

    if (result == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapChain();
        return;
    }
    else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    updateUniformBuffer();
    updateSkyBoxUniforms();

    device.resetFences(1, &in_flight_fences_[current_frame_]);

    command_buffers_[current_frame_].reset();

    vk::CommandBufferBeginInfo begin_info{};
    command_buffers_[current_frame_].begin(begin_info);

    vk::RenderPassBeginInfo render_pass_info{};
    render_pass_info.renderPass = render_pass_;
    render_pass_info.framebuffer = swap_chain_framebuffers_[image_index];
    render_pass_info.renderArea.offset = vk::Offset2D{ 0, 0 };
    render_pass_info.renderArea.extent = context_->getSwapChainExtent();

    std::array<vk::ClearValue, 2> clear_values{};
    clear_values[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
    clear_values[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

    render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    command_buffers_[current_frame_].beginRenderPass(render_pass_info, vk::SubpassContents::eInline);

    vk::Viewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(context_->getSwapChainExtent().width);
    viewport.height = static_cast<float>(context_->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    command_buffers_[current_frame_].setViewport(0, 1, &viewport);

    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{ 0, 0 };
    scissor.extent = context_->getSwapChainExtent();
    command_buffers_[current_frame_].setScissor(0, 1, &scissor);

    if (skybox_ && skybox_pipeline_) {
        command_buffers_[current_frame_].bindPipeline(vk::PipelineBindPoint::eGraphics, skybox_pipeline_);
        skybox_->draw(command_buffers_[current_frame_], skybox_pipeline_layout_);
    }

    command_buffers_[current_frame_].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);
    material_->bind(command_buffers_[current_frame_], pipeline_layout_);

    if (mesh_) {
        mesh_->draw(command_buffers_[current_frame_]);
    }

    if (ui_overlay_) {
        ui_overlay_->render(command_buffers_[current_frame_]);
    }

    command_buffers_[current_frame_].endRenderPass();
    command_buffers_[current_frame_].end();

    vk::SubmitInfo submit_info{};

    vk::Semaphore wait_semaphores[] = { image_available_semaphores_[current_frame_] };
    vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers_[current_frame_];

    vk::Semaphore signal_semaphores[] = { render_finished_semaphores_[current_frame_] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    context_->getGraphicsQueue().submit(submit_info, in_flight_fences_[current_frame_]);

    vk::PresentInfoKHR present_info{};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    vk::SwapchainKHR swap_chains[] = { context_->getSwapChain() };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;

    result = context_->getPresentQueue().presentKHR(present_info);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebuffer_resized_) {
        framebuffer_resized_ = false;
        recreateSwapChain();
    }
    else if (result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::updateUniformBuffer() {
    static auto start_time = std::chrono::high_resolution_clock::now();
    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = camera_->getViewMatrix();
    ubo.proj = camera_->getProjectionMatrix(static_cast<float>(context_->getSwapChainExtent().width) /
        static_cast<float>(context_->getSwapChainExtent().height));
    ubo.proj[1][1] *= -1;
    ubo.normal_matrix = glm::transpose(glm::inverse(ubo.model));
    ubo.camera_pos = camera_->getPosition();

    material_->updateUniforms(ubo);
}

void VulkanRenderer::updateSkyBoxUniforms() {
    if (!skybox_) return;

    SkyBoxUBO ubo{};
    ubo.view = camera_->getViewMatrix();
    ubo.projection = camera_->getProjectionMatrix(static_cast<float>(context_->getSwapChainExtent().width) /
        static_cast<float>(context_->getSwapChainExtent().height));
    ubo.projection[1][1] *= -1;

    skybox_->updateUniforms(ubo);
}

void VulkanRenderer::recreateSwapChain() {
    auto device = context_->getDevice();
    device.waitIdle();

    for (auto framebuffer : swap_chain_framebuffers_) {
        device.destroyFramebuffer(framebuffer);
    }

    device.destroyImageView(depth_image_view_);
    device.destroyImage(depth_image_);
    device.freeMemory(depth_image_memory_);

    if (graphics_pipeline_) device.destroyPipeline(graphics_pipeline_);
    if (skybox_pipeline_) device.destroyPipeline(skybox_pipeline_);

    context_->recreateSwapChain();

    createDepthResources();
    createFramebuffers();
    createGraphicsPipeline();

    if (skybox_) {
        createSkyBoxPipeline();
    }
}

void VulkanRenderer::processInput() {
    bool move_forward = glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS;
    bool move_backward = glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS;
    bool move_left = glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS;
    bool move_right = glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS;

    camera_->processInput(delta_time_, move_forward, move_backward, move_left, move_right);

    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, true);
    }
}

uint32_t VulkanRenderer::findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties mem_properties = context_->getPhysicalDevice().getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void VulkanRenderer::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto renderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    renderer->framebuffer_resized_ = true;
}

void VulkanRenderer::mouseCallback(GLFWwindow* window, double x_pos, double y_pos) {
    auto renderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));

    if (renderer->first_mouse_) {
        renderer->last_x_ = x_pos;
        renderer->last_y_ = y_pos;
        renderer->first_mouse_ = false;
    }

    float x_offset = x_pos - renderer->last_x_;
    float y_offset = renderer->last_y_ - y_pos;

    renderer->last_x_ = x_pos;
    renderer->last_y_ = y_pos;

    renderer->camera_->processMouseMovement(x_offset, y_offset);
}

void VulkanRenderer::scrollCallback(GLFWwindow* window, double x_offset, double y_offset) {
    auto renderer = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
    renderer->camera_->processMouseScroll(static_cast<float>(y_offset));
}
