#pragma once

#include "texture.h"
#include "uniform_buffer.h"
#include <vulkan/vulkan.hpp>
#include <memory>

class VulkanContext;

class Material {
public:
    Material(std::shared_ptr<VulkanContext> context);
    ~Material();

    bool initialize();
    void cleanup();

    void setPBRProperties(const glm::vec3& albedo, float metallic, float roughness, float ao);
    void setLightProperties(const glm::vec3& position, const glm::vec3& color);
    bool setTexture(std::shared_ptr<Texture> texture);

    void updateUniforms(const UniformBufferObject& ubo);
    void bind(vk::CommandBuffer command_buffer, vk::PipelineLayout pipeline_layout);

    vk::DescriptorSetLayout getDescriptorSetLayout() const { return descriptor_set_layout_; }
    vk::DescriptorSet getDescriptorSet() const { return descriptor_set_; }
    
private:
    bool createDescriptorSetLayout();
    bool createDescriptorPool();
    bool createDescriptorSets();
    bool createUniformBuffers();
    
    bool createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                     vk::MemoryPropertyFlags properties,
                     vk::Buffer& buffer, vk::DeviceMemory& buffer_memory);
    uint32_t findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);

    std::shared_ptr<VulkanContext> context_;
    std::shared_ptr<Texture> texture_;

    vk::DescriptorSetLayout descriptor_set_layout_;
    vk::DescriptorPool descriptor_pool_;
    vk::DescriptorSet descriptor_set_;
    
    vk::Buffer uniform_buffer_;
    vk::DeviceMemory uniform_buffer_memory_;
    vk::Buffer material_buffer_;
    vk::DeviceMemory material_buffer_memory_;
    vk::Buffer light_buffer_;
    vk::DeviceMemory light_buffer_memory_;

    PBRMaterial pbr_material_;
    LightData light_data_;
    
};
