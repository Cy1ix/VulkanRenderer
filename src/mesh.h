#pragma once

#include "vertex.h"
#include <vulkan/vulkan.hpp>
#include <vector>
#include <memory>

class VulkanContext;

class Mesh {
public:
    Mesh(std::shared_ptr<VulkanContext> context);
    ~Mesh();

    bool create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void cleanup();
    
    void draw(vk::CommandBuffer command_buffer) const;
    
    size_t getVertexCount() const { return vertex_count_; }
    size_t getIndexCount() const { return index_count_; }

private:
    bool createVertexBuffer(const std::vector<Vertex>& vertices);
    bool createIndexBuffer(const std::vector<uint32_t>& indices);
    bool createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                     vk::MemoryPropertyFlags properties,
                     vk::Buffer& buffer, vk::DeviceMemory& buffer_memory);
    void copyBuffer(vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size);
    uint32_t findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);

    std::shared_ptr<VulkanContext> context_;
    
    vk::Buffer vertex_buffer_;
    vk::DeviceMemory vertex_buffer_memory_;
    vk::Buffer index_buffer_;
    vk::DeviceMemory index_buffer_memory_;
    
    size_t vertex_count_;
    size_t index_count_;
};
