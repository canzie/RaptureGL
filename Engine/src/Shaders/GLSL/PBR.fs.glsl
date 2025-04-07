#version 420 core

layout(location = 0) out vec4 outColor;

precision highp float;

in vec3 normalInterp;
in vec3 vertPos;
in vec3 camPos;
in vec2 texCoord;
in vec3 tangentInterp;  // Tangent from vertex shader
in vec3 bitangentInterp; // Bitangent from vertex shader

// Removed hardcoded light
// const vec3 lightPos = vec3(1.25, 1.0, 2.0);

// Define max light count - must match the C++ side
#define MAX_LIGHTS 8

// Light types
#define LIGHT_TYPE_POINT       0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_SPOT        2

// Define texture flag constants - must match C++ side
#define ALBEDO_MAP_FLAG    (1 << 0)
#define NORMAL_MAP_FLAG    (1 << 1)
#define METALLIC_MAP_FLAG  (1 << 2)
#define ROUGHNESS_MAP_FLAG (1 << 3)
#define AO_MAP_FLAG        (1 << 4)
#define EMISSIVE_MAP_FLAG  (1 << 5)
#define HEIGHT_MAP_FLAG    (1 << 6)

// Light data structure
struct Light {
    vec4 position;     // xyz = position, w = type (0=point, 1=directional, 2=spot)
    vec4 color;        // xyz = color, w = intensity
    vec4 direction;    // xyz = direction (for spot/directional), w = range
    vec4 coneAngles;   // x = innerConeAngle, y = outerConeAngle (for spot lights)
};

layout (std140, binding=1) uniform PBR
{
	vec4 baseColorFactor;
	float metallicFactor;
	float roughnessFactor;
    float specularFactor;
    uint flags;
};

// Light uniform buffer
layout(std140, binding = 2) uniform Lights {
    uint lightCount;
    Light lights[MAX_LIGHTS];
};

layout(std140, binding = 7) uniform Camera
{
    vec3 u_camPos;
};

// PBR textures
layout(binding = 0) uniform sampler2D u_AlbedoMap;    // ALBEDO=0
layout(binding = 1) uniform sampler2D u_NormalMap;    // NORMAL=1
layout(binding = 2) uniform sampler2D u_MetallicMap;  // METALLIC=2
layout(binding = 3) uniform sampler2D u_RoughnessMap; // ROUGHNESS=3
layout(binding = 4) uniform sampler2D u_AOMap;        // AO=4
layout(binding = 5) uniform sampler2D u_EmissiveMap;  // EMISSION=5
layout(binding = 6) uniform sampler2D u_HeightMap;    // HEIGHT=6




// Debug uniform to control visualization mode
uniform int u_DebugMode = 0; // 0=Normal, 1=BaseColor, 2=Normals, 3=ID-based

#define PI 3.1415926535897932384626433832795

vec3 lin2rgb(vec3 lin) {
	return pow(lin, vec3(1.0/2.2));
}

vec3 rgb2lin(vec3 rgb) {
	return pow(rgb, vec3(2.2));
}

// normal distribution function
// GGX/Trowbridge-reitz model
float distributionGGX(float NdotH, float roughness)
{
    float a = pow(roughness, 2.0);
    float a2 = pow(a, 2.0);
    float denom = pow(NdotH, 2.0) * (a2-1.0) + 1.0;
    denom = PI * pow(denom, 2.0);
    return a2/max(denom, 0.0000001);

}

// geometry shadowing function
//schlick-ggx
float geometrySmith(float NdotV, float NdotL, float roughness)
{
    float r = roughness + 1.0;
    float k = pow(r, 2.0) / 8.0;
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
    return ggx1*ggx2;
}

// fresnel function
vec3 fresnel(float HdotV, vec3 baseReflectivity)
{
    return baseReflectivity + (1.0 - baseReflectivity) * pow(1.0-HdotV, 5.0);
}

// Function to calculate tangent space normal from normal map using pre-computed TBN
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(u_NormalMap, texCoord).xyz * 2.0 - 1.0;

    // Use the pre-calculated tangent and bitangent
    vec3 N = normalize(normalInterp);
    vec3 T = normalize(tangentInterp);
    vec3 B = normalize(bitangentInterp);
    
    // Form TBN matrix from pre-computed vectors
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

// Use derivative approach as fallback when no tangents are provided
vec3 getNormalFromMapNoTangent()
{
    vec3 tangentNormal = texture(u_NormalMap, texCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(vertPos);
    vec3 Q2  = dFdy(vertPos);
    vec2 st1 = dFdx(texCoord);
    vec2 st2 = dFdy(texCoord);

    vec3 N   = normalize(normalInterp);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

// Calculate attenuation for point/spot lights
float calculateAttenuation(vec3 lightPos, float range) {
    float distance = length(lightPos - vertPos);
    float attenuation = 1.0;
    
    // Apply attenuation only if it's a point or spot light
    if (range > 0.0) {
        // More physically correct quadratic attenuation with range control
        float rangeSquared = range * range;
        attenuation = clamp(1.0 - (distance * distance) / rangeSquared, 0.0, 1.0);
        attenuation *= attenuation; // Apply squared falloff for smoother transition
    }
    
    return attenuation;
}

// Calculate spot light effect
float calculateSpotEffect(vec3 lightDir, vec3 spotDir, float innerConeAngle, float outerConeAngle) {
    float cosAngle = dot(normalize(-lightDir), normalize(spotDir));
    float cosInner = cos(innerConeAngle);
    float cosOuter = cos(outerConeAngle);
    
    // Smooth edge of spotlight
    return smoothstep(cosOuter, cosInner, cosAngle);
}

void main() {
    // Get material properties from textures or fallback to uniforms
    vec3 albedo = ((flags & ALBEDO_MAP_FLAG) != 0) ? 
                   texture(u_AlbedoMap, texCoord).rgb : 
                   baseColorFactor.rgb;
                   
    float material_roughness = ((flags & ROUGHNESS_MAP_FLAG) != 0) ? 
                               texture(u_RoughnessMap, texCoord).r : 
                               roughnessFactor;
                               
    float material_metallic = ((flags & METALLIC_MAP_FLAG) != 0) ? 
                              texture(u_MetallicMap, texCoord).r : 
                              metallicFactor;
                              
    float ao = ((flags & AO_MAP_FLAG) != 0) ? 
               texture(u_AOMap, texCoord).r : 
               1.0;
               
    vec3 emission = ((flags & EMISSIVE_MAP_FLAG) != 0) ? 
                    texture(u_EmissiveMap, texCoord).rgb : 
                    vec3(0.0);
    
    // Get normal either from normal map or interpolated vertex normal
    vec3 N;
    if ((flags & NORMAL_MAP_FLAG) != 0) {
        // Check if tangent data exists by checking if it's not a zero vector
        if (length(tangentInterp) > 0.01) {
            N = getNormalFromMap();
        } else {
            N = getNormalFromMapNoTangent();
        }
    } else {
        N = normalize(normalInterp);
    }
             
    vec3 V = normalize(camPos - vertPos);
    vec3 visualColor;

    // Debug visualization modes
    if (u_DebugMode == 1) {
        // Just show the base color directly
        visualColor = albedo;
    }
    else if (u_DebugMode == 2) {
        // Show surface normals
        visualColor = (N * 0.5) + 0.5; // Remap from [-1,1] to [0,1]
    }
    else if (u_DebugMode == 3) {
        // Generate a unique color based on object ID (using any unique per-object value)
        // For testing, we'll just use position as a makeshift ID
        float r = fract(sin(dot(floor(vertPos.xyz*10.0), vec3(12.9898, 78.233, 45.164))) * 43758.5453);
        float g = fract(sin(dot(floor(vertPos.xyz*10.0), vec3(39.3465, 23.427, 83.654))) * 34561.5453);
        float b = fract(sin(dot(floor(vertPos.xyz*10.0), vec3(73.9826, 45.337, 27.436))) * 65428.5453);
        visualColor = vec3(r, g, b);
    }
    else {
        // Normal PBR rendering
        vec3 baseReflectivity = mix(vec3(0.04), rgb2lin(albedo), material_metallic);
        vec3 Lo = vec3(0.0); // Total radiance

        float NdotV = max(dot(N, V), 0.0000001);
        
        // Process each light
        for (uint i = 0u; i < lightCount; i++) {
            Light light = lights[i];
            
            vec3 lightPos = light.position.xyz;
            int lightType = int(light.position.w);
            vec3 lightColor = light.color.xyz;
            float lightIntensity = light.color.w;
            float lightRange = light.direction.w;
            vec3 lightDir = vec3(0.0);
            float attenuation = 1.0;
            
            // Calculate light direction and attenuation based on light type
            if (lightType == LIGHT_TYPE_DIRECTIONAL) {
                // Directional light (from direction)
                lightDir = normalize(light.direction.xyz);
                attenuation = 1.0; // No attenuation for directional lights
            }
            else if (lightType == LIGHT_TYPE_POINT) {
                // Point light (from position to fragment)
                lightDir = normalize(lightPos - vertPos);
                attenuation = calculateAttenuation(lightPos, lightRange);
            }
            else if (lightType == LIGHT_TYPE_SPOT) {
                // Spot light (from position to fragment with cone restriction)
                lightDir = normalize(lightPos - vertPos);
                float spotEffect = calculateSpotEffect(lightDir, light.direction.xyz, 
                                                    light.coneAngles.x, light.coneAngles.y);
                attenuation = calculateAttenuation(lightPos, lightRange) * spotEffect;
            }
            
            // Calculate radiance from this light
            vec3 radiance = lightColor * lightIntensity * attenuation;
            
            // Skip calculation if radiance is negligible
            if (length(radiance) < 0.001) continue;
            
            // Standard PBR calculations for this light
            vec3 H = normalize(V + lightDir);
            
            float NdotL = max(dot(N, lightDir), 0.0000001);
            float HdotV = max(dot(H, V), 0.0);
            float NdotH = max(dot(N, H), 0.0);
            
            float D = distributionGGX(NdotH, material_roughness);
            float G = geometrySmith(NdotV, NdotL, material_roughness);
            vec3 F = fresnel(HdotV, baseReflectivity);
            
            vec3 spec = D * G * F;
            spec /= 4.0 * NdotV * NdotL;
            
            vec3 kD = vec3(1.0) - F;
            kD *= 1.0 - material_metallic;
            
            // Add this light's contribution
            Lo += (kD * lin2rgb(albedo) / PI + spec) * radiance * NdotL;
        }

        // Add ambient lighting and emission
        vec3 ambient = vec3(0.03) * albedo * ao;
        
        visualColor = ambient + Lo + emission;

        // HDR tonemapping
        visualColor = visualColor / (visualColor + vec3(1.0));

        // gamma correction
        visualColor = lin2rgb(visualColor);
    }
    outColor = vec4(visualColor, 1.0);
}