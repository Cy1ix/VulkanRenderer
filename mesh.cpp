#include "mesh.h"
#include "vulkan_context.h"
#include "utils/logger.h"

Mesh::Mesh(std::shared_ptr<VulkanContext> context)
    : context_(context), vertex_count_(0), index_count_(0) {
}

Mesh::~Mesh() {
    cleanup();
}

bool Mesh::create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    vertex_count_ = vertices.size();
    index_count_ = indices.size();

    if (!createVertexBuffer(vertices)) {
        LOGE("Failed to create vertex buffer");
        return false;
    }

    if (!createIndexBuffer(indices)) {
        LOGE("Failed to create index buffer");
        return false;
    }

    LOGI("Mesh created with {} vertices and {} indices", vertex_count_, index_count_);
    return true;
}

void Mesh::cleanup() {
    auto device = context_->getDevice();
    
    if (vertex_buffer_) {
        device.destroyBuffer(vertex_buffer_);
        device.freeMemory(vertex_buffer_memory_);
        vertex_buffer_ = nullptr;
    }
    
    if (index_buffer_) {
        device.destroyBuffer(index_buffer_);
        device.freeMemory(index_buffer_memory_);
        index_buffer_ = nullptr;
    }
}

void Mesh::draw(vk::CommandBuffer command_buffer) const {
    vk::Buffer vertex_buffers[] = { vertex_buffer_ };
    vk::DeviceSize offsets[] = { 0 };
    
    command_buffer.bindVertexBuffers(0, 1, vertex_buffers, offsets);
    command_buffer.bindIndexBuffer(index_buffer_, 0, vk::IndexType::eUint32);
    command_buffer.drawIndexed(static_cast<uint32_t>(index_count_), 1, 0, 0, 0);
}

bool Mesh::createVertexBuffer(const std::vector<Vertex>& vertices) {
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
    
    copyBuffer(staging_buffer, vertex_buffer_, buffer_size);
    
    device.destroyBuffer(staging_buffer);
    device.freeMemory(staging_buffer_memory);
    
    return true;
}

bool Mesh::createIndexBuffer(const std::vector<uint32_t>& indices) {
    vk::DeviceSize buffer_size = sizeof(indices[0]) * indices.size();
    
    vk::Buffer staging_buffer;
    vk::DeviceMemory staging_buffer_memory;
    
    if (!createBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     staging_buffer, staging_buffer_memory)) {
        return false;
    }
    
    auto device = context_->getDevice();
    void* data = device.mapMemory(staging_buffer_memory, 0, buffer_size);
    memcpy(data, indices.data(), static_cast<size_t>(buffer_size));
    device.unmapMemory(staging_buffer_memory);
    
    if (!createBuffer(buffer_size,
                     vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                     vk::MemoryPropertyFlagBits::eDeviceLocal,
                     index_buffer_, index_buffer_memory_)) {
        device.destroyBuffer(staging_buffer);
        device.freeMemory(staging_buffer_memory);
        return false;
    }
    
    copyBuffer(staging_buffer, index_buffer_, buffer_size);
    
    device.destroyBuffer(staging_buffer);
    device.freeMemory(staging_buffer_memory);
    
    return true;
}

bool Mesh::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                       vk::MemoryPropertyFlags properties,
                       vk::Buffer& buffer, vk::DeviceMemory& buffer_memory) {
    auto device = context_->getDevice();
    
    vk::BufferCreateInfo buffer_info{};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;
    
    try {
        buffer = device.createBuffer(buffer_info);
    } catch (const std::exception& e) {
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
    } catch (const std::exception& e) {
        LOGE("Failed to allocate buffer memory: {}", e.what());
        device.destroyBuffer(buffer);
        return false;
    }
    
    return true;
}

void Mesh::copyBuffer(vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size) {
    vk::CommandBuffer command_buffer = context_->beginSingleTimeCommands();
    
    vk::BufferCopy copy_region{};
    copy_region.size = size;
    command_buffer.copyBuffer(src_buffer, dst_buffer, 1, &copy_region);
    
    context_->endSingleTimeCommands(command_buffer);
}

uint32_t Mesh::findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties mem_properties = context_->getPhysicalDevice().getMemoryProperties();
    
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && 
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    throw std::runtime_error("Failed to find suitable memory type!");
}
