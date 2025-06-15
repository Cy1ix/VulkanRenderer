#pragma once

#include "vertex.h"
#include "uniform_buffer.h"
#include "texture.h"
#include <vulkan/vulkan.hpp>
#include <memory>
#include <vector>
#include <string>
#include <array>

class VulkanContext;

class SkyBox {
public:
    SkyBox(std::shared_ptr<VulkanContext> context);
    ~SkyBox();

    bool initialize();
    void cleanup();
    
    bool createDefaultSkyBox(const glm::vec3& top_color = glm::vec3(0.5f, 0.7f, 1.0f),
        const glm::vec3& bottom_color = glm::vec3(0.9f, 0.9f, 0.8f),
        int resolution = 256);

    void updateUniforms(const SkyBoxUBO& ubo);
    void draw(vk::CommandBuffer command_buffer, vk::PipelineLayout pipeline_layout);

    vk::DescriptorSetLayout getDescriptorSetLayout() const { return descriptor_set_layout_; }
    vk::DescriptorSet getDescriptorSet() const { return descriptor_set_; }

    static std::string getVertexShaderSource();
    static std::string getFragmentShaderSource();

    struct SkyBoxVertex {
        glm::vec3 position;

        static vk::VertexInputBindingDescription getBindingDescription() {
            vk::VertexInputBindingDescription binding_description{};
            binding_description.binding = 0;
            binding_description.stride = sizeof(SkyBoxVertex);
            binding_description.inputRate = vk::VertexInputRate::eVertex;
            return binding_description;
        }

        static std::array<vk::VertexInputAttributeDescription, 1> getAttributeDescriptions() {
            std::array<vk::VertexInputAttributeDescription, 1> attribute_descriptions{};

            attribute_descriptions[0].binding = 0;
            attribute_descriptions[0].location = 0;
            attribute_descriptions[0].format = vk::Format::eR32G32B32Sfloat;
            attribute_descriptions[0].offset = offsetof(SkyBoxVertex, position);

            return attribute_descriptions;
        }
    };

private:
    bool createVertexBuffer();
    bool createDescriptorSetLayout();
    bool createDescriptorPool();
    bool createDescriptorSets();
    bool createUniformBuffer();
    
    bool createCubemapTexture(int width, int height);

    bool createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties,
        vk::Buffer& buffer, vk::DeviceMemory& buffer_memory);
    uint32_t findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);
    void transitionImageLayout(vk::Image image, vk::Format format,
        vk::ImageLayout old_layout, vk::ImageLayout new_layout,
        uint32_t layer_count = 1);
    void copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height,
        uint32_t layer_index = 0);
    
    glm::vec3 uvToDirection(float u, float v, int face);
    glm::vec3 interpolateColor(const glm::vec3& color1, const glm::vec3& color2, float t);
    std::vector<unsigned char> generateFaceTexture(int face, int resolution,
        const glm::vec3& top_color,
        const glm::vec3& bottom_color);

    std::shared_ptr<VulkanContext> context_;
    
    glm::vec3 top_color_;
    glm::vec3 bottom_color_;

    vk::Buffer vertex_buffer_;
    vk::DeviceMemory vertex_buffer_memory_;
    uint32_t vertex_count_;

    vk::DescriptorSetLayout descriptor_set_layout_;
    vk::DescriptorPool descriptor_pool_;
    vk::DescriptorSet descriptor_set_;

    vk::Buffer uniform_buffer_;
    vk::DeviceMemory uniform_buffer_memory_;

    vk::Image cubemap_image_;
    vk::DeviceMemory cubemap_image_memory_;
    vk::ImageView cubemap_image_view_;
    vk::Sampler cubemap_sampler_;
};