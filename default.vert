#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normalMatrix;
    vec3 cameraPos;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragTangent;
layout(location = 4) out vec3 fragBitangent;
layout(location = 5) out vec3 fragCameraPos;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    
    fragNormal = normalize((ubo.normalMatrix * vec4(inNormal, 0.0)).xyz);
    fragTangent = normalize((ubo.normalMatrix * vec4(inTangent, 0.0)).xyz);
    fragBitangent = normalize(cross(fragNormal, fragTangent));
    
    fragTexCoord = vec2(inTexCoord.x, 1.0 - inTexCoord.y);
    fragCameraPos = ubo.cameraPos;
    
    gl_Position = ubo.proj * ubo.view * worldPos;
}