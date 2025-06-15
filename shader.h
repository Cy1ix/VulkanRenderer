#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <string>
#include <memory>

class VulkanContext;
enum class ShaderStage;

class Shader {
public:
    Shader(std::shared_ptr<VulkanContext> context);
    ~Shader();

    static std::string readFile(const std::string& filename);
    bool loadFromFile(const std::string& shader_path);
    bool loadFromSource(const std::string& vertex_source, const std::string& fragment_source);
    void cleanup();

    vk::ShaderModule getVertexShader() const { return vertex_shader_; }
    vk::ShaderModule getFragmentShader() const { return fragment_shader_; }
    
    std::string getDefaultVertexShader();
    std::string getDefaultFragmentShader();

private:
    bool createShaderModule(const std::vector<uint32_t>& code, vk::ShaderModule& shader_module);
    std::vector<uint32_t> compileGLSL(const std::string& source, const ShaderStage& stage);

    std::shared_ptr<VulkanContext> context_;
    vk::ShaderModule vertex_shader_;
    vk::ShaderModule fragment_shader_;
};
