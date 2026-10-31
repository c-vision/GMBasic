// GPU/Shaders/sprite.vert — Vertex shader per sprite rendering
#version 450

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
} pc;

layout(location = 0) out vec2 vTexCoord;

void main() {
    // Normalize to clip space
    vec2 pos = aPosition / pc.screenSize * 2.0 - 1.0;
    pos.y = -pos.y; // Flip Y for Vulkan
    gl_Position = vec4(pos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
