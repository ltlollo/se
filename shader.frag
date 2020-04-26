#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;
layout(binding = 1) uniform sampler2D texSampler;

void main() {
    outColor = texture(texSampler, fragTexCoord);
    vec4 bg = vec4(0.152, 0.156, 0.13, 1.0);
    vec4 color = vec4(fragColor, 1.0);
    if (color.r <= 1.0) {
        outColor = outColor.r * color + bg;
    } else {
        outColor = (color - 1.0 - outColor.r);
    }
}
