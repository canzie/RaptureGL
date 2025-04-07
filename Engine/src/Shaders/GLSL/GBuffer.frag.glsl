#version 420 core

layout(location = 0) out vec3 gPosition;
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;
layout(location = 3) out vec4 gMaterial;

precision highp float;


in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
} fs_in;

// Define texture flag constants - must match C++ side
#define ALBEDO_MAP_FLAG    (1 << 0)
#define NORMAL_MAP_FLAG    (1 << 1)
#define METALLIC_MAP_FLAG  (1 << 2)
#define ROUGHNESS_MAP_FLAG (1 << 3)
#define AO_MAP_FLAG        (1 << 4)
#define EMISSIVE_MAP_FLAG  (1 << 5)
#define HEIGHT_MAP_FLAG    (1 << 6)

// PBR textures
layout(binding = 0) uniform sampler2D u_AlbedoMap;    // ALBEDO=0
layout(binding = 1) uniform sampler2D u_NormalMap;    // NORMAL=1
layout(binding = 2) uniform sampler2D u_MetallicMap;  // METALLIC=2
layout(binding = 3) uniform sampler2D u_RoughnessMap; // ROUGHNESS=3
layout(binding = 4) uniform sampler2D u_AOMap;        // AO=4
layout(binding = 5) uniform sampler2D u_EmissiveMap;  // EMISSION=5
layout(binding = 6) uniform sampler2D u_HeightMap;    // HEIGHT=6

layout (std140, binding=1) uniform PBR
{
	vec4 baseColorFactor;
	float metallicFactor;
	float roughnessFactor;
    float specularFactor;
    uint flags;
};

vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(u_NormalMap,  fs_in.TexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(fs_in.FragPos);
    vec3 Q2  = dFdy(fs_in.FragPos);
    vec2 st1 = dFdx(fs_in.TexCoord);
    vec2 st2 = dFdy(fs_in.TexCoord);

    vec3 N   = normalize(fs_in.Normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main() {

    // Get material properties from textures or fallback to uniforms
    vec3 albedo = ((flags & ALBEDO_MAP_FLAG) != 0) ? 
                   texture(u_AlbedoMap, fs_in.TexCoord).rgb : 
                   baseColorFactor.rgb;
                   
    float material_roughness = ((flags & ROUGHNESS_MAP_FLAG) != 0) ? 
                               texture(u_RoughnessMap, fs_in.TexCoord).r : 
                               roughnessFactor;
                               
    float material_metallic = ((flags & METALLIC_MAP_FLAG) != 0) ? 
                              texture(u_MetallicMap, fs_in.TexCoord).r : 
                              metallicFactor;
                              
    float ao = ((flags & AO_MAP_FLAG) != 0) ? 
               texture(u_AOMap, fs_in.TexCoord).r : 
               1.0;
               
    vec3 emission = ((flags & EMISSIVE_MAP_FLAG) != 0) ? 
                    texture(u_EmissiveMap, fs_in.TexCoord).rgb : 
                    vec3(0.0);

    // Position (view space)
    gPosition = fs_in.FragPos;
    
    // Normal
    vec3 normal;
    if ((flags & NORMAL_MAP_FLAG) != 0) {
        normal = getNormalFromMap();
    } else {
        normal = normalize(fs_in.Normal);
    }
    gNormal = normal;
    
    // Albedo and specular
    gAlbedoSpec = vec4(albedo, 1.0);
    
    // Material properties
    float metallic = material_metallic;
    float roughness = material_roughness;
    //float ao = ao;
    
    gMaterial = vec4(metallic, roughness, ao, 1.0);

}