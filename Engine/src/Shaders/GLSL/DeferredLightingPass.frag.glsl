#version 420 core

// Add a debug mode flag at the top
#define DEBUG_SPOTLIGHTS 0

layout(location = 0) out vec4 FragColor;

in vec2 TexCoord;

// G-buffer textures
layout(binding = 3) uniform sampler2D u_gPosition;
layout(binding = 1) uniform sampler2D u_gNormal;
layout(binding = 0) uniform sampler2D u_gAlbedo;
layout(binding = 2) uniform sampler2D u_gMaterialProps; // metallic, roughness, ao

// Shadow map texture (using sampler2DShadow for hardware comparison)
layout(binding = 8) uniform sampler2DShadow u_shadowMap;

// Camera position for specular calculations
uniform vec3 u_CameraPosition;

// Light space matrix for shadow mapping
uniform mat4 u_LightSpaceMatrix;

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
    vec4 coneAngles;   // x = cosInnerConeAngle, y = cosOuterConeAngle (for spot lights)
};

// Light uniform buffer
layout(std140, binding = 2) uniform Lights {
    uint u_LightCount;
    Light u_Lights[MAX_LIGHTS];
};

#define PI 3.14159265359

// --------------------------------
// Lighting calculation functions
// --------------------------------

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
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0); // Added clamp for safety
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


// Calculate spot light cone effect
float calculateSpotEffect(vec3 lightToFrag, vec3 spotDirection, float cosInnerAngle, float cosOuterAngle) {
    // lightToFrag: vector from light to fragment (points toward the fragment)
    // spotDirection: direction the spotlight is pointing (points away from the light)
    
    // Calculate the cosine of the angle between the negative light-to-fragment direction 
    // and the spotlight direction.
    // dot(-lightToFragDir, spotDirection) gives cos(angle between them)
    float cosAngle = dot(-lightToFrag, spotDirection); 
    
#if DEBUG_SPOTLIGHTS
    // For debugging, return a clear visualization
    // 0.0 = outside cone (black)
    // 0.25 = between outer and inner (red)
    // 1.0 = inside inner cone (green)
    if (cosAngle < cosOuterAngle)
        return 0.0; // Outside cone
    else if (cosAngle < cosInnerAngle)
        return 0.25; // Between outer and inner cone
    else
        return 1.0; // Inside inner cone
#else
    // Normal mode
    // Return 0 if outside outer cone
    if (cosAngle < cosOuterAngle) return 0.0;
    
    // Return 1 if inside inner cone
    if (cosAngle > cosInnerAngle) return 1.0;
    
    // Smooth interpolation between outer and inner cone
    // Use smoothstep for a nicer gradient
    return smoothstep(cosOuterAngle, cosInnerAngle, cosAngle);
#endif
}

// --------------------------------
// Shadow mapping functions (Revised for sampler2DShadow)
// --------------------------------

float calculateShadow(vec3 fragPosWorld, vec3 normal, vec3 lightDir) {
    // Transform fragment position from world space to light clip space
    vec4 fragPosLightSpace = u_LightSpaceMatrix * vec4(fragPosWorld, 1.0);
    
    // Perform perspective divide (clip space -> NDC [-1, 1])
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range (NDC -> UV coordinates for texture lookup)
    projCoords = projCoords * 0.5 + 0.5;
    
    // Check if fragment is outside the light's view frustum [0, 1] range
    // (Can help avoid sampling outside the shadow map border)
    if(projCoords.x < 0.0 || projCoords.x > 1.0 || 
       projCoords.y < 0.0 || projCoords.y > 1.0 || 
       projCoords.z < 0.0 || projCoords.z > 1.0) { // Check Z too
        return 1.0; // Outside frustum = Not shadowed (fully lit)
    }
    

    // PCF (Percentage-Closer Filtering) using sampler2DShadow
    float shadowFactor = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0); // textureSize works on sampler2DShadow
    
    // Apply bias to avoid shadow acne - adjust based on surface angle
    float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
    float bias = max(0.05 * (1.0 - cosTheta), 0.005);
    

    // Use smaller sampling kernel for better performance
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            // The Z coordinate for texture() with sampler2DShadow is the depth to compare against.
            // The texture function performs the depth comparison (fragment_depth <= texture_depth)
            // It returns 1.0 if lit (comparison passes), 0.0 if shadowed (comparison fails) for that sample.
            float comparisonDepth = projCoords.z - bias; 
            shadowFactor += texture(u_shadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, comparisonDepth));       
        }
    }
    
    shadowFactor /= 9.0; // Average the results (percentage of samples that are lit)
    
    // shadowFactor is now in the range [0.0, 1.0], where 0.0 is full shadow and 1.0 is fully lit.
    return clamp(shadowFactor, 0.0, 1.0);
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
    
    // Calculate view direction
    vec3 V = normalize(u_CameraPosition - FragPos);
    float NdotV = max(dot(N, V), 0.0001);

    // Base reflectivity for PBR
    vec3 F0 = mix(vec3(0.04), Albedo.rgb, Metallic);
    
    // Initialize total lighting contribution
    vec3 Lo = vec3(0.0);
    
    // Process each light
    for (uint i = 0u; i < u_LightCount; i++) {
        // Get light properties
        Light light = u_Lights[i];
        int lightType = int(light.position.w);
        vec3 lightPos = light.position.xyz;
        vec3 lightColor = light.color.rgb;
        float lightIntensity = light.color.a;
        float lightRange = light.direction.w;
        vec3 lightDirWorld; // Direction from fragment TO the light
        float attenuation = 1.0;
        float shadowFactor = 1.0; // Default: fully lit

        // Calculate light direction and attenuation based on type
        if (lightType == LIGHT_TYPE_DIRECTIONAL) {
            // Directional light: direction is constant, coming FROM the specified direction
            lightDirWorld = normalize(-light.direction.xyz); // Negate to get vector towards light source
            attenuation = 1.0; // No distance attenuation
        }
        else { // Point or Spot light
            lightDirWorld = normalize(lightPos - FragPos); // Direction from fragment to light
            attenuation = calculateAttenuation(lightPos, FragPos, lightRange);
            
            if (lightType == LIGHT_TYPE_SPOT) {
                // Apply spot light cone effect
                attenuation *= calculateSpotEffect(
                    lightDirWorld,             // Use normalized direction TO the light
                    normalize(light.direction.xyz), // Ensure spot direction is normalized
                    light.coneAngles.x, 
                    light.coneAngles.y
                );
            }
        }
        
        // Calculate shadow only for the first light (assuming light 0 is the shadow caster)
        if (i == 0u) { 
            // Note: lightDirWorld points TOWARDS the light source.
            // calculateShadow might need the direction FROM the light source depending on bias calculation.
            // Let's pass the direction towards the light source as standard practice for lighting calcs.
            shadowFactor = calculateShadow(FragPos, N, lightDirWorld);
        }
        
#if DEBUG_SPOTLIGHTS
        // Debug visualization for spotlight remains the same, check attenuation
        if (lightType == LIGHT_TYPE_SPOT && attenuation > 0.0) { 
           float spotEffectValue = calculateSpotEffect(lightDirWorld, normalize(light.direction.xyz), light.coneAngles.x, light.coneAngles.y);
           if (spotEffectValue < 0.01) { // Outside
               lightColor = vec3(0.0); // Black
           } else if (spotEffectValue < 0.99) { // Penumbra
                lightColor = vec3(1.0, 0.0, 0.0); // Red
           } else { // Umbra
                lightColor = vec3(0.0, 1.0, 0.0); // Green
           }
           attenuation = 1.0; // Override attenuation for debug viz
        }
#endif

        // Skip lights with negligible contribution (after shadow and attenuation)
        if (attenuation * shadowFactor < 0.001) continue;
        
        // Compute lighting vectors
        vec3 H = normalize(V + lightDirWorld); // Halfway vector
        float NdotL = max(dot(N, lightDirWorld), 0.0);

        // Skip if light is behind the surface
        if (NdotL <= 0.0) continue;
        
        // Specular BRDF terms
        float NDF = distributionGGX(N, H, Roughness);
        float G = geometrySmith(NdotV, NdotL, Roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        // Calculate specular term
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * NdotV * NdotL;
        vec3 specular = numerator / max(denominator, 0.0001); // Add epsilon
        
        // Energy conservation: light not reflected is refracted (diffuse)
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        
        // Metallic surfaces don't have diffuse lighting
        kD *= (1.0 - Metallic);
        
        // Calculate final light contribution for this light source
        // Modulate by NdotL, attenuation, and shadow factor
        vec3 radiance = lightColor * lightIntensity * attenuation; // Base radiance
        Lo += (kD * Albedo.rgb / PI + specular) * radiance * NdotL * shadowFactor; // Apply NdotL and shadowFactor here
    }
    
    // Add ambient lighting (modulated by AO)
    vec3 ambient = vec3(0.03) * Albedo.rgb * AO;
    vec3 finalColor = ambient + Lo;
    
    // Tone mapping (Reinhard operator)
    finalColor = finalColor / (finalColor + vec3(1.0));
    
    // Apply gamma correction
    finalColor = pow(finalColor, vec3(1.0/2.2));
    
    FragColor = vec4(finalColor, Albedo.a); // Use albedo alpha
}
