#version 460 core

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require // Needed for uint64_t

layout(location = 0) out vec4 gPositionDepth; // Renamed and changed to vec4
layout(location = 1) out vec3 gNormal;
layout(location = 2) out vec4 gAlbedoSpec;
layout(location = 3) out vec4 gMaterial; // R: Metallic, G: Roughness, B: AO

precision highp float;


in VS_OUT {
    vec3 FragPos;    // World position
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;     // Assumed to be valid and non-zero if normal mapping is needed
    vec3 Bitangent;   // Assumed to be valid and non-zero if normal mapping is needed
    float FragDepthView; // Added: View-space Z depth
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
    uint64_t albedoMap;
    uint64_t normalMap;
    uint64_t metallicMap;
    uint64_t roughnessMap;
    uint64_t aoMap;
    uint64_t emissiveMap;
};

vec3 getNormalFromMapNoTangent()
{
    vec3 tangentNormal = vec3(0.0);
    if (normalMap != 0) {
        tangentNormal = texture(sampler2D(normalMap), fs_in.TexCoord).xyz * 2.0 - 1.0;
    } else if ((flags & NORMAL_MAP_FLAG) != 0) {
        tangentNormal = texture(u_NormalMap, fs_in.TexCoord).xyz * 2.0 - 1.0;
    }

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

// Function to calculate tangent space normal from normal map using pre-computed TBN
vec3 getNormalFromMap()
{
    vec3 tangentNormal = vec3(0.0);
    if (normalMap != 0) {
        tangentNormal = texture(sampler2D(normalMap), fs_in.TexCoord).xyz * 2.0 - 1.0;
    } else if ((flags & NORMAL_MAP_FLAG) != 0) {
        tangentNormal = texture(u_NormalMap, fs_in.TexCoord).xyz * 2.0 - 1.0;
    }
    // Use the pre-calculated tangent and bitangent
    vec3 N = normalize(fs_in.Normal);
    vec3 T = normalize(fs_in.Tangent);
    vec3 B = normalize(fs_in.Bitangent);
    
    // Form TBN matrix from pre-computed vectors
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main() {


    // Get material properties from textures or fallback to uniforms

    vec3 albedo = baseColorFactor.rgb;

    float roughness = roughnessFactor;
    float metallic = metallicFactor;
    float ao = 1.0;
    vec3 emission = vec3(0.0);

    // Albedo
    if (albedoMap != 0) {
        sampler2D albedoSampler = sampler2D(albedoMap);
        albedo = texture(albedoSampler, fs_in.TexCoord).rgb;
    } else if ((flags & ALBEDO_MAP_FLAG) != 0) {
        albedo = texture(u_AlbedoMap, fs_in.TexCoord).rgb;
    }

    // Roughness
    if (roughnessMap != 0) {
        sampler2D roughnessSampler = sampler2D(roughnessMap);
        roughness = texture(roughnessSampler, fs_in.TexCoord).g;
    } else if ((flags & ROUGHNESS_MAP_FLAG) != 0) {
        roughness = texture(u_RoughnessMap, fs_in.TexCoord).g;
    }
    
    if (metallicMap != 0) {
        sampler2D metallicSampler = sampler2D(metallicMap);
        metallic = texture(metallicSampler, fs_in.TexCoord).b;
    } else if ((flags & METALLIC_MAP_FLAG) != 0) {
        metallic = texture(u_MetallicMap, fs_in.TexCoord).b;
    }
    
    if (aoMap != 0) {
        sampler2D aoSampler = sampler2D(aoMap);
        ao = texture(aoSampler, fs_in.TexCoord).r;
    } else if ((flags & AO_MAP_FLAG) != 0) {
        ao = texture(u_AOMap, fs_in.TexCoord).r;
    }
    
    if (emissiveMap != 0) {
        sampler2D emissiveSampler = sampler2D(emissiveMap);
        emission = texture(emissiveSampler, fs_in.TexCoord).rgb;
    } else if ((flags & EMISSIVE_MAP_FLAG) != 0) {
        emission = texture(u_EmissiveMap, fs_in.TexCoord).rgb;
    }

    // Position (world space) and Depth (view space Z)
    // Store World Position in rgb, and linear View-Space Z Depth in alpha
    gPositionDepth = vec4(fs_in.FragPos, fs_in.FragDepthView);
    
    // Normal
    vec3 normal;
    if (normalMap != 0 || (flags & NORMAL_MAP_FLAG) != 0) {
        // Check if tangent data exists by checking if it's not a zero vector
        if (length(fs_in.Tangent) > 0.01) {
            normal = getNormalFromMap();
            if (length(normal) < 0.01) {
                normal = vec3(1.0, 1.0, 1.0);
            }
        } else {
            normal = getNormalFromMapNoTangent();
        
        }    
    } else {
        normal = normalize(fs_in.Normal);
    }

    gNormal = normal;
    
    // Albedo and specular
    gAlbedoSpec = vec4(albedo, 1.0);
    
    // Material properties
    //float metallic = material_metallic;
    //float roughness = material_roughness;
    //float ao = ao;
    
    gMaterial = vec4(metallic, roughness, ao, 1.0);

}