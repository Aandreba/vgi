#version 450

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 inTex;
layout (location = 0) out vec4 outColor;
layout (binding = 0) uniform sampler2D texSampler;

void main() {
   outColor = inColor * texture(texSampler, inTex);
}
