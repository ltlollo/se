#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding  = 0) uniform ubotype {
    float move;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    vec2 vertFlip = vec2(inPosition.x, -1 * (ubo.move + inPosition.y));
    gl_Position = vec4(vertFlip, 0.0, 1.0);
    fragColor = vec3(inColor.x, inColor.y, inColor.z);
    fragTexCoord = inTexCoord;
}
