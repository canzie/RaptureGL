#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord0;


precision highp float;

layout (std140) uniform BaseTransformMats
{
	mat4 u_proj;
	mat4 u_view;
};

layout (std140) uniform SOLID
{
	vec4 base_color;
};

out vec4 v_Albedo;


uniform mat4 u_model;

void main()
{
	v_Albedo = base_color;
	gl_Position = u_proj * u_view * u_model * vec4(aPos, 1.0);

}