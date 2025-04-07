#version 420 core

layout(location = 0) out vec4 FragColor;

in vec2 TexCoord;

// G-buffer textures
layout(binding = 3) uniform sampler2D u_gPosition;
layout(binding = 1) uniform sampler2D u_gNormal;
layout(binding = 0) uniform sampler2D u_gAlbedo;
layout(binding = 2) uniform sampler2D u_gMaterialProps; // metallic, roughness, ao, unused

// Camera position for specular calculations
uniform vec3 u_CameraPosition;

// Define max light count - must match the C++ side
#define MAX_LIGHTS 8

// Light types
#define LIGHT_TYPE_POINT       0
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_SPOT        2

// Light data structure
struct Light {
    vec4 position;     // xyz = position, w = type (0=point, 1=directional, 2=spot)
    vec4 color;        // xyz = color, w = intensity
    vec4 direction;    // xyz = direction (for spot/directional), w = range
    vec4 coneAngles;   // x = innerConeAngle, y = outerConeAngle (for spot lights)
};

// Light uniform buffer
layout(std140, binding = 2) uniform Lights {
    uint u_LightCount;
    Light u_Lights[MAX_LIGHTS];
};

#define PI 3.14159265359

// PBR calculation functions
// Distribution term - GGX/Trowbridge-Reitz
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return a2 / max(denom, 0.0000001);
}

// Geometry term - Smith's method with GGX
float geometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
    
    return ggx1 * ggx2;
}

// Fresnel term - Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Calculate attenuation for point/spot lights
float calculateAttenuation(vec3 lightPos, vec3 fragPos, float range) {
    float distance = length(lightPos - fragPos);
    float attenuation = 1.0;
    
    // Apply attenuation only if it's a point or spot light with specified range
    if (range > 0.0) {
        // Quadratic attenuation with range control
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
    // Sample G-buffer textures
    vec3 FragPos = texture(u_gPosition, TexCoord).rgb;
    vec3 Normal = texture(u_gNormal, TexCoord).rgb;
    vec4 Albedo = texture(u_gAlbedo, TexCoord);
    vec4 MaterialProps = texture(u_gMaterialProps, TexCoord);
    
    // Extract material properties
    float Metallic = MaterialProps.r;
    float Roughness = MaterialProps.g;
    float AO = MaterialProps.b;
    
    // Ensure normal is normalized
    vec3 N = normalize(Normal);
    
    // View direction
    vec3 V = normalize(u_CameraPosition - FragPos);
    float NdotV = max(dot(N, V), 0.0001);
    
    // Calculate base reflectivity (F0)
    // For metals, base reflectivity is tinted by albedo color
    // For dielectrics, base reflectivity is typically 0.04
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, Albedo.rgb, Metallic);
    
    // Store accumulated lighting
    vec3 Lo = vec3(0.0);
    
    // Calculate lighting contribution from each light
    for (uint i = 0u; i < u_LightCount; i++) {
        Light light = u_Lights[i];
        int lightType = int(light.position.w);
        vec3 lightPos = light.position.xyz;
        vec3 lightColor = light.color.rgb;
        float lightIntensity = light.color.a;
        float lightRange = light.direction.w;
        vec3 lightDir;
        float attenuation = 1.0;
        
        // Calculate light direction and attenuation based on light type
        if (lightType == LIGHT_TYPE_DIRECTIONAL) {
            // Directional light
            lightDir = normalize(light.direction.xyz);
            attenuation = 1.0; // No attenuation for directional lights
        } 
        else if (lightType == LIGHT_TYPE_POINT) {
            // Point light
            lightDir = normalize(lightPos - FragPos);
            attenuation = calculateAttenuation(lightPos, FragPos, lightRange);
        } 
        else if (lightType == LIGHT_TYPE_SPOT) {
            // Spot light
            lightDir = normalize(lightPos - FragPos);
            attenuation = calculateAttenuation(lightPos, FragPos, lightRange);
            
            // Apply spot light cone effect
            attenuation *= calculateSpotEffect(
                lightDir, 
                light.direction.xyz, 
                light.coneAngles.x, 
                light.coneAngles.y
            );
        }
        
        // Skip if light has no effect
        if (attenuation <= 0.001) continue;
        
        // Compute lighting vectors
        vec3 H = normalize(V + lightDir);
        float NdotL = max(dot(N, lightDir), 0.0);
        
        // Skip if no contribution
        if (NdotL <= 0.0001) continue;
        
        // Specular BRDF terms
        float NDF = distributionGGX(N, H, Roughness);
        float G = geometrySmith(NdotV, NdotL, Roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        // Calculate specular term
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * NdotV * NdotL;
        vec3 specular = numerator / max(denominator, 0.0001);
        
        // Energy conservation: light not reflected is refracted (diffuse)
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        
        // Metallic surfaces don't have diffuse lighting
        kD *= 1.0 - Metallic;
        
        // Combine diffuse and specular terms
        vec3 radiance = lightColor * lightIntensity * attenuation;
        Lo += (kD * Albedo.rgb / PI + specular) * radiance * NdotL;
    }
    
    // Add ambient lighting (modulated by AO)
    vec3 ambient = vec3(0.03) * Albedo.rgb * AO;
    vec3 color = ambient + Lo;
    
    // Tone mapping (Reinhard operator)
    color = color / (color + vec3(1.0));
    
    // Apply gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    FragColor = vec4(color, 1.0);
    //FragColor = texture(u_gAlbedo, TexCoord);
}
