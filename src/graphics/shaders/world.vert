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
    // SDL_GPU handles the backend-specific viewport orientation. Keep the
    // projection matrix's Y coordinate unchanged so GPU and SDL_RenderGeometry
    // place positive clip-space Y toward the top of the viewport.
    // Convert only OpenGL-style Z [-1, 1] to SDL_GPU Z [0, 1].
    clipPos.z = (clipPos.z + clipPos.w) * 0.5;
    gl_Position = clipPos;
    
    outColor = inColor;
    outTexCoord = inTexCoord;
}
