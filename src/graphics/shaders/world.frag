#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outFragColor;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

layout(set = 3, binding = 0) uniform FragUniforms {
    float nightBlend;
} frag;

void main() {
    vec4 color = inColor * texture(texSampler, inTexCoord);
    // Apply night darkening: lerp toward dark blue at nightBlend=1
    vec3 nightColor = color.rgb * mix(vec3(1.0), vec3(0.15, 0.15, 0.25), frag.nightBlend);
    outFragColor = vec4(nightColor, color.a);
}
