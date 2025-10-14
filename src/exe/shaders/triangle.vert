#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTex;
layout (location = 3) in vec3 inNormal;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outTex;

layout (binding = 0) uniform UBO 
{
	mat4 projectionMatrix;
	mat4 modelMatrix;
	mat4 viewMatrix;
} ubo;

void main()  {
	outColor = inColor;
    outTex = inTex;
    gl_Position = vec4(inPos.xyz, 1.0);
	// gl_Position = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4(inPos.xyz, 1.0);
}
