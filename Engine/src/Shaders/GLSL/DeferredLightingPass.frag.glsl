#version 420 core

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require // Needed for uint64_t
#extension GL_ARB_shader_storage_buffer_object : require


// Add a debug mode flag at the top
#define DEBUG_SPOTLIGHTS 0
#define MAX_CASCADES 4
#define MAX_SHADOW_CASTERS 4
#define DEBUG_CASCADES 0

// Define the relative width of the blend zone at the end of each cascade
#define CASCADE_BLEND_WIDTH_PERCENT 0.15 // 10% blend width


layout(location = 0) out vec4 FragColor;


in vec2 TexCoord;

// G-buffer textures
layout(binding = 0) uniform sampler2D u_gAlbedo;
layout(binding = 1) uniform sampler2D u_gNormal;
layout(binding = 2) uniform sampler2D u_gMaterialProps; // metallic, roughness, ao
layout(binding = 3) uniform sampler2D u_gPositionDepth; // World Pos (rgb), Linear View Depth (a)

// DDGI textures
layout(binding = 4) uniform sampler2D probeAtlas;         // Irradiance
layout(binding = 5) uniform sampler2D probeDepthAtlas;    // Distance, Distance^2

precision highp float;


// Camera position for specular calculations
uniform vec3 u_CameraPosition;

// Light space matrix for shadow mapping - keeping for backward compatibility
//uniform mat4 u_LightSpaceMatrix;

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

struct ShadowBufferData {
    int type; // (0 = point), 1 = directional, 2 = spot
    uint cascadeCount;
    uint lightIndex; // Index of the light this shadow maps to
    uint64_t textureIDs[MAX_CASCADES];
    mat4 cascadeMatrices[MAX_CASCADES];
    vec4 cascadeSplitsViewSpace[MAX_CASCADES]; // Contains view-space Z split depths in .x component
};

layout(std430, binding = 0) buffer ShadowDataLayout {
    uint shadowCount;
    ShadowBufferData shadowData[];
};

// DDGI probe info
layout(std140, binding = 1) uniform ProbeInfoUBO { // Changed binding to 1
    uvec3 probeGridDimensions;
    uvec2 probeResolution; // Resolution of each probe texture (e.g., 8x8) - Assuming same for both atlases for now
    vec3 probeSpacing;
    vec3 probeOrigin;
} u_ProbeInfo;

// Need probes per row for atlas coord calculation
uniform uint u_probesPerRow;
uniform vec2 u_atlasSize;    // Total pixel dimensions of the probe atlases

#define PI 3.14159265359
#define INFINITY_FLOAT (1.0 / 0.0) // Match compute shader infinity
#define epsilon 0.0001

layout(std140, binding = 3) uniform debugConfig {
    bool debugDDGI;
    bool showDiffuse;
    bool showDirect;
    bool showDirectAmbient;
    bool showFinal;
    bool showDiffuseIntensity;

    // --- New DDGI Debugging Flags ---
    bool debug_DDGI_TrilinearWeightSum;    // Visualize sum of trilinear weights for the 8 probes
    bool debug_DDGI_BackfaceWeightSum;     // Visualize sum of backface weights
    bool debug_DDGI_VisibilityFactorSum;   // Visualize sum of visibility factors
    bool debug_DDGI_FinalProbeWeightSum;   // Visualize sum of final combined probe weights
    bool debug_DDGI_RawIndirectSum;        // Visualize sum of (probeIrradianceContribution * finalProbeWeight) before normalization
    bool debug_DDGI_TotalWeight;           // Visualize the totalWeight variable used for normalization
    bool debug_DDGI_ProbeIrradianceSum;    // Visualize sum of probeIrradianceContribution (radiance * cosine)

} u_debugConfig;


// --- Global DDGI Debugging Variables ---
vec3 g_debug_DDGI_TrilinearWeightSum;
vec3 g_debug_DDGI_BackfaceWeightSum;
vec3 g_debug_DDGI_VisibilityFactorSum;
vec3 g_debug_DDGI_FinalProbeWeightSum;
vec3 g_debug_DDGI_RawIndirectSum;
vec3 g_debug_DDGI_TotalWeight;
vec3 g_debug_DDGI_ProbeIrradianceSum;


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

// Helper function to calculate shadow for a specific cascade - this contains the PCF shadow mapping logic
float calculateShadowForCascade(vec3 fragPosWorld, vec3 normal, vec3 lightDir, ShadowBufferData shadowInfo, 
                              mat4 lightMatrix, int cascadeIndex) {
    // Transform fragment position from world space to light clip space
    vec4 fragPosLightSpace = lightMatrix * vec4(fragPosWorld, 1.0);

    // Perform perspective divide (clip space -> NDC [-1, 1])
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    // Transform to [0,1] range (NDC -> UV coordinates for texture lookup)
    projCoords = projCoords * 0.5 + 0.5;
    
    // Check if fragment is outside the light's view frustum [0, 1] range
    if(projCoords.x < 0.0 || projCoords.x > 1.0 || 
       projCoords.y < 0.0 || projCoords.y > 1.0 || 
       projCoords.z < 0.0 || projCoords.z > 1.0) { // Check Z too
        return 1.0; // Outside frustum = Not shadowed (fully lit)
    }
    
    // Create the appropriate sampler based on cascade count
    float shadowFactor = 0.0;
    vec2 texelSize;
    float bias;

    if (shadowInfo.cascadeCount > 1) {
        // Use texture array for cascaded shadow mapping
        sampler2DArrayShadow shadowMapArray = sampler2DArrayShadow(shadowInfo.textureIDs[0]);
        texelSize = 1.0 / vec2(textureSize(shadowMapArray, 0));
        
        // Apply bias to avoid shadow acne - adjust based on surface angle and cascade level
        float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
        // Progressively reduce bias for farther cascades to reduce light leaking
        float cascadeBiasMultiplier = 1.0 / (1.0 + float(cascadeIndex) * 0.5);
        
        // Use an adaptive bias that scales with distance (for spotlights)
        float distanceScale = 1.0;
        if (shadowInfo.type == 2) { // Spotlight
            // Increase bias with distance to handle perspective distortion
            float viewDepth = abs(fragPosLightSpace.z);
            distanceScale = mix(1.0, 3.0, clamp(viewDepth / 50.0, 0.0, 1.0));
        }
        
        bias = max(0.05 * (1.0 - cosTheta) * distanceScale * cascadeBiasMultiplier, 0.005);
        
        // Use a 3x3 kernel for PCF with the texture array
        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float comparisonDepth = projCoords.z - bias;
                // Use vec4 for sampler2DArrayShadow: vec4(u, v, layer, comparisonValue)
                shadowFactor += texture(shadowMapArray, vec4(
                    projCoords.xy + vec2(x, y) * texelSize,
                    float(cascadeIndex),  // Layer index
                    comparisonDepth
                ));
            }
        }
    } else {
        // Use regular 2D shadow map (backward compatibility)
        sampler2DShadow shadowMapSampler = sampler2DShadow(shadowInfo.textureIDs[0]);
        texelSize = 1.0 / textureSize(shadowMapSampler, 0);

        // Apply bias to avoid shadow acne - adjust based on surface angle
        float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
        
        // Use an adaptive bias that scales with distance (for spotlights)
        float distanceScale = 1.0;
        if (shadowInfo.type == 2) { // Spotlight
            // Increase bias with distance to handle perspective distortion
            float viewDepth = abs(fragPosLightSpace.z);
            distanceScale = mix(1.0, 3.0, clamp(viewDepth / 50.0, 0.0, 1.0));
        }
        
        bias = max(0.005 * (1.0 - cosTheta) * distanceScale, 0.001);
        
        // Use a 3x3 kernel for PCF
        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float comparisonDepth = projCoords.z - bias; 
                shadowFactor += texture(shadowMapSampler, vec3(
                    projCoords.xy + vec2(x, y) * texelSize,
                    comparisonDepth
                ));
            }
        }
    }
    
    shadowFactor /= 9.0; // Average the results
    
    return clamp(shadowFactor, 0.0, 1.0);
}

// Modified to use linear view-space depth from G-buffer
float calculateShadow(vec3 fragPosWorld, float fragDepthView, vec3 normal, vec3 lightDir, ShadowBufferData shadowInfo, out int cascadeIndexOut) { // Added fragDepthView parameter
    cascadeIndexOut = -1; // Default value

    if (shadowInfo.type <= 0) return 1.0; // No shadow for this light or unsupported type

    // NOTE: We now receive fragDepthView directly

    mat4 lightMatrix;
    int cascadeIndex = 0;

    // Check if we're using cascaded shadow mapping
    if (shadowInfo.cascadeCount > 1) {
        // Select cascade based on depth and calculate blend factor
        cascadeIndex = int(shadowInfo.cascadeCount - 1); // Assume farthest initially
        float blendFactor = 0.0;
        int nextCascadeIndex = -1;

        // Loop through the split planes (boundary between cascade i and i+1)
        for (int i = 0; i < int(shadowInfo.cascadeCount - 1); ++i) {
            // Split depth marks the FAR plane of cascade 'i' in view space Z
            // Assuming cascadeSplitsViewSpace.x holds positive linear view Z depth
            float cascadeSplitDepth = shadowInfo.cascadeSplitsViewSpace[i].x;

            // If fragment depth is less than this split depth, it belongs to cascade 'i' or earlier
            if (fragDepthView < cascadeSplitDepth) {
                cascadeIndex = i;

                // Calculate the start depth (NEAR plane) of this cascade in view space Z
                // Assuming positive depths, near plane of first cascade is technically 0? Or camera near plane?
                // Using 0.0 might be problematic if near plane > 0. Check C++ split calculation.
                // Let's assume splits store far planes, so cascade 'i' goes from split[i-1] to split[i]
                float cascadeStartDepth = (i == 0) ? 0.0 : shadowInfo.cascadeSplitsViewSpace[i-1].x;

                // Calculate the size of this cascade's depth range
                float cascadeSize = cascadeSplitDepth - cascadeStartDepth;

                // Avoid division by zero or negative size if splits are invalid
                if (cascadeSize > 0.0001) {
                    // Calculate the absolute size of the blend zone at the end of this cascade
                    float blendZoneSize = cascadeSize * CASCADE_BLEND_WIDTH_PERCENT;

                    // Calculate the start of the blend zone (depth value where blending begins)
                    float blendZoneStart = cascadeSplitDepth - blendZoneSize;

                    // Check if fragment depth is within the blend zone [blendZoneStart, cascadeSplitDepth]
                    if (fragDepthView > blendZoneStart) {
                        // Calculate blend factor: 0 at blendZoneStart, 1 at cascadeSplitDepth
                        blendFactor = (fragDepthView - blendZoneStart) / blendZoneSize;
                        blendFactor = clamp(blendFactor, 0.0, 1.0); // Ensure it's within [0, 1]
                        nextCascadeIndex = i + 1;
                    }
                }

                // Found the primary cascade (and potential blend zone), no need to check further splits
                break;
            }
        }

        cascadeIndexOut = cascadeIndex; // Output the primary cascade index

        // Perform shadow calculation(s) based on whether blending is needed
        if (blendFactor > 0.0 && nextCascadeIndex >= 0 && nextCascadeIndex < int(shadowInfo.cascadeCount)) {
            // Blend between cascadeIndex and nextCascadeIndex
            mat4 lightMatrix1 = shadowInfo.cascadeMatrices[cascadeIndex];
            mat4 lightMatrix2 = shadowInfo.cascadeMatrices[nextCascadeIndex];

            float shadow1 = calculateShadowForCascade(fragPosWorld, normal, lightDir, shadowInfo, lightMatrix1, cascadeIndex);
            float shadow2 = calculateShadowForCascade(fragPosWorld, normal, lightDir, shadowInfo, lightMatrix2, nextCascadeIndex);

            // Linearly interpolate between the two shadow values
            return mix(shadow1, shadow2, blendFactor);
        } else {
            // No blending needed, use only the selected cascadeIndex
            lightMatrix = shadowInfo.cascadeMatrices[cascadeIndex];
            return calculateShadowForCascade(fragPosWorld, normal, lightDir, shadowInfo, lightMatrix, cascadeIndex);
        }

    } else {
        // Non-cascaded shadow (backward compatibility or single cascade setup)
        lightMatrix = shadowInfo.cascadeMatrices[0];
        cascadeIndexOut = 0;
        return calculateShadowForCascade(fragPosWorld, normal, lightDir, shadowInfo, lightMatrix, 0);
    }
}

// DDGI helper functions

// Octahedral encoding for mapping a 3D direction vector to a 2D UV coordinate
// From "A Survey of Octahedral Mappings" and common DDGI implementations
vec2 octEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z)); // Project to octahedron
    vec2 encoded;
    if (n.z >= 0.0) {
        encoded = n.xy;
    } else {
        encoded.x = (1.0 - abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0);
        encoded.y = (1.0 - abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0);
    }
    return encoded * 0.5 + 0.5; // Map from [-1, 1] to [0, 1]
}

// Calculates the UV coordinates within a probe atlas for a given probe and sample direction
vec2 getProbeSpecificAtlasUV(uvec3 probeIndex3D, vec2 sampleDirOctEncodedUV, uvec2 singleProbeResolution) {
    // 1. Convert 3D probe index to linear 1D index
    uint linearIdx = probeIndex3D.z * (u_ProbeInfo.probeGridDimensions.x * u_ProbeInfo.probeGridDimensions.y) +
                     probeIndex3D.y * u_ProbeInfo.probeGridDimensions.x +
                     probeIndex3D.x;

    // 2. Convert linear 1D index to 2D atlas grid coordinates (which probe tile)
    uint atlasGridX = linearIdx % u_probesPerRow;
    uint atlasGridY = linearIdx / u_probesPerRow;

    // 3. Top-left texel of this specific probe's data in the atlas
    vec2 probeTexelOrigin = vec2(atlasGridX * singleProbeResolution.x,
                                 atlasGridY * singleProbeResolution.y);

    // 4. Texel coordinate within this probe based on the octahedral encoded sample direction
    // sampleDirOctEncodedUV is already [0,1], scale by probe resolution
    vec2 localTexelOffset = sampleDirOctEncodedUV * vec2(singleProbeResolution); // Can go up to singleProbeResolution

    // 5. Final texel coordinate in the atlas (center of the texel)
    vec2 atlasTexelCoord = probeTexelOrigin + localTexelOffset;

    // 6. Convert to normalized UV coordinates for texture sampling
    // Add 0.5 to sample texel centers if localTexelOffset was integer-based.
    // Since localTexelOffset can be fractional up to singleProbeResolution,
    // we effectively want to sample across the probe's area.
    // For direct texel lookup, it would be floor(localTexelOffset) + 0.5
    // Given sampleDirOctEncodedUV is continuous [0,1], this directly maps to the probe's normalized space.
    return atlasTexelCoord / u_atlasSize;
}







/*
by looking at the way probes are populated and the format/ordering they take on in the probeatlas, can you help me figure out why it feels like the final irradiance or even the final color on the screen seems to only be affected by a couple of probes and some of these probes have an insane weight, for example the skybox texels in the atlas seem to be super dominant, even in areas where non of the provbes nearby sample from the sun? please find out what is causing this to happen in the calculateDDGIIndirectDiffuse. as for linter errors, just ignore them they are wrong

another observation is that when i increase the sun intensity, and the enitre atlas brightens up, i barely see any difference, only in the non shadowed part, but since iam not using the shadow for now in the atlas, and the probes then correctly become brighter i should see the entire scene light up but nothing changes, why is that?

and finaly check the probe sampling in the deferredlighting shader, be absolutly sure that the correct 8 probes are being sampled from, because 1 slight oops in the logic like lineair vs non linear formatiing in the atlas can make or break the algorithm, maybe even think about a way to debug this

*/

vec3 calculateDDGIIndirectDiffuse(vec3 worldPos, vec3 worldNormal, vec3 viewDir) {
    // Initialize global debug variables
    g_debug_DDGI_TrilinearWeightSum = vec3(0.0);
    g_debug_DDGI_BackfaceWeightSum = vec3(0.0);
    g_debug_DDGI_VisibilityFactorSum = vec3(0.0);
    g_debug_DDGI_FinalProbeWeightSum = vec3(0.0);
    g_debug_DDGI_RawIndirectSum = vec3(0.0);
    g_debug_DDGI_TotalWeight = vec3(0.0);
    g_debug_DDGI_ProbeIrradianceSum = vec3(0.0);

    vec3 totalWeightedIrradiance = vec3(0.0);
    float totalWeightSum = 0.0;

    // Apply normal bias to avoid self-occlusion
    float normalBias = 0.01;
    vec3 biasedWorldPos = worldPos + worldNormal * normalBias;
    
    // Calculate grid dimensions and starting position
    vec3 totalGridVolumeSize = vec3(u_ProbeInfo.probeGridDimensions) * u_ProbeInfo.probeSpacing;
    vec3 minGridCorner = u_ProbeInfo.probeOrigin - 
                        ((vec3(u_ProbeInfo.probeGridDimensions - 1u) / 2.0f) * u_ProbeInfo.probeSpacing);

    // Calculate position relative to grid
    vec3 relativePos = biasedWorldPos - minGridCorner;
    vec3 normalizedPosInGrid = relativePos / u_ProbeInfo.probeSpacing;
    ivec3 baseProbeIndex = ivec3(floor(normalizedPosInGrid));
    vec3 interpolationFactors = fract(normalizedPosInGrid);

    // Iterate through the 8 surrounding probes
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                ivec3 currentProbeOffset = ivec3(i, j, k);
                ivec3 currentProbeIndex3D = baseProbeIndex + currentProbeOffset;

                // Skip probes outside the grid
                if (any(lessThan(currentProbeIndex3D, ivec3(0))) || 
                    any(greaterThanEqual(currentProbeIndex3D, u_ProbeInfo.probeGridDimensions))) {
                    continue;
                }

            // --- 1. Calculate Trilinear Interpolation Weight ---
            float w_x = (i == 0) ? (1.0 - interpolationFactors.x) : interpolationFactors.x;
            float w_y = (j == 0) ? (1.0 - interpolationFactors.y) : interpolationFactors.y;
            float w_z = (k == 0) ? (1.0 - interpolationFactors.z) : interpolationFactors.z;
            float trilinearWeight = w_x * w_y * w_z;            
            g_debug_DDGI_TrilinearWeightSum += vec3(trilinearWeight);
            
            if (trilinearWeight < 0.001 ) continue; // Optimization
            

            // --- Calculate probe's world position (center of the probe) ---
            vec3 probeWorldPos = minGridCorner + (vec3(currentProbeIndex3D) + 0.5) * u_ProbeInfo.probeSpacing;
            vec3 dirToProbe = probeWorldPos - biasedWorldPos; // Vector from shading point to probe
            float distToProbeSq = dot(dirToProbe, dirToProbe);
            float distToProbe = sqrt(distToProbeSq);
            
            if (distToProbe < 0.001) {
                dirToProbe = worldNormal; // Avoid issues if at probe center
            }
            else {
                dirToProbe /= distToProbe;
            }

            // --- 2. Calculate Backface Culling Weight ---
            float dotNormalDirToProbe = dot(worldNormal, dirToProbe);
            float backfaceWeight = smoothstep(0.0, 0.2, dotNormalDirToProbe);
            g_debug_DDGI_BackfaceWeightSum += vec3(backfaceWeight);


            // --- Get UV for this probe's data in the atlas ---
            vec2 probeBaseUV = getProbeSpecificAtlasUV(currentProbeIndex3D, octEncode(dirToProbe), u_ProbeInfo.probeResolution);
            
            vec3 dirFromProbeToShadingPoint = normalize(biasedWorldPos - probeWorldPos);
            if (length(biasedWorldPos - probeWorldPos) < 0.0001) { // If at probe center
                dirFromProbeToShadingPoint = -worldNormal; // Look opposite to surface normal
            }

            vec2 depthMoments = texture(probeDepthAtlas, probeBaseUV).rg;
            float meanDepth = depthMoments.r;
            float meanDepthSq = depthMoments.g;

            
            float visibilityWeight = 1.0;
            if (meanDepth > 0.0) {
                float variance = max(0.0, meanDepthSq - meanDepth * meanDepth);
                float delta = distToProbe - meanDepth;
                if (delta > 0.001) {
                    visibilityWeight = variance / (variance + delta * delta);
                    visibilityWeight = smoothstep(0.0, 1.0, visibilityWeight);
                    //visibilityWeight *= visibilityWeight * visibilityWeight;

                }
            }
            g_debug_DDGI_VisibilityFactorSum += vec3(visibilityWeight);

            // --- 4. Sample Irradiance from the Probe ---
            vec3 probeIrradiance = texture(probeAtlas, probeBaseUV).rgb;
            g_debug_DDGI_ProbeIrradianceSum += probeIrradiance;


            // --- Combine Weights ---
            float combinedWeight = trilinearWeight * backfaceWeight * visibilityWeight;
            g_debug_DDGI_FinalProbeWeightSum += vec3(combinedWeight);

            if (combinedWeight > 0.001) {
                totalWeightedIrradiance += probeIrradiance * combinedWeight;
                totalWeightSum += combinedWeight;
            }


            }
        }

    }

    // Normalize and return final indirect diffuse
    vec3 finalIndirectDiffuse = vec3(0.0);
    if (totalWeightSum > 0.001) {
        finalIndirectDiffuse = totalWeightedIrradiance / totalWeightSum;
    }

    // Populate remaining global debug variables after the loop
    g_debug_DDGI_RawIndirectSum = totalWeightedIrradiance;
    g_debug_DDGI_TotalWeight = vec3(totalWeightSum);

    return finalIndirectDiffuse;
}

// --------------------------------
// Main Shading Logic
// --------------------------------
void main() {
    // Sample G-buffer textures
    vec4 gPosDepth = texture(u_gPositionDepth, TexCoord); // Sample vec4
    vec3 FragPos = gPosDepth.rgb;         // World Position
    float FragDepthView = gPosDepth.a;     // Linear View-Space Z Depth
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
    int debugCascadeIndex = -1; // Store the cascade index for debugging
    
    vec3 kD_direct = vec3(0.0); // kD for direct lighting calculation

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
            lightDirWorld = normalize(lightPos - FragPos); 
            attenuation = calculateAttenuation(lightPos, FragPos, lightRange);
            
            if (lightType == LIGHT_TYPE_SPOT) {
                // Apply spot light cone effect
                attenuation *= calculateSpotEffect(
                    lightDirWorld,            
                    normalize(light.direction.xyz), 
                    light.coneAngles.x, 
                    light.coneAngles.y
                );
            }
        }

        shadowFactor = 1.0; 
        int currentCascadeIndex = -1; 
        for (uint j = 0u; j < shadowCount; j++) {
            ShadowBufferData shadowInfo = shadowData[j];
            if (shadowInfo.lightIndex == i && shadowInfo.type > 0) {
                shadowFactor = calculateShadow(FragPos, FragDepthView, N, lightDirWorld, shadowInfo, currentCascadeIndex);
                if (debugCascadeIndex == -1 && shadowFactor < 1.0) {
                     debugCascadeIndex = currentCascadeIndex;
                }
                break; 
            }
        }
        
#if DEBUG_SPOTLIGHTS
        if (lightType == LIGHT_TYPE_SPOT && attenuation > 0.0) { 
           float spotEffectValue = calculateSpotEffect(lightDirWorld, normalize(light.direction.xyz), light.coneAngles.x, light.coneAngles.y);
           if (spotEffectValue < 0.01) { // Outside
               lightColor = vec3(0.0); // Black
           } else if (spotEffectValue < 0.99) { // Penumbra
                lightColor = vec3(1.0, 0.0, 0.0); // Red
           } else { // Umbra
                lightColor = vec3(0.0, 1.0, 0.0); // Green
           }
           attenuation = 1.0; 
        }
#endif

        // Skip lights with negligible contribution (after shadow and attenuation)
        if (attenuation * shadowFactor < 0.001) continue;
        
        vec3 H = normalize(V + lightDirWorld); 
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
        vec3 specular = numerator / max(denominator, 0.0001); 
        
        vec3 kS = F;
        // kD_direct is the diffuse reflection fraction for direct light
        kD_direct = vec3(1.0) - kS;
        kD_direct *= (1.0 - Metallic);
        
        vec3 radiance = lightColor * lightIntensity * attenuation; 
        Lo += (kD_direct * Albedo.rgb / PI + specular) * radiance * NdotL * shadowFactor; 
    }
    
    // ===============================================
    // --- Indirect Diffuse Lighting (DDGI) Start ---
    // ===============================================
    vec3 indirectDiffuseIntensity = calculateDDGIIndirectDiffuse(FragPos, N, V);
    //vec3 indirectDiffuseIntensity = getIrradiance(FragPos, N, V);

    vec3 kD_indirect = vec3(1.0); 
    vec3 indirectDiffuseContribution = (kD_indirect * Albedo.rgb / PI) * indirectDiffuseIntensity;
    // ===============================================
    // --- Indirect Diffuse Lighting (DDGI) End   ---
    // ===============================================

    // Add ambient lighting (modulated by AO)
    vec3 ambient = vec3(0.02) * Albedo.rgb * AO; // Small base ambient
    vec3 finalColor = Lo + indirectDiffuseContribution;

    // --- New DDGI Debugging Visualizations ---
    if (u_debugConfig.debug_DDGI_TrilinearWeightSum) {
        finalColor = g_debug_DDGI_TrilinearWeightSum;
    } else if (u_debugConfig.debug_DDGI_BackfaceWeightSum) {
        finalColor = g_debug_DDGI_BackfaceWeightSum;
    } else if (u_debugConfig.debug_DDGI_VisibilityFactorSum) {
        finalColor = g_debug_DDGI_VisibilityFactorSum;
    } else if (u_debugConfig.debug_DDGI_FinalProbeWeightSum) {
        finalColor = g_debug_DDGI_FinalProbeWeightSum;
    } else if (u_debugConfig.debug_DDGI_RawIndirectSum) {
        finalColor = g_debug_DDGI_RawIndirectSum;
    } else if (u_debugConfig.debug_DDGI_TotalWeight) {
        finalColor = g_debug_DDGI_TotalWeight;
    } else if (u_debugConfig.debug_DDGI_ProbeIrradianceSum) {
        finalColor = g_debug_DDGI_ProbeIrradianceSum;
    }
    // --- End New DDGI Debugging Visualizations ---
    else if (u_debugConfig.showDiffuseIntensity) { // Added 'else' to chain after new debugs
        finalColor = indirectDiffuseIntensity;
    }
    else if (u_debugConfig.showDiffuse) {
        finalColor = indirectDiffuseContribution;
    }
    else if (u_debugConfig.showDirect) {
        finalColor = Lo;
    }
    else if (u_debugConfig.showDirectAmbient) {
        finalColor = ambient + Lo;
    }
    else if (u_debugConfig.showFinal) {
        finalColor = finalColor;
    }


    finalColor = max(finalColor, vec3(0.0));


#if DEBUG_CASCADES
    // Apply cascade visualization tint if enabled and a cascade was determined
    if (debugCascadeIndex >= 0) {
        vec3 cascadeColorTint = vec3(1.0); // Default: no tint
        if (debugCascadeIndex == 0) cascadeColorTint = vec3(1.0, 0.5, 0.5); // Red tint
        else if (debugCascadeIndex == 1) cascadeColorTint = vec3(0.5, 1.0, 0.5); // Green tint
        else if (debugCascadeIndex == 2) cascadeColorTint = vec3(0.5, 0.5, 1.0); // Blue tint
        else if (debugCascadeIndex == 3) cascadeColorTint = vec3(1.0, 1.0, 0.5); // Yellow tint
        finalColor *= cascadeColorTint;
    }
#endif
    
    // Tone mapping (Reinhard operator)
    finalColor = finalColor / (finalColor + vec3(1.0));
    
    // Apply gamma correction
    finalColor = pow(finalColor, vec3(1.0/2.2));
    
    FragColor = vec4(finalColor, Albedo.a); // Use albedo alpha
}

