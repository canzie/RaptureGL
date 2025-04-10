#version 330 core

// Basic vertex attributes
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 4) in vec4 a_Joints;   // Indices of influencing bones (up to 4)
layout(location = 5) in vec4 a_Weights;   // Weights of each bone's influence

layout(location = 6) in vec3 a_Tangent;

// Uniforms
uniform mat4 gWVP;  // World-View-Projection matrix
uniform float u_IsSkinnedMesh = 0.0f;


void main() {
    
    gl_Position = gWVP * vec4(a_Position, 1.0);
}