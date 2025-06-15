#include "skybox.h"
#include "vulkan_context.h"
#include "utils/logger.h"
#include <array>
#include <cmath>

#include "shader.h"

SkyBox::SkyBox(std::shared_ptr<VulkanContext> context)
    : context_(context), vertex_count_(0),
    top_color_(0.5f, 0.7f, 1.0f), bottom_color_(0.9f, 0.9f, 0.8f) {
}

SkyBox::~SkyBox() {
    cleanup();
}

bool SkyBox::initialize() {
    if (!createVertexBuffer()) {
        LOGE("Failed to create skybox vertex buffer");
        return false;
    }

    if (!createDescriptorSetLayout()) {
        LOGE("Failed to create skybox descriptor set layout");
        return false;
    }

    if (!createUniformBuffer()) {
        LOGE("Failed to create skybox uniform buffer");
        return false;
    }

    if (!createDescriptorPool()) {
        LOGE("Failed to create skybox descriptor pool");
        return false;
    }

    LOGI("SkyBox initialized successfully");
    return true;
}

void SkyBox::cleanup() {
    auto device = context_->getDevice();

    if (cubemap_sampler_) device.destroySampler(cubemap_sampler_);
    if (cubemap_image_view_) device.destroyImageView(cubemap_image_view_);
    if (cubemap_image_) {
        device.destroyImage(cubemap_image_);
        device.freeMemory(cubemap_image_memory_);
    }

    if (uniform_buffer_) {
        device.destroyBuffer(uniform_buffer_);
        device.freeMemory(uniform_buffer_memory_);
    }

    if (vertex_buffer_) {
        device.destroyBuffer(vertex_buffer_);
        device.freeMemory(vertex_buffer_memory_);
    }

    if (descriptor_pool_) device.destroyDescriptorPool(descriptor_pool_);
    if (descriptor_set_layout_) device.destroyDescriptorSetLayout(descriptor_set_layout_);
}

bool SkyBox::createDefaultSkyBox(const glm::vec3& top_color, const glm::vec3& bottom_color, int resolution) {
    LOGI("Creating default gradient skybox with resolution: {}", resolution);
    
    top_color_ = top_color;
    bottom_color_ = bottom_color;

    if (!createCubemapTexture(resolution, resolution)) {
        LOGE("Failed to create cubemap from generated data");
        return false;
    }

    if (!createDescriptorSets()) {
        LOGE("Failed to create skybox descriptor sets");
        return false;
    }

    LOGI("Default gradient skybox created successfully");
    return true;
}

std::vector<unsigned char> SkyBox::generateFaceTexture(int face, int resolution,
    const glm::vec3& top_color,
    const glm::vec3& bottom_color) {
    std::vector<unsigned char> image_data(resolution * resolution * 4);

    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            float u = (x + 0.5f) / resolution;
            float v = (y + 0.5f) / resolution;

            glm::vec3 direction = uvToDirection(u, v, face);
            
            float t = (direction.y + 1.0f) * 0.5f;
            t = std::clamp(t, 0.0f, 1.0f);

            glm::vec3 color = interpolateColor(bottom_color, top_color, t);

            int index = (y * resolution + x) * 4;
            image_data[index] = static_cast<unsigned char>(color.r * 255);
            image_data[index + 1] = static_cast<unsigned char>(color.g * 255);
            image_data[index + 2] = static_cast<unsigned char>(color.b * 255);
            image_data[index + 3] = 255;
        }
    }

    return image_data;
}

glm::vec3 SkyBox::uvToDirection(float u, float v, int face) {
    float x = u * 2.0f - 1.0f;
    float y = v * 2.0f - 1.0f;

    glm::vec3 direction;

    switch (face) {
    case 0:
        direction = glm::vec3(1.0f, -y, -x);
        break;
    case 1:
        direction = glm::vec3(-1.0f, -y, x);
        break;
    case 2:
        direction = glm::vec3(x, 1.0f, y);
        break;
    case 3:
        direction = glm::vec3(x, -1.0f, -y);
        break;
    case 4:
        direction = glm::vec3(x, -y, 1.0f);
        break;
    case 5:
        direction = glm::vec3(-x, -y, -1.0f);
        break;
    default:
        direction = glm::vec3(0.0f, 1.0f, 0.0f);
        break;
    }

    return glm::normalize(direction);
}

glm::vec3 SkyBox::interpolateColor(const glm::vec3& color1, const glm::vec3& color2, float t) {
    return color1 * (1.0f - t) + color2 * t;
}

bool SkyBox::createCubemapTexture(int width, int height) {
    auto device = context_->getDevice();
    
    vk::ImageCreateInfo image_info{};
    image_info.imageType = vk::ImageType::e2D;
    image_info.extent.width = static_cast<uint32_t>(width);
    image_info.extent.height = static_cast<uint32_t>(height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 6;
    image_info.format = vk::Format::eR8G8B8A8Srgb;
    image_info.tiling = vk::ImageTiling::eOptimal;
    image_info.initialLayout = vk::ImageLayout::eUndefined;
    image_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.sharingMode = vk::SharingMode::eExclusive;
    image_info.flags = vk::ImageCreateFlagBits::eCubeCompatible;

    try {
        cubemap_image_ = device.createImage(image_info);
    }
    catch (const std::exception& e) {
        LOGE("Failed to create cubemap image: {}", e.what());
        return false;
    }
    
    vk::MemoryRequirements mem_requirements = device.getImageMemoryRequirements(cubemap_image_);
    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_requirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    try {
        cubemap_image_memory_ = device.allocateMemory(alloc_info);
        device.bindImageMemory(cubemap_image_, cubemap_image_memory_, 0);
    }
    catch (const std::exception& e) {
        LOGE("Failed to allocate cubemap image memory: {}", e.what());
        return false;
    }
    
    transitionImageLayout(cubemap_image_, vk::Format::eR8G8B8A8Srgb,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, 6);
    
    vk::DeviceSize image_size = width * height * 4;

    for (uint32_t i = 0; i < 6; i++) {
        auto face_data = generateFaceTexture(i, width, top_color_, bottom_color_);

        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_buffer_memory;

        if (!createBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            staging_buffer, staging_buffer_memory)) {
            return false;
        }

        void* data = device.mapMemory(staging_buffer_memory, 0, image_size);
        memcpy(data, face_data.data(), static_cast<size_t>(image_size));
        device.unmapMemory(staging_buffer_memory);

        copyBufferToImage(staging_buffer, cubemap_image_, static_cast<uint32_t>(width),
            static_cast<uint32_t>(height), i);

        device.destroyBuffer(staging_buffer);
        device.freeMemory(staging_buffer_memory);
    }
    
    transitionImageLayout(cubemap_image_, vk::Format::eR8G8B8A8Srgb,
        vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, 6);
    
    vk::ImageViewCreateInfo view_info{};
    view_info.image = cubemap_image_;
    view_info.viewType = vk::ImageViewType::eCube;
    view_info.format = vk::Format::eR8G8B8A8Srgb;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 6;

    try {
        cubemap_image_view_ = device.createImageView(view_info);
    }
    catch (const std::exception& e) {
        LOGE("Failed to create cubemap image view: {}", e.what());
        return false;
    }
    
    vk::SamplerCreateInfo sampler_info{};
    sampler_info.magFilter = vk::Filter::eLinear;
    sampler_info.minFilter = vk::Filter::eLinear;
    sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16.0f;
    sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = vk::CompareOp::eAlways;
    sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;

    try {
        cubemap_sampler_ = device.createSampler(sampler_info);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create cubemap sampler: {}", e.what());
        return false;
    }
}

bool SkyBox::createVertexBuffer() {
    std::array<SkyBoxVertex, 36> vertices = { {
            {{-1.0f, -1.0f,  1.0f}},
            {{ 1.0f, -1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{-1.0f,  1.0f,  1.0f}},
            {{-1.0f, -1.0f,  1.0f}},
            
            {{-1.0f, -1.0f, -1.0f}},
            {{-1.0f,  1.0f, -1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
            
            {{-1.0f,  1.0f,  1.0f}},
            {{-1.0f,  1.0f, -1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
            {{-1.0f, -1.0f,  1.0f}},
            {{-1.0f,  1.0f,  1.0f}},
            
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f, -1.0f,  1.0f}},
            
            {{-1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f,  1.0f}},
            {{ 1.0f, -1.0f,  1.0f}},
            {{-1.0f, -1.0f,  1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
            
            {{-1.0f,  1.0f, -1.0f}},
            {{-1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{-1.0f,  1.0f, -1.0f}}
        } };

    vertex_count_ = vertices.size();
    vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

    vk::Buffer staging_buffer;
    vk::DeviceMemory staging_buffer_memory;

    if (!createBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        staging_buffer, staging_buffer_memory)) {
        return false;
    }

    auto device = context_->getDevice();
    void* data = device.mapMemory(staging_buffer_memory, 0, buffer_size);
    memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));
    device.unmapMemory(staging_buffer_memory);

    if (!createBuffer(buffer_size,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal,
        vertex_buffer_, vertex_buffer_memory_)) {
        device.destroyBuffer(staging_buffer);
        device.freeMemory(staging_buffer_memory);
        return false;
    }
    
    vk::CommandBuffer command_buffer = context_->beginSingleTimeCommands();
    vk::BufferCopy copy_region{};
    copy_region.size = buffer_size;
    command_buffer.copyBuffer(staging_buffer, vertex_buffer_, 1, &copy_region);
    context_->endSingleTimeCommands(command_buffer);

    device.destroyBuffer(staging_buffer);
    device.freeMemory(staging_buffer_memory);

    return true;
}

bool SkyBox::createDescriptorSetLayout() {
    auto device = context_->getDevice();

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings{};
    
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = vk::ShaderStageFlagBits::eVertex;
    
    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    bindings[1].pImmutableSamplers = nullptr;
    bindings[1].stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo layout_info{};
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();

    try {
        descriptor_set_layout_ = device.createDescriptorSetLayout(layout_info);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create skybox descriptor set layout: {}", e.what());
        return false;
    }
}

bool SkyBox::createDescriptorPool() {
    auto device = context_->getDevice();

    std::array<vk::DescriptorPoolSize, 2> pool_sizes{};
    pool_sizes[0].type = vk::DescriptorType::eUniformBuffer;
    pool_sizes[0].descriptorCount = 1;
    pool_sizes[1].type = vk::DescriptorType::eCombinedImageSampler;
    pool_sizes[1].descriptorCount = 1;

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = 1;

    try {
        descriptor_pool_ = device.createDescriptorPool(pool_info);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create skybox descriptor pool: {}", e.what());
        return false;
    }
}

bool SkyBox::createDescriptorSets() {
    auto device = context_->getDevice();

    vk::DescriptorSetAllocateInfo alloc_info{};
    alloc_info.descriptorPool = descriptor_pool_;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout_;

    try {
        auto descriptor_sets = device.allocateDescriptorSets(alloc_info);
        descriptor_set_ = descriptor_sets[0];
    }
    catch (const std::exception& e) {
        LOGE("Failed to allocate skybox descriptor sets: {}", e.what());
        return false;
    }

    std::array<vk::WriteDescriptorSet, 2> descriptor_writes{};
    
    vk::DescriptorBufferInfo buffer_info{};
    buffer_info.buffer = uniform_buffer_;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(SkyBoxUBO);

    descriptor_writes[0].dstSet = descriptor_set_;
    descriptor_writes[0].dstBinding = 0;
    descriptor_writes[0].dstArrayElement = 0;
    descriptor_writes[0].descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptor_writes[0].descriptorCount = 1;
    descriptor_writes[0].pBufferInfo = &buffer_info;
    
    vk::DescriptorImageInfo image_info{};
    image_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    image_info.imageView = cubemap_image_view_;
    image_info.sampler = cubemap_sampler_;

    descriptor_writes[1].dstSet = descriptor_set_;
    descriptor_writes[1].dstBinding = 1;
    descriptor_writes[1].dstArrayElement = 0;
    descriptor_writes[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    descriptor_writes[1].descriptorCount = 1;
    descriptor_writes[1].pImageInfo = &image_info;

    device.updateDescriptorSets(static_cast<uint32_t>(descriptor_writes.size()),
        descriptor_writes.data(), 0, nullptr);

    return true;
}

bool SkyBox::createUniformBuffer() {
    vk::DeviceSize buffer_size = sizeof(SkyBoxUBO);

    return createBuffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        uniform_buffer_, uniform_buffer_memory_);
}

void SkyBox::updateUniforms(const SkyBoxUBO& ubo) {
    auto device = context_->getDevice();
    void* data = device.mapMemory(uniform_buffer_memory_, 0, sizeof(SkyBoxUBO));
    memcpy(data, &ubo, sizeof(SkyBoxUBO));
    device.unmapMemory(uniform_buffer_memory_);
}

void SkyBox::draw(vk::CommandBuffer command_buffer, vk::PipelineLayout pipeline_layout) {
    vk::Buffer vertex_buffers[] = { vertex_buffer_ };
    vk::DeviceSize offsets[] = { 0 };

    command_buffer.bindVertexBuffers(0, 1, vertex_buffers, offsets);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
        pipeline_layout, 0, 1, &descriptor_set_, 0, nullptr);
    command_buffer.draw(vertex_count_, 1, 0, 0);
}

bool SkyBox::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    vk::Buffer& buffer, vk::DeviceMemory& buffer_memory) {
    auto device = context_->getDevice();

    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    try {
        buffer = device.createBuffer(buffer_info);
    }
    catch (const std::exception& e) {
        LOGE("Failed to create buffer: {}", e.what());
        return false;
    }

    vk::MemoryRequirements mem_requirements = device.getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_requirements.memoryTypeBits, properties);

    try {
        buffer_memory = device.allocateMemory(alloc_info);
        device.bindBufferMemory(buffer, buffer_memory, 0);
    }
    catch (const std::exception& e) {
        LOGE("Failed to allocate buffer memory: {}", e.what());
        device.destroyBuffer(buffer);
        return false;
    }

    return true;
}

uint32_t SkyBox::findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties mem_properties = context_->getPhysicalDevice().getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void SkyBox::transitionImageLayout(vk::Image image, vk::Format format,
    vk::ImageLayout old_layout, vk::ImageLayout new_layout,
    uint32_t layer_count) {
    vk::CommandBuffer command_buffer = context_->beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layer_count;

    vk::PipelineStageFlags source_stage;
    vk::PipelineStageFlags destination_stage;

    if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    command_buffer.pipelineBarrier(source_stage, destination_stage, vk::DependencyFlags{},
        0, nullptr, 0, nullptr, 1, &barrier);

    context_->endSingleTimeCommands(command_buffer);
}

void SkyBox::copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height,
    uint32_t layer_index) {
    vk::CommandBuffer command_buffer = context_->beginSingleTimeCommands();

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = layer_index;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{ 0, 0, 0 };
    region.imageExtent = vk::Extent3D{ width, height, 1 };

    command_buffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

    context_->endSingleTimeCommands(command_buffer);
}

std::string SkyBox::getVertexShaderSource() {
    return Shader::readFile(SHADER_DIR "skybox/skybox.vert");
}

std::string SkyBox::getFragmentShaderSource() {
    return Shader::readFile(SHADER_DIR "skybox/skybox.frag");
}
