#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outTexCoord;

layout(set = 1, binding = 0) uniform Uniforms {
    mat4 viewProjection;
} ubo;

void main() {
    vec4 clipPos = ubo.viewProjection * vec4(inPosition, 1.0);
    // The CPU projection matrix generates OpenGL-style Y-up and Z in [-1, 1].
    // SDL_GPU/SPIR-V uses Y-down and Z in [0, 1] for screen space.
    clipPos.y = -clipPos.y;
    clipPos.z = (clipPos.z + clipPos.w) * 0.5;
    gl_Position = clipPos;
    
    outColor = inColor;
    outTexCoord = inTexCoord;
}
