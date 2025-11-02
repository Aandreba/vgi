#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTex;
layout (location = 3) in vec3 inNormal;
layout (location = 4) in uvec4 inJoints;
layout (location = 5) in vec4 inWeights;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outTex;

layout (std430, push_constant) uniform PC {
	mat4 mvp;
};
layout (std430, binding = 2) readonly buffer JointMatrix {
	mat4 mat[];
} joints;

void main() {
	outColor = inColor;
    outTex = inTex;

    mat4 skin_mat =
        inWeights.x * joints.mat[inJoints.x] +
        inWeights.y * joints.mat[inJoints.y] +
        inWeights.z * joints.mat[inJoints.z] +
        inWeights.w * joints.mat[inJoints.w];

	gl_Position = mvp * skin_mat * vec4(inPos.xyz, 1.0);
}
