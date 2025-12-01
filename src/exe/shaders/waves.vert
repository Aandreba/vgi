#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTex;
layout (location = 3) in vec3 inNormal;
layout (location = 4) in uvec4 inJoints;
layout (location = 5) in vec4 inWeights;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outTex;

layout (push_constant, std430) uniform PC {
	mat4 mvp;
};
layout (std430, binding = 1) readonly buffer JointMatrix {
	mat4 joints[];
};

void main() {
	outColor = inColor;
    outTex = inTex;

    vec4 pos;
    if (inWeights == vec4(0.0f)) {
        pos = mvp * vec4(inPos.xyz, 1.0);
    } else {
        mat4 skin_mat =
            inWeights.x * joints[inJoints.x] +
            inWeights.y * joints[inJoints.y] +
            inWeights.z * joints[inJoints.z] +
            inWeights.w * joints[inJoints.w];

        pos = mvp * skin_mat * vec4(inPos.xyz, 1.0);
    }

	gl_Position = vec4(pos.x, pos.y, pos.z, pos.w);
}
