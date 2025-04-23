#version 450 core
#extension GL_ARB_bindless_texture : require // Using bindless for cascade atlases
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_gpu_shader_int64 : require // Needed for uint64_t

// --- Debug View Options ---
// Set one of these to 1 to enable the corresponding debug visualization
#define DEBUG_VIEW_INDIRECT_DIFFUSE 0 // Show only calculated indirect diffuse light
#define DEBUG_VIEW_DIRECT           0 // Show only direct lighting from G-Buffer
#define DEBUG_VIEW_COMBINED         1 // Default: Show combined direct + indirect
#define DEBUG_VIEW_CASCADE_INDEX    -1 // Set index (0 to u_NumCascades-1) to view ONLY that cascade's *interpolated* contribution
#define DEBUG_SHOW_NORMAL           0 // Show world normal
#define DEBUG_SHOW_ALBEDO           0 // Show albedo
// --------------------------

layout (location = 0) out vec4 outColor;

// --- G-Buffer Textures ---
layout (binding = 3) uniform sampler2D gPositionDepth; // World Position (RGB), Linear View Depth (A)
layout (binding = 1) uniform sampler2D gNormal;       // World Normal (RGB) [Encoded 0-1]
layout (binding = 0) uniform sampler2D gAlbedoSpec;   // Albedo (RGB), Roughness/Metallic/Etc (A)
layout (binding = 5) uniform sampler2D gDirectLighting; // Pre-calculated Direct Lighting (RGB)

precision highp float;

// --- Cascade Data --- (Matches C++ version and CS version)
struct RadianceCascadeShaderData {
    // Grid Definition
    vec3 gridMinWorldPos; // Not used in FS sampling typically
    vec3 gridCellSizeWorld; // Not used in FS sampling typically
    ivec3 gridDimensions3D; // Not used in FS sampling typically
    mat4 worldToGridTransform; // Not used in FS sampling typically

    float rangeStart;
    float rangeEnd;
    int angularResolution; // N for NxN oct map

    ivec2 gridDimensions; // Screen-space probe grid WxH for this cascade

    // Atlas Info
    ivec2 atlasProbeGridDim; // Num probes WxH in atlas (Should match gridDimensions for screen-space)
    ivec2 atlasPixelDim;     // Total atlas pixels WxH

    // Camera Info (Potentially useful if needed later)
    mat4 inverseProjectionMatrix;
    mat4 inverseViewMatrix;
    mat4 projectionMatrix;
    mat4 viewMatrix;
    vec3 cameraWorldPos;

    // Screen & Ray Params (Not directly used in FS sampling)
    int numRayDirections;
    int numStepsPerRay;
    float jitterStrength;

    uint64_t atlasTextureHandle; // Bindless handle to the cascade's atlas texture
};

layout(std430, binding = 0) readonly buffer CascadeInfoBlock {
    RadianceCascadeShaderData cascades[]; // Unsized array
} cascadeData;

in vec2 TexCoord; // Fragment's UV coordinate [0,1]

uniform int u_NumCascades; // Total number of cascades available

const float PI = 3.14159265359;
const float INV_PI = 1.0 / PI; // For Lambertian BRDF

// --- Helper Functions ---

// Encodes a 3D direction vector to a 2D point on an octahedral map [0, 1]
// (Assumes input vector `n` is normalized)
vec2 octEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 encoded = (n.z >= 0.0) ? n.xy : vec2(
        (1.0 - abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0),
        (1.0 - abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0)
    );
    return encoded * 0.5 + 0.5;
}

// Decodes a 2D point on an octahedral map [0, 1] to a 3D direction vector
// (Ensures output is normalized)
vec3 octDecode(vec2 f) {
    f = f * 2.0 - 1.0; // Map from [0, 1] to [-1, 1]
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0); // Equivalent to saturate(-n.z)
    n.x += (n.x >= 0.0 ? -t : t);
    n.y += (n.y >= 0.0 ? -t : t);
    // Normalize the result rigorously
    float lenSq = dot(n, n);
    if (lenSq > 0.0) {
        return n / sqrt(lenSq);
    } else {
        return vec3(0.0, 0.0, 1.0); // Fallback for zero vector
    }
}

// Samples the bindless cascade atlas texture for a specific probe and direction.
// probeAtlasGridCoord: 2D integer index of the probe within the atlas grid.
// octCoordNorm: Normalized [0, 1] octahedral coordinates representing the sample direction.
vec4 SampleCascadeAtlas(uint64_t atlasHandle, ivec2 atlasPixelDim, ivec2 probeAtlasGridCoord, int angularResolution, vec2 octCoordNorm) {
    if (angularResolution <= 0 || atlasHandle == 0 || atlasPixelDim.x <= 0 || atlasPixelDim.y <= 0) {
        return vec4(1.0, 0.0, 1.0, 1.0); // Return magenta for error/invalid input
    }

    // Calculate the top-left pixel coordinate of the probe's block in the atlas
    ivec2 probeTopLeftPixel = probeAtlasGridCoord * angularResolution;

    // Calculate the integer pixel offset within the probe's NxN block based on the octCoord
    // Clamp octCoordNorm to avoid potential minor precision issues leading to out-of-bounds access
    ivec2 texelOffset = ivec2(floor(clamp(octCoordNorm, 0.0, 1.0) * angularResolution));
    // Ensure offset doesn't exceed N-1
    texelOffset = min(texelOffset, ivec2(angularResolution - 1));

    // Calculate the final global pixel coordinate in the atlas to sample
    ivec2 samplePixelCoord = probeTopLeftPixel + texelOffset;

    // Convert the integer pixel coordinate to normalized UVs [0, 1] for texture sampling
    // Add 0.5 to sample center of pixel
    vec2 atlasUV = (vec2(samplePixelCoord) + 0.5) / vec2(atlasPixelDim);

    // Perform the bindless texture sample
    // Clamp UVs just in case, although ideally they should be correct
    atlasUV = clamp(atlasUV, 0.0, 1.0);
    return texture(sampler2D(atlasHandle), atlasUV);
}

void main() {
    // 1. Sample G-Buffer
    vec4 posDepthSample = texture(gPositionDepth, TexCoord);
    vec3 worldPos = posDepthSample.rgb;
    vec3 normal = texture(gNormal, TexCoord).rgb; // Assume encoded [0, 1]

    // Decode normal if necessary (adjust if encoding changes)
    normal = normalize(normal * 2.0 - 1.0);

    vec4 albedoSpec = texture(gAlbedoSpec, TexCoord);
    vec3 albedo = albedoSpec.rgb;
    vec3 directLighting = texture(gDirectLighting, TexCoord).rgb;

    // Early out for fragments with invalid normals (e.g., sky) or if no cascades are active
    if (dot(normal, normal) < 0.1 || u_NumCascades <= 0) {
         #if DEBUG_VIEW_DIRECT == 1
             outColor = vec4(directLighting, 1.0);
         #elif DEBUG_VIEW_NORMAL == 1
             outColor = vec4(normal * 0.5 + 0.5, 1.0);
         #elif DEBUG_SHOW_ALBEDO == 1
             outColor = vec4(albedo, 1.0);
         #else // Default combined (effectively direct only here) or indirect (which is 0)
             outColor = vec4(directLighting, 1.0);
         #endif
         return;
    }

    // 2. Calculate Indirect Lighting by Merging Cascades with Interpolation

    vec3 accumulatedIndirectRadiance = vec3(0.0);
    // Sample direction (normal) remains constant for all probe samples within a cascade
    vec2 sampleOctCoords = octEncode(normal);

    // Loop through cascades back-to-front (N-1 down to 0) for correct merging
    for (int i = u_NumCascades - 1; i >= 0; --i) {
        RadianceCascadeShaderData cascade = cascadeData.cascades[i];

        if (cascade.gridDimensions.x <= 0 || cascade.gridDimensions.y <= 0) continue;

        // Calculate the fractional grid coordinates and interpolation weights
        vec2 probeGridSizeF = vec2(cascade.gridDimensions);
        vec2 fragmentGridPosF = TexCoord * probeGridSizeF - vec2(0.5); // Center sampling within grid cell
        ivec2 probeCoord00 = ivec2(floor(fragmentGridPosF));
        vec2 interpWeights = fract(fragmentGridPosF);

        // Get coordinates of the 4 neighboring probes (clamp to grid bounds)
        ivec2 probeCoord10 = clamp(probeCoord00 + ivec2(1, 0), ivec2(0), cascade.gridDimensions - 1);
        ivec2 probeCoord01 = clamp(probeCoord00 + ivec2(0, 1), ivec2(0), cascade.gridDimensions - 1);
        ivec2 probeCoord11 = clamp(probeCoord00 + ivec2(1, 1), ivec2(0), cascade.gridDimensions - 1);
        probeCoord00       = clamp(probeCoord00       , ivec2(0), cascade.gridDimensions - 1); // Clamp the base coord too

        // Sample all 4 probes
        vec4 sample00 = SampleCascadeAtlas(cascade.atlasTextureHandle, cascade.atlasPixelDim, probeCoord00, cascade.angularResolution, sampleOctCoords);
        vec4 sample10 = SampleCascadeAtlas(cascade.atlasTextureHandle, cascade.atlasPixelDim, probeCoord10, cascade.angularResolution, sampleOctCoords);
        vec4 sample01 = SampleCascadeAtlas(cascade.atlasTextureHandle, cascade.atlasPixelDim, probeCoord01, cascade.angularResolution, sampleOctCoords);
        vec4 sample11 = SampleCascadeAtlas(cascade.atlasTextureHandle, cascade.atlasPixelDim, probeCoord11, cascade.angularResolution, sampleOctCoords);

        // Bilinearly interpolate radiance (rgb) and opacity (a)
        vec4 bottomInterpolated = mix(sample00, sample10, interpWeights.x);
        vec4 topInterpolated    = mix(sample01, sample11, interpWeights.x);
        vec4 finalInterpolated  = mix(bottomInterpolated, topInterpolated, interpWeights.y);

        // Extract interpolated radiance and transparency
        vec3 intervalRadiance = finalInterpolated.rgb;
        float intervalTransparency = 1.0 - finalInterpolated.a; // Transparency = 1 - Opacity

        // Merge the interpolated radiance interval
        accumulatedIndirectRadiance = intervalRadiance + intervalTransparency * accumulatedIndirectRadiance;
    }

    // 3. Calculate Final Color (Lambertian Diffuse for Indirect)
    vec3 indirectDiffuse = accumulatedIndirectRadiance * albedo * INV_PI;

    vec3 finalColor = directLighting + indirectDiffuse;

    // --- 4. Apply Debug Views ---
#if DEBUG_VIEW_CASCADE_INDEX >= 0
    int k = DEBUG_VIEW_CASCADE_INDEX;
    if (k >= 0 && k < u_NumCascades) {
         RadianceCascadeShaderData cascade = cascadeData.cascades[k];
         if (cascade.gridDimensions.x > 0 && cascade.gridDimensions.y > 0) {
             vec2 probeGridSizeF = vec2(cascade.gridDimensions);
             vec2 fragmentGridPosF = TexCoord * probeGridSizeF - vec2(0.5);
             ivec2 probeCoord00 = ivec2(floor(fragmentGridPosF));
             vec2 interpWeights = fract(fragmentGridPosF);

             ivec2 probeCoord10 = clamp(probeCoord00 + ivec2(1, 0), ivec2(0), cascade.gridDimensions - 1);
             ivec2 probeCoord01 = clamp(probeCoord00 + ivec2(0, 1), ivec2(0), cascade.gridDimensions - 1);
             ivec2 probeCoord11 = clamp(probeCoord00 + ivec2(1, 1), ivec2(0), cascade.gridDimensions - 1);
             probeCoord00       = clamp(probeCoord00       , ivec2(0), cascade.gridDimensions - 1);

             vec4 sample00 = SampleCascadeAtlas(cascade.atlasTextureHandle, cascade.atlasPixelDim, probeCoord00, cascade.angularResolution, sampleOctCoords);
             vec4 sample10 = SampleCascadeAtlas(cascade.atlasTextureHandle, cascade.atlasPixelDim, probeCoord10, cascade.angularResolution, sampleOctCoords);
             vec4 sample01 = SampleCascadeAtlas(cascade.atlasTextureHandle, cascade.atlasPixelDim, probeCoord01, cascade.angularResolution, sampleOctCoords);
             vec4 sample11 = SampleCascadeAtlas(cascade.atlasTextureHandle, cascade.atlasPixelDim, probeCoord11, cascade.angularResolution, sampleOctCoords);

             vec4 bottomInterpolated = mix(sample00, sample10, interpWeights.x);
             vec4 topInterpolated    = mix(sample01, sample11, interpWeights.x);
             vec4 finalInterpolated  = mix(bottomInterpolated, topInterpolated, interpWeights.y);

             // Show the *interpolated* raw radiance interval (RGB) and opacity (A) for this cascade
             finalColor = finalInterpolated.rgb;
             // If you want to see interpolated opacity use: finalColor = vec3(finalInterpolated.a);
         } else {
              finalColor = vec3(0.0, 1.0, 0.0); // Green: Cascade dimensions invalid
         }
    } else {
        finalColor = vec3(1.0, 0.0, 0.0); // Red: Invalid cascade index requested
    }

#elif DEBUG_VIEW_INDIRECT_DIFFUSE == 1
    finalColor = indirectDiffuse;

#elif DEBUG_VIEW_DIRECT == 1
    finalColor = directLighting;

#elif DEBUG_SHOW_NORMAL == 1
    finalColor = normal * 0.5 + 0.5; // Remap [-1, 1] normal to [0, 1] color

#elif DEBUG_SHOW_ALBEDO == 1
     finalColor = albedo;

#elif DEBUG_VIEW_COMBINED == 1
    // finalColor is already combined direct + indirect from step 3
     finalColor = directLighting + indirectDiffuse; // Explicitly state for clarity

#else
    // Default case if no debug view is selected or multiple are (shouldn't happen with #if/#elif)
    finalColor = directLighting + indirectDiffuse;
#endif

    outColor = vec4(finalColor, 1.0);
}
