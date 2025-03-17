#version 330 core

layout(location = 0) out vec4 outColor;

in vec4 v_Albedo;


void main()
{
	outColor = v_Albedo;
}