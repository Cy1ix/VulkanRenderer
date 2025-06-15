#pragma once
#include <shaderc/shaderc.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <unordered_map>

enum class ShaderStage {
    VERTEX,
    FRAGMENT,
    GEOMETRY,
};

class ShadercCompiler {
private:
    shaderc::Compiler compiler_;
    shaderc::CompileOptions options_;
    
    static const std::unordered_map<ShaderStage, shaderc_shader_kind> stageMap_;

public:
    ShadercCompiler() {
        options_.SetOptimizationLevel(shaderc_optimization_level_performance);
        options_.SetGenerateDebugInfo();
        options_.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
        options_.SetTargetSpirv(shaderc_spirv_version_1_0);
    }
    
    std::vector<uint32_t> compileFromFile(const std::string& filePath, ShaderStage stage) {
        std::string source = readFile(filePath);
        if (source.empty()) {
            throw std::runtime_error("Failed to read shader file: " + filePath);
        }

        return compileFromSource(source, filePath, stage);
    }
    
    std::vector<uint32_t> compileFromSource(const std::string& source,
        const std::string& name,
        ShaderStage stage) {
        auto it = stageMap_.find(stage);
        if (it == stageMap_.end()) {
            throw std::runtime_error("Unsupported shader stage");
        }

        shaderc::SpvCompilationResult result = compiler_.CompileGlslToSpv(
            source, it->second, name.c_str(), options_);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            throw std::runtime_error("Shader compilation failed: " + result.GetErrorMessage());
        }

        return std::vector<uint32_t>(result.cbegin(), result.cend());
    }
    
    std::string preprocessSource(const std::string& source,
        const std::string& name,
        ShaderStage stage) {
        auto it = stageMap_.find(stage);
        if (it == stageMap_.end()) {
            throw std::runtime_error("Unsupported shader stage");
        }

        shaderc::PreprocessedSourceCompilationResult result =
            compiler_.PreprocessGlsl(source, it->second, name.c_str(), options_);

        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            throw std::runtime_error("Shader preprocessing failed: " + result.GetErrorMessage());
        }

        return std::string(result.cbegin(), result.cend());
    }
    
    void addMacroDefinition(const std::string& name, const std::string& value = "") {
        options_.AddMacroDefinition(name, value);
    }
    
    void addIncludeDirectory(const std::string& path) {
        // TODO: Need implement shaderc::CompileOptions::IncludeResolver to customize includer to handle include
    }
    
    void setOptimizationLevel(bool optimize) {
        if (optimize) {
            options_.SetOptimizationLevel(shaderc_optimization_level_performance);
        }
        else {
            options_.SetOptimizationLevel(shaderc_optimization_level_zero);
        }
    }
    
    void setGenerateDebugInfo(bool enable) {
        if (enable) {
            options_.SetGenerateDebugInfo();
        }
        else {
            options_.SetSuppressWarnings();
        }
    }

private:
    std::string readFile(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            return "";
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::string buffer(fileSize, ' ');

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }
};

const std::unordered_map<ShaderStage, shaderc_shader_kind> ShadercCompiler::stageMap_ = {
    {ShaderStage::VERTEX, shaderc_glsl_vertex_shader},
    {ShaderStage::FRAGMENT, shaderc_glsl_fragment_shader},
    {ShaderStage::GEOMETRY, shaderc_glsl_geometry_shader},
};
