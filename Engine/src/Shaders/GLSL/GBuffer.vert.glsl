#version 420 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 4) in vec4 a_Joints;   // Indices of influencing bones (up to 4)
layout(location = 5) in vec4 a_Weights;   // Weights of each bone's influence
layout(location = 6) in vec3 a_Tangent;

precision highp float;


layout(std140, binding = 0) uniform BaseTransformMats {
	mat4 u_proj;
	mat4 u_view;
};

layout(std140, binding = 6) uniform BoneMatrices {
    mat4 u_BoneTransforms[100]; // Array of bone transforms
};

uniform mat4 u_model;


out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} vs_out;

void main() {
    vs_out.FragPos = vec3(u_model * vec4(a_Position, 1.0));
    vs_out.Normal = normalize(a_Normal);
    vs_out.TexCoord = a_TexCoord;
    
    
    gl_Position = u_proj * u_view * u_model * vec4(a_Position, 1.0);
}