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

layout (std140, binding=3) uniform SOLID
{
	vec4 color;
};

out vec4 v_Albedo;
out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;

uniform mat4 u_model;
uniform int u_DebugMode = 0; // 0=Normal, 1=BaseColor, 2=Normals, 3=ID-based

void main()
{
	// Calculate position and normal
	v_Position = vec3(u_model * vec4(aPos, 1.0));
	v_Normal = mat3(u_model) * aNormal;
	
	// Set color based on debug mode
	if (u_DebugMode == 0) {
		// Normal rendering - use material color
		v_Albedo = color;
	}
	else if (u_DebugMode == 1) {
		// Show material color without shading
		v_Albedo = color;
	}
	else if (u_DebugMode == 2) {
		// Show normals as colors
		v_Albedo = vec4(aNormal * 0.5 + 0.5, 1.0);
	}
	else if (u_DebugMode == 3) {
		// Show unique object color
		float r = fract(sin(gl_VertexID * 0.1) * 43758.5453);
		float g = fract(sin(gl_VertexID * 0.2) * 22578.1459);
		float b = fract(sin(gl_VertexID * 0.3) * 19642.3571);
		v_Albedo = vec4(r, g, b, 1.0);
	}
	v_TexCoord = aTexCoord0;
	gl_Position = u_proj * u_view * u_model * vec4(aPos, 1.0);
}