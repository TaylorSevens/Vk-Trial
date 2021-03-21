#version 460 core
#extension GL_ARB_separate_shader_objects: enable

layout(location = 0) in vec2 fragTexC;

layout(binding = 0, set = 1) uniform sampler2D g_txDiffuseMap;
layout(binding = 1, set = 1) uniform sampler2D g_txMaskDiffuseMap;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(g_txDiffuseMap, fragTexC) * 
	texture(g_txMaskDiffuseMap, fragTexC);

	outColor.a = 1.0f;
}