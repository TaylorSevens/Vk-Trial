#version 460 core
#extension GL_ARB_separate_shader_objects: enable

layout(location = 0) in vec3 inputPos;
layout(location = 1) in vec2 inputTexC;

layout(location = 0) out vec2 vertexTexC;

layout(std140, binding = 0, set = 0) uniform cbPerObject {
	uniform mat4x4 g_matWorldViewProj;
	mat4x4 g_matTexTransform;
};

void main() {
	gl_Position = g_matWorldViewProj * vec4(inputPos, 1.0f);
	vertexTexC = (g_matTexTransform * vec4(inputTexC, 0.0f, 1.0f)).xy;
}