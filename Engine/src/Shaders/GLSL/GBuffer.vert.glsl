#version 460 core

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require // Needed for uint64_t

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

// Skinning control - 1.0 when skinning is enabled, 0.0 when disabled
uniform float u_SkinningEnabled = 0.0;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 Bitangent;
    float FragDepthView;
} vs_out;

void main() {

    // Start with vertex in bind pose
    vec4 skinnedPosition = vec4(a_Position, 1.0);
    vec3 skinnedNormal = a_Normal;
    vec3 skinnedTangent = a_Tangent;
    
    // Calculate bitangent (cross product of normal and tangent)
    vec3 skinnedBitangent = cross(skinnedNormal, skinnedTangent);
    
    // Apply skinning using weighted bone transformations
    vec4 blendedPosition = vec4(0.0);
    vec3 blendedNormal = vec3(0.0);
    vec3 blendedTangent = vec3(0.0);
    vec3 blendedBitangent = vec3(0.0);
    
    for(int i = 0; i < 4; i++) {
        // Get bone index and weight
        int boneIndex = int(a_Joints[i]);
        float weight = a_Weights[i];
        
        // Calculate skin influence - no branches needed
        mat4 boneTransform = u_BoneTransforms[boneIndex];
        
        // Transform position, normal, tangent and bitangent by bone matrix, weighted by influence
        blendedPosition += weight * (boneTransform * vec4(a_Position, 1.0));
        blendedNormal += weight * (mat3(boneTransform) * a_Normal);
        blendedTangent += weight * (mat3(boneTransform) * a_Tangent);
        blendedBitangent += weight * (mat3(boneTransform) * skinnedBitangent);
    }
    
    // Linearly interpolate between original and skinned vertices based on skinning flag
    // When u_SkinningEnabled is 0, we get the original vertex
    // When u_SkinningEnabled is 1, we get the skinned vertex
    skinnedPosition = mix(skinnedPosition, blendedPosition, u_SkinningEnabled);
    skinnedNormal = mix(skinnedNormal, blendedNormal, u_SkinningEnabled);
    skinnedTangent = mix(skinnedTangent, blendedTangent, u_SkinningEnabled);
    skinnedBitangent = mix(skinnedBitangent, blendedBitangent, u_SkinningEnabled);
    
    // Transform to world space
    vs_out.FragPos = vec3(u_model * skinnedPosition);
    vs_out.Normal = mat3(u_model) * skinnedNormal;
    vs_out.Tangent = mat3(u_model) * skinnedTangent;
    vs_out.Bitangent = mat3(u_model) * skinnedBitangent;
    
    // Ensure orthogonality in world space

    vs_out.Normal = normalize(vs_out.Normal);
    vs_out.Tangent = normalize(vs_out.Tangent);
    // Re-orthogonalize tangent with respect to normal
    vs_out.Tangent = normalize(vs_out.Tangent - dot(vs_out.Tangent, vs_out.Normal) * vs_out.Normal);
    // Recalculate bitangent to ensure orthogonal basis
    vs_out.Bitangent = cross(vs_out.Normal, vs_out.Tangent);


    vs_out.TexCoord = a_TexCoord;
    
    // Calculate position in view space
    vec4 viewPos = u_view * vec4(vs_out.FragPos, 1.0);

    // Store the negative Z value (common convention, depth increases into the screen)
    // Ensure this matches how cascade splits are calculated on the CPU.
    // If cascade splits are positive distances, use abs(viewPos.z) or just viewPos.z
    vs_out.FragDepthView = -viewPos.z;

    // Final clip space position
    gl_Position = u_proj * viewPos; // Use viewPos directly for projection
}