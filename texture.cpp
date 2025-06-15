#include "texture.h"
#include "vulkan_context.h"
#include "utils/logger.h"

#include <stb_image/stb_image.h>
#include <stb_image/stb_image_write.h>

void ImageData::free() {
    if (pixels) {
        stbi_image_free(pixels);
        pixels = nullptr;
        width = height = channels = 0;
    }
}

Texture::Texture(std::shared_ptr<VulkanContext> context) : context_(context) {
}

Texture::~Texture() {
    cleanup();
}

bool Texture::loadFromFile(const std::string& filename) {
    ImageData imageData = loadImageData(filename, 4);
    if (!imageData.isValid()) {
        LOGE("Failed to load texture: {}", filename);
        return false;
    }

    vk::DeviceSize image_size = imageData.width * imageData.height * 4;
    auto device = context_->getDevice();
    
    vk::Buffer staging_buffer;
    vk::DeviceMemory staging_buffer_memory;

    if (!createBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
        staging_buffer, staging_buffer_memory)) {
        return false;
    }

    void* data = device.mapMemory(staging_buffer_memory, 0, image_size);
    memcpy(data, imageData.pixels, static_cast<size_t>(image_size));
    device.unmapMemory(staging_buffer_memory);
    
    if (!createImage(imageData.width, imageData.height, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal)) {
        device.destroyBuffer(staging_buffer);
        device.freeMemory(staging_buffer_memory);
        return false;
    }
    
    transitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(staging_buffer, static_cast<uint32_t>(imageData.width), static_cast<uint32_t>(imageData.height));
    transitionImageLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    device.destroyBuffer(staging_buffer);
    device.freeMemory(staging_buffer_memory);
    
    if (!createImageView(vk::Format::eR8G8B8A8Srgb) || !createSampler()) {
        return false;
    }

    LOGI("Texture loaded successfully: {}", filename);
    return true;
}

void Texture::cleanup() {
    auto device = context_->getDevice();

    if (sampler_) device.destroySampler(sampler_);
    if (image_view_) device.destroyImageView(image_view_);
    if (image_) device.destroyImage(image_);
    if (image_memory_) device.freeMemory(image_memory_);
}

ImageData Texture::loadImageData(const std::string& filename, int desired_channels) {
    ImageData data;

    data.pixels = stbi_load(filename.c_str(), &data.width, &data.height, &data.channels, desired_channels);

    if (!data.pixels) {
        LOGE("Failed to load image: {}", filename);
        return data;
    }
    
    if (desired_channels > 0) {
        data.channels = desired_channels;
    }

    LOGI("Loaded image: {} ({}x{}, {} channels)", filename, data.width, data.height, data.channels);
    return data;
}

void Texture::freeImageData(ImageData& data) {
    data.free();
}

bool Texture::saveImageData(const std::string& filename, const std::vector<unsigned char>& data,
    int width, int height, int channels) {
    return stbi_write_png(filename.c_str(), width, height, channels, data.data(), width * channels) != 0;
}

bool Texture::createImage(uint32_t width, uint32_t height, vk::Format format,
    vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties) {
    auto device = context_->getDevice();

    vk::ImageCreateInfo image_info{};
    image_info.imageType = vk::ImageType::e2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = tiling;
    image_info.initialLayout = vk::ImageLayout::eUndefined;
    image_info.usage = usage;
    image_info.samples = vk::SampleCountFlagBits::e1;
    image_info.sharingMode = vk::SharingMode::eExclusive;

    try {
        image_ = device.createImage(image_info);
    }
    catch (const std::exception& e) {
        LOGE("Failed to create image: {}", e.what());
        return false;
    }

    vk::MemoryRequirements mem_requirements = device.getImageMemoryRequirements(image_);
    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = findMemoryType(mem_requirements.memoryTypeBits, properties);

    try {
        image_memory_ = device.allocateMemory(alloc_info);
        device.bindImageMemory(image_, image_memory_, 0);
    }
    catch (const std::exception& e) {
        LOGE("Failed to allocate image memory: {}", e.what());
        return false;
    }

    return true;
}

bool Texture::createImageView(vk::Format format) {
    auto device = context_->getDevice();

    vk::ImageViewCreateInfo view_info{};
    view_info.image = image_;
    view_info.viewType = vk::ImageViewType::e2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    try {
        image_view_ = device.createImageView(view_info);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create texture image view: {}", e.what());
        return false;
    }
}

bool Texture::createSampler() {
    auto device = context_->getDevice();

    vk::SamplerCreateInfo sampler_info{};
    sampler_info.magFilter = vk::Filter::eLinear;
    sampler_info.minFilter = vk::Filter::eLinear;
    sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
    sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
    sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = 16.0f;
    sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = vk::CompareOp::eAlways;
    sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;

    try {
        sampler_ = device.createSampler(sampler_info);
        return true;
    }
    catch (const std::exception& e) {
        LOGE("Failed to create texture sampler: {}", e.what());
        return false;
    }
}

void Texture::transitionImageLayout(vk::ImageLayout old_layout, vk::ImageLayout new_layout) {
    vk::CommandBuffer command_buffer = context_->beginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image_;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

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

    command_buffer.pipelineBarrier(source_stage, destination_stage, vk::DependencyFlags{}, 0, nullptr, 0, nullptr, 1, &barrier);
    context_->endSingleTimeCommands(command_buffer);
}

void Texture::copyBufferToImage(vk::Buffer buffer, uint32_t width, uint32_t height) {
    vk::CommandBuffer command_buffer = context_->beginSingleTimeCommands();

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = vk::Offset3D{ 0, 0, 0 };
    region.imageExtent = vk::Extent3D{ width, height, 1 };

    command_buffer.copyBufferToImage(buffer, image_, vk::ImageLayout::eTransferDstOptimal, 1, &region);
    context_->endSingleTimeCommands(command_buffer);
}

bool Texture::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
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

uint32_t Texture::findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties mem_properties = context_->getPhysicalDevice().getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}
