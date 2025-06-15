#include "model_loader.h"
#include "utils/logger.h"
#include <fstream>
#include <sstream>
#include <unordered_map>

bool ModelLoader::loadOBJ(const std::string& filename, 
                         std::vector<Vertex>& vertices, 
                         std::vector<uint32_t>& indices) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOGE("Failed to open file: {}", filename);
        return false;
    }

    std::vector<glm::vec3> temp_positions;
    std::vector<glm::vec3> temp_normals;
    std::vector<glm::vec2> temp_tex_coords;
    std::unordered_map<std::string, uint32_t> vertex_map;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            glm::vec3 position;
            iss >> position.x >> position.y >> position.z;
            temp_positions.push_back(position);
        }
        else if (prefix == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            temp_normals.push_back(normal);
        }
        else if (prefix == "vt") {
            glm::vec2 tex_coord;
            iss >> tex_coord.x >> tex_coord.y;
            temp_tex_coords.push_back(tex_coord);
        }
        else if (prefix == "f") {
            std::string vertex_str;
            std::vector<uint32_t> face_indices;

            while (iss >> vertex_str) {
                if (vertex_map.find(vertex_str) != vertex_map.end()) {
                    face_indices.push_back(vertex_map[vertex_str]);
                } else {
                    Vertex vertex{};
                    std::istringstream vertex_stream(vertex_str);
                    std::string index_str;
                    
                    if (std::getline(vertex_stream, index_str, '/') && !index_str.empty()) {
                        int pos_index = std::stoi(index_str) - 1;
                        if (pos_index >= 0 && pos_index < temp_positions.size()) {
                            vertex.position = temp_positions[pos_index];
                        }
                    }
                    
                    if (std::getline(vertex_stream, index_str, '/') && !index_str.empty()) {
                        int tex_index = std::stoi(index_str) - 1;
                        if (tex_index >= 0 && tex_index < temp_tex_coords.size()) {
                            vertex.tex_coord = temp_tex_coords[tex_index];
                        }
                    }
                    
                    if (std::getline(vertex_stream, index_str) && !index_str.empty()) {
                        int norm_index = std::stoi(index_str) - 1;
                        if (norm_index >= 0 && norm_index < temp_normals.size()) {
                            vertex.normal = temp_normals[norm_index];
                        }
                    }

                    vertex_map[vertex_str] = static_cast<uint32_t>(vertices.size());
                    face_indices.push_back(static_cast<uint32_t>(vertices.size()));
                    vertices.push_back(vertex);
                }
            }
            
            if (face_indices.size() >= 3) {
                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[1]);
                indices.push_back(face_indices[2]);

                if (face_indices.size() == 4) {
                    indices.push_back(face_indices[0]);
                    indices.push_back(face_indices[2]);
                    indices.push_back(face_indices[3]);
                }
            }
        }
    }

    file.close();

    if (vertices.empty()) {
        LOGE("No vertices loaded from file: {}", filename);
        return false;
    }

    normalizeModel(vertices, 2.0f);
    
    calculateTangents(vertices, indices);

    LOGI("Loaded model: {} vertices, {} indices", vertices.size(), indices.size());
    return true;
}

void ModelLoader::calculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
    for (auto& vertex : vertices) {
        vertex.tangent = glm::vec3(0.0f);
    }
    
    for (size_t i = 0; i < indices.size(); i += 3) {
        if (i + 2 < indices.size()) {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            if (i0 < vertices.size() && i1 < vertices.size() && i2 < vertices.size()) {
                glm::vec3 tangent = calculateTangent(vertices[i0], vertices[i1], vertices[i2]);
                
                vertices[i0].tangent += tangent;
                vertices[i1].tangent += tangent;
                vertices[i2].tangent += tangent;
            }
        }
    }
    
    for (auto& vertex : vertices) {
        if (glm::length(vertex.tangent) > 0.0f) {
            vertex.tangent = glm::normalize(vertex.tangent);
        } else {
            vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        }
    }
}

glm::vec3 ModelLoader::calculateTangent(const Vertex& v1, const Vertex& v2, const Vertex& v3) {
    glm::vec3 edge1 = v2.position - v1.position;
    glm::vec3 edge2 = v3.position - v1.position;
    
    glm::vec2 delta_uv1 = v2.tex_coord - v1.tex_coord;
    glm::vec2 delta_uv2 = v3.tex_coord - v1.tex_coord;
    
    float f = 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y);
    
    glm::vec3 tangent;
    tangent.x = f * (delta_uv2.y * edge1.x - delta_uv1.y * edge2.x);
    tangent.y = f * (delta_uv2.y * edge1.y - delta_uv1.y * edge2.y);
    tangent.z = f * (delta_uv2.y * edge1.z - delta_uv1.y * edge2.z);
    
    return tangent;
}

void ModelLoader::normalizeModel(std::vector<Vertex>& vertices, float target_size) {
    if (vertices.empty()) return;
    
    glm::vec3 min_pos(FLT_MAX);
    glm::vec3 max_pos(-FLT_MAX);

    for (const auto& vertex : vertices) {
        min_pos = glm::min(min_pos, vertex.position);
        max_pos = glm::max(max_pos, vertex.position);
    }
    
    glm::vec3 center = (min_pos + max_pos) * 0.5f;
    glm::vec3 size = max_pos - min_pos;
    float max_extent = std::max({ size.x, size.y, size.z });

    if (max_extent == 0.0f) return;
    
    float scale = target_size / max_extent;
    
    for (auto& vertex : vertices) {
        vertex.position = (vertex.position - center) * scale;
    }
}
