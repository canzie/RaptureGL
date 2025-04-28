#version 420 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord0;
layout(location = 3) in mat4 aInstanceMatrix;


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
uniform bool u_IsInstanced = false;


void main()
{
	mat4 final_model_matrix;
	if (u_IsInstanced) {
		final_model_matrix = aInstanceMatrix;
	} else {
		final_model_matrix = u_model;
	}

	// Calculate position and normal using the selected matrix
	v_Position = vec3(final_model_matrix * vec4(aPos, 1.0));
	// Normal calculation needs inverse transpose for non-uniform scaling,
	// but for simple transforms (like bounding boxes), this is often sufficient.
	// For correctness with arbitrary scaling, use: mat3(transpose(inverse(final_model_matrix))) * aNormal;
	v_Normal = mat3(final_model_matrix) * aNormal;


	v_Albedo = color;

	v_TexCoord = aTexCoord0;
	gl_Position = u_proj * u_view * final_model_matrix * vec4(aPos, 1.0);
}