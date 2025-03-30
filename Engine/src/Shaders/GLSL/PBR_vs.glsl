#version 420 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord0;
layout(location = 3) in vec2 aTexCoord1;
layout(location = 4) in vec4 aBoneIDs;   // Indices of influencing bones (up to 4)
layout(location = 5) in vec4 aWeights;   // Weights of each bone's influence


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




// Uniform buffer for bone transforms
layout (std140, binding=6) uniform BoneMatrices
{
    mat4 u_BoneTransforms[100]; // Array of bone transforms
};

out vec3 normalInterp;
out vec3 vertPos;
out vec3 camPos;
out vec2 texCoord;

uniform mat4 u_model;
uniform vec3 u_camPos;


// Skinning control - 1.0 when skinning is enabled, 0.0 when disabled
uniform float u_SkinningEnabled = 0.0;

void main()
{
    // Start with vertex in bind pose
    vec4 skinnedPosition = vec4(aPos, 1.0);
    vec3 skinnedNormal = aNormal;
    
    // Apply skinning using weighted bone transformations
    vec4 blendedPosition = vec4(0.0);
    vec3 blendedNormal = vec3(0.0);
    
    for(int i = 0; i < 4; i++) {
        // Get bone index and weight
        int boneIndex = int(aBoneIDs[i]);
        float weight = aWeights[i];
        
        // Calculate skin influence - no branches needed
        mat4 boneTransform = u_BoneTransforms[boneIndex];
        
        // Transform position and normal by bone matrix, weighted by influence
        blendedPosition += weight * (boneTransform * vec4(aPos, 1.0));
        blendedNormal += weight * (mat3(boneTransform) * aNormal);
    }
    
    // Linearly interpolate between original and skinned vertices based on skinning flag
    // When u_SkinningEnabled is 0, we get the original vertex
    // When u_SkinningEnabled is 1, we get the skinned vertex
    skinnedPosition = mix(skinnedPosition, blendedPosition, u_SkinningEnabled);
    skinnedNormal = mix(skinnedNormal, blendedNormal, u_SkinningEnabled);
    
    // Transform to world space
    vertPos = vec3(u_model * skinnedPosition);
    normalInterp = mat3(u_model) * skinnedNormal;
    
    // Output other values
    camPos = u_camPos;
    texCoord = aTexCoord0;
    
    // Final position
    gl_Position = u_proj * u_view * vec4(vertPos, 1.0);

}
