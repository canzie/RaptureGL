#version 420 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord0;
layout(location = 3) in vec2 aTexCoord1;

layout (std140) uniform BaseTransformMats
{
	mat4 u_proj;
	mat4 u_view;
};

layout (std140) uniform Phong
{
	vec4 ambientLight;
	vec4 diffuseColor;
	vec4 specularColor;
	float flux;
	float shininess;
};

out vec3 normalInterp;
out vec3 vertPos;

uniform mat4 u_model;



void main()
{

	vec4 vertPos4 = u_model * vec4(aPos, 1.0);
	// for frament shader
	vertPos = vec3(vertPos4) / vertPos4.w;
	normalInterp = normalize(vec3(transpose(inverse(u_model)) * vec4(aNormal, 0.0)));

	gl_Position = u_proj * u_view * vertPos4;

}