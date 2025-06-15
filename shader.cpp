#include "shader.h"
#include "vulkan_context.h"
#include "utils/logger.h"
#include "glsl_compiler.h"
#include <fstream>
#include <sstream>

Shader::Shader(std::shared_ptr<VulkanContext> context) : context_(context) {
}

Shader::~Shader() {
    cleanup();
}

ShaderStage getShaderType(const std::string& file_name) {

    std::string file_type;

    size_t pos = file_name.find_last_of('.');

    if (pos != std::string::npos && pos != 0) {
        file_type = file_name.substr(pos + 1);
    }
    else {
        LOGE("Wrong shader file name!");
    }

    static const std::unordered_map<const char*, ShaderStage> shader_stage_map{
        {"vert", ShaderStage::VERTEX},
        {"frag", ShaderStage::FRAGMENT},
        {"geom", ShaderStage::GEOMETRY},
    };

    return shader_stage_map.find(file_type.c_str())->second;
}

bool Shader::loadFromFile(const std::string& shader_path) {
    auto shader_spirv = compileGLSL(readFile(shader_path), getShaderType(shader_path));

    if (shader_spirv.empty()) {
        LOGE("Failed to read shader files");
        return false;
    }

    return createShaderModule(shader_spirv, vertex_shader_);
}

bool Shader::loadFromSource(const std::string& vertex_source, const std::string& fragment_source) {
    auto vertex_spirv = compileGLSL(vertex_source, ShaderStage::VERTEX);
    auto fragment_spirv = compileGLSL(fragment_source, ShaderStage::FRAGMENT);

    if (vertex_spirv.empty() || fragment_spirv.empty()) {
        LOGE("Failed to compile shaders");
        return false;
    }

    std::vector<char> vertex_code(vertex_spirv.size() * sizeof(uint32_t));
    std::vector<char> fragment_code(fragment_spirv.size() * sizeof(uint32_t));

    memcpy(vertex_code.data(), vertex_spirv.data(), vertex_code.size());
    memcpy(fragment_code.data(), fragment_spirv.data(), fragment_code.size());

    return createShaderModule(vertex_spirv, vertex_shader_) &&
           createShaderModule(fragment_spirv, fragment_shader_);
}

void Shader::cleanup() {
    auto device = context_->getDevice();
    
    if (vertex_shader_) {
        device.destroyShaderModule(vertex_shader_);
        vertex_shader_ = nullptr;
    }
    
    if (fragment_shader_) {
        device.destroyShaderModule(fragment_shader_);
        fragment_shader_ = nullptr;
    }
}

bool Shader::createShaderModule(const std::vector<uint32_t>& code, vk::ShaderModule& shader_module) {
    vk::ShaderModuleCreateInfo create_info{};
    create_info.codeSize = code.size() * sizeof(uint32_t);
    create_info.pCode = code.data();

    try {
        shader_module = context_->getDevice().createShaderModule(create_info);
        return true;
    } catch (const std::exception& e) {
        LOGE("Failed to create shader module: {}", e.what());
        return false;
    }
}

std::string Shader::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        LOGE("Failed to open file: {}", filename);
        return {};
    }

    file.seekg(0, std::ios::end);
    const size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    if (size == 0) return {};

    std::string content(size, '\0');
    file.read(content.data(), static_cast<std::streamsize>(size));

    return content;
}

std::vector<uint32_t> Shader::compileGLSL(const std::string& source, const ShaderStage& stage) {
    ShadercCompiler compiler;
    compiler.setOptimizationLevel(true);
    return compiler.compileFromSource(source, "", stage);
}

std::string Shader::getDefaultVertexShader() {
    return readFile(SHADER_DIR "default.vert");
}

std::string Shader::getDefaultFragmentShader() {
    return readFile(SHADER_DIR "default.frag");
}
