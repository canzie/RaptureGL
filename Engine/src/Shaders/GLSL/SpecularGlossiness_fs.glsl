#version 420 core

layout(location = 0) out vec4 outColor;

precision highp float;

in vec3 normalInterp;
in vec3 vertPos;
in vec3 camPos;
in vec2 texCoord;

// Removed hardcoded light
// const vec3 lightPos = vec3(1.25, 1.0, 2.0);

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
    uint lightCount;
    Light lights[MAX_LIGHTS];
};

layout (std140, binding=4) uniform SpecularGlossiness
{
	vec3 diffuse_color;
	float glossiness;
	vec3 specular_color;
    float padding;
};

// Texture samplers
layout(binding = 0) uniform sampler2D u_DiffuseMap;             // ALBEDO=0
layout(binding = 1) uniform sampler2D u_NormalMap;              // NORMAL=1 
layout(binding = 7) uniform sampler2D u_SpecularGlossinessMap;  // SPECULAR=7
layout(binding = 4) uniform sampler2D u_AOMap;                  // AO=4
layout(binding = 5) uniform sampler2D u_EmissiveMap;            // EMISSION=5

// Texture availability flags
uniform bool u_HasDiffuseMap = false;
uniform bool u_HasSpecularGlossinessMap = false;
uniform bool u_HasNormalMap = false;
uniform bool u_HasAOMap = false;
uniform bool u_HasEmissiveMap = false;

// Debug uniform to control visualization mode
uniform int u_DebugMode = 0; // 0=Normal, 1=Diffuse, 2=Normals, 3=ID-based

#define PI 3.1415926535897932384626433832795

// Function to calculate tangent space normal from normal map
vec3 getNormalFromMap()
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
    vec3 diffuseVal = u_HasDiffuseMap ? texture(u_DiffuseMap, texCoord).rgb : diffuse_color;
    float glossinessVal = glossiness;
    vec3 specularVal = specular_color;
    
    if (u_HasSpecularGlossinessMap) {
        vec4 specGloss = texture(u_SpecularGlossinessMap, texCoord);
        specularVal = specGloss.rgb;
        glossinessVal = specGloss.a;
    }
    
    float ao = u_HasAOMap ? texture(u_AOMap, texCoord).r : 1.0;
    vec3 emission = u_HasEmissiveMap ? texture(u_EmissiveMap, texCoord).rgb : vec3(0.0);
    
    // Always calculate these basic values regardless of mode
    vec3 N = u_HasNormalMap ? getNormalFromMap() : normalize(normalInterp);
    vec3 V = normalize(camPos - vertPos);
    vec3 visualColor;

    // Debug visualization modes
    if (u_DebugMode == 1) {
        // Just show the diffuse color directly
        visualColor = diffuseVal;
    }
    else if (u_DebugMode == 2) {
        // Show surface normals
        visualColor = (N * 0.5) + 0.5; // Remap from [-1,1] to [0,1]
    }
    else if (u_DebugMode == 3) {
        // Generate a unique color based on object ID (using any unique per-object value)
        float r = fract(sin(dot(floor(vertPos.xyz*10.0), vec3(12.9898, 78.233, 45.164))) * 43758.5453);
        float g = fract(sin(dot(floor(vertPos.xyz*10.0), vec3(39.3465, 23.427, 83.654))) * 34561.5453);
        float b = fract(sin(dot(floor(vertPos.xyz*10.0), vec3(73.9826, 45.337, 27.436))) * 65428.5453);
        visualColor = vec3(r, g, b);
    }
    else {
        // Standard specular-glossiness rendering
        // Convert glossiness to shininess
        float shininess = exp2(10.0 * glossinessVal + 1.0);
        
        // Ambient term (same for all lights)
        vec3 ambient = diffuseVal * 0.03 * ao;
        
        // Accumulated light contribution
        vec3 Lo = vec3(0.0);
        
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
            
            // Standard specular-glossiness calculations for this light
            vec3 H = normalize(V + lightDir);
            
            // Key reflection angles
            float NdotL = max(dot(N, lightDir), 0.0);
            float NdotV = max(dot(N, V), 0.0);
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);
            
            // Diffuse term (lambertian)
            vec3 diffuse = diffuseVal / PI;
            
            // Specular term (Blinn-Phong with glossiness)
            vec3 specular = specularVal * pow(NdotH, shininess);
            
            // Fresnel approximation
            vec3 fresnel = specularVal + (1.0 - specularVal) * pow(1.0 - VdotH, 5.0);
            
            // Add this light's contribution
            Lo += (diffuse * (1.0 - fresnel) + specular * fresnel) * radiance * NdotL;
        }
        
        visualColor = ambient + Lo + emission;
        
        // HDR tonemapping
        visualColor = visualColor / (visualColor + vec3(1.0));
        
        // gamma correction
        visualColor = pow(visualColor, vec3(1.0/2.2));
    }

    outColor = vec4(visualColor, 1.0);
} 