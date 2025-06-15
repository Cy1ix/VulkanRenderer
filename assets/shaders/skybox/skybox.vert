#version 450

layout(binding = 0) uniform SkyBoxUBO {
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 TexCoords;

void main() {
    TexCoords = inPosition;
    
    mat4 rotView = mat4(mat3(ubo.view));
    vec4 clipPos = ubo.projection * rotView * vec4(inPosition, 1.0);
    
    gl_Position = clipPos.xyww;
}
