#version 420 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aTexCoord0;


precision highp float;

layout (std140, binding=0) uniform BaseTransformMats
{
	mat4 u_proj;
	mat4 u_view;
};

layout (std140, binding=1) uniform PBR
{
	vec3 base_color;
	float roughness;
	float metallic;
    float specular;
};

out vec3 normalInterp;
out vec3 vertPos;
out vec3 camPos;

uniform mat4 u_model;
uniform vec3 u_camPos;



void main()
{

	vertPos = vec3(u_model * vec4(aPos, 1.0));
    normalInterp = mat3(u_model) * aNormal;

    camPos = u_camPos;

	gl_Position = u_proj * u_view * vec4(vertPos, 1.0);

}