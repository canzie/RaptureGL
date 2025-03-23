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

layout (std140, binding=4) uniform SpecularGlossiness
{
	vec3 diffuse_color;
	float glossiness;
	vec3 specular_color;
    float padding;
};

out vec3 normalInterp;
out vec3 vertPos;
out vec3 camPos;
out vec2 texCoord;

uniform mat4 u_model;
uniform vec3 u_camPos;

void main()
{
	vertPos = vec3(u_model * vec4(aPos, 1.0));
    normalInterp = mat3(u_model) * aNormal;
    camPos = u_camPos;
    texCoord = aTexCoord0;

	gl_Position = u_proj * u_view * vec4(vertPos, 1.0);
} 