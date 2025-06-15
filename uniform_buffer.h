#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 normal_matrix;
    alignas(16) glm::vec3 camera_pos;
};

struct PBRMaterial {
    alignas(16) glm::vec3 albedo = glm::vec3(1.0f);
    alignas(4) float metallic = 0.0f;
    alignas(4) float roughness = 0.5f;
    alignas(4) float ao = 1.0f;
};

struct LightData {
    alignas(16) glm::vec3 position = glm::vec3(10.0f, 10.0f, 10.0f);
    alignas(16) glm::vec3 color = glm::vec3(300.0f);
};

struct SkyBoxUBO {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 projection;
};
