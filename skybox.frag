#version 450

layout(binding = 1) uniform samplerCube skybox;

layout(location = 0) in vec3 TexCoords;

layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = texture(skybox, TexCoords);
}