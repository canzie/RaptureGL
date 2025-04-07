#version 420 core

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;


layout (std140, binding=0) uniform BaseTransformMats
{
	mat4 u_proj;
	mat4 u_view;
};

uniform mat4 u_model;

void main()
{
    TexCoords = aPos;
    gl_Position = u_proj * u_view * u_model * vec4(aPos, 1.0);
}  