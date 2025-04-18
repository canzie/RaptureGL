#version 420 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord0;


precision highp float;

layout (std140, binding=0) uniform BaseTransformMats
{
	mat4 u_proj;
	mat4 u_view;
};


layout (std140, binding=4) uniform SOLID
{
	vec4 color;
};

out vec4 v_Albedo;
out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;

uniform mat4 u_model;


void main()
{
	// Calculate position and normal
	v_Position = vec3(u_model * vec4(aPos, 1.0));
	v_Normal = mat3(u_model) * aNormal;
	

	v_Albedo = color;

	v_TexCoord = aTexCoord0;
	gl_Position = u_proj * u_view * u_model * vec4(aPos, 1.0);
}