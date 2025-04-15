#version 420 core

// Basic vertex attributes
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 4) in vec4 a_Joints;   // Indices of influencing bones (up to 4)
layout(location = 5) in vec4 a_Weights;   // Weights of each bone's influence

layout(location = 6) in vec3 a_Tangent;

// Output to geometry shader - simplified since matrices are applied in geometry shader
out VS_OUT {
    vec3 position;
} vs_out;

// Shadow matrices uniform block - defined but not used in vertex shader
// Used by the geometry shader to transform vertices for each cascade
layout (std140, binding=8) uniform shadowMatrices {
    mat4 u_LightSpaceMatrix[4];
};

uniform float u_IsSkinnedMesh = 0.0f;
uniform mat4 u_model;

void main() {
    // Calculate model space position
    vec4 modelPosition = u_model * vec4(a_Position, 1.0);
    
    // Optional: Apply skinning if this is a skinned mesh
    if (u_IsSkinnedMesh > 0.5) {
        // Add skinning calculation here if needed
    }
    
    // Simply pass the model-space position to the geometry shader
    // Geometry shader will handle transforming to each cascade's clip space
    vs_out.position = modelPosition.xyz;
    
    // This gets ignored by the rasterizer but helps with compatibility
    // when not using the geometry shader
    gl_Position = modelPosition;
}