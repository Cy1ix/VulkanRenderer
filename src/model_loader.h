#pragma once

#include "vertex.h"
#include <vector>
#include <string>

class ModelLoader {
public:
    static bool loadOBJ(const std::string& filename, 
                       std::vector<Vertex>& vertices, 
                       std::vector<uint32_t>& indices);

private:
    static void calculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    static glm::vec3 calculateTangent(const Vertex& v1, const Vertex& v2, const Vertex& v3);
    static void normalizeModel(std::vector<Vertex>& vertices, float target_size = 2.0f);
};
