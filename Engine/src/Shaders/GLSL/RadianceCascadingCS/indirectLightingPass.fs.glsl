#version 450 core
#extension GL_ARB_bindless_texture : require // Using bindless for cascade atlases
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_gpu_shader_int64 : require // Needed for uint64_t

// --- Debug View Options ---
// Uncomment one to isolate a component, otherwise shows combined result.
// #define DEBUG_VIEW_DIFFUSE 1
// #define DEBUG_VIEW_DIRECT 1
// #define DEBUG_VIEW_CASCADE_INDEX 0 // Set index (0 to u_NumCascades-1) to view contribution
// --------------------------

layout (location = 0) out vec4 outColor;

// --- G-Buffer Textures --- (Match bindings with PopulateCascade_SSR)
layout (binding = 3) uniform sampler2D gPositionDepth; // World Position (RGB), Linear Depth (A)
layout (binding = 1) uniform sampler2D gNormal;       // World Normal (RGB)
layout (binding = 0) uniform sampler2D gAlbedoSpec; // Albedo (RGB), Specular/Roughness (A)
layout (binding = 5) uniform sampler2D gDirectLighting; // Direct Lighting (RGB) from surfaces visible in G-Buffer
// layout (binding = 5) uniform sampler2D gDirectLighting; // Direct Lighting (RGB) - Not typically used in indirect pass itself

precision highp float;


// --- Cascade Data --- (Matches PopulateCascade_SSR)
struct RadianceCascadeShaderData {
    // Grid Definition
    vec3 gridMinWorldPos;
    vec3 gridCellSizeWorld;
    ivec3 gridDimensions;
    mat4 worldToGridTransform; // Unused currently

    float rangeStart;          // Unused currently
    float rangeEnd;            // Unused currently
    int angularResolution;     // N for NxN oct map

    // Atlas Info
    ivec2 atlasProbeGridDim;   // Unused currently
    ivec2 atlasPixelDim;       // Total atlas pixels WxH (Needed!)

    // Camera Info
    mat4 inverseProjectionMatrix; // Unused currently
    mat4 inverseViewMatrix;     // Unused currently
    mat4 projectionMatrix;      // Unused currently
    mat4 viewMatrix;            // Unused currently
    vec3 cameraWorldPos;        // Unused currently

    // Screen & Ray Params (Unused currently)
    int numRayDirections;      // angularResolution * angularResolution
    int numStepsPerRay;
    float jitterStrength;

    // New field for bindless texture handle
    uint64_t atlasTextureHandle; // Needed!
};

layout(std430, binding = 0) readonly buffer CascadeInfoBlock {
    RadianceCascadeShaderData cascades[]; // Unsized array
} cascadeData;

in vec2 TexCoord;


uniform int u_NumCascades;
// Assuming screen dimensions are not needed unless doing screen-space effects
// uniform int u_screenDimensionsX;
// uniform int u_screenDimensionsY;

const float PI = 3.14159265359;
const float INV_PI = 1.0 / PI; // For Lambertian BRDF


// --- Helper Functions (Adapted from PopulateCascade_SSR & Paper) ---

// Decodes a 2D point on an octahedral map [0, 1] to a 3D direction vector
vec3 octDecode(vec2 f) {
    f = f * 2.0 - 1.0; // Map from [0, 1] to [-1, 1]
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0); // saturate(-n.z)
    // n.xy += sign(n.xy) * t; // Equivalent but requires GLSL 400+?
    n.x += (n.x >= 0.0 ? -t : t);
    n.y += (n.y >= 0.0 ? -t : t);
    return normalize(n);
}

// Encodes a 3D direction vector to a 2D point on an octahedral map [0, 1]
// Based on https://jcgt.org/published/0003/02/01/paper.pdf (Cigolle et al. 2014)
vec2 octEncode(vec3 n) {
    // Project the sphere onto the octahedron, and then onto the xy plane
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    // If the point is on the upper hemisphere (z >= 0), use the normal projection.
    // If the point is on the lower hemisphere (z < 0), fold the coordinates based on their signs.
    vec2 encoded = (n.z >= 0.0) ? n.xy : vec2(
        (1.0 - abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0), // sign(n.x)
        (1.0 - abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0)  // sign(n.y)
    );
    // Map from [-1, 1] range to [0, 1] range for texture coordinates
    return encoded * 0.5 + 0.5;
}

// Convert 3D grid coordinates to a 1D probe index
int gridCoordsToProbeIndex(ivec3 gridCoords, ivec3 gridDimensions) {
    // Ensure coordinates are clamped (should be done by caller, but safe)
    gridCoords = clamp(gridCoords, ivec3(0), gridDimensions - 1);
    int layerSize = gridDimensions.x * gridDimensions.y; // Probes per Z layer
    return gridCoords.x + gridCoords.y * gridDimensions.x + gridCoords.z * layerSize;
}

// Convert normalized octahedral coordinates [0, 1]x[0, 1] to the 1D pixel index within a probe's angular resolution block
int octCoordNormToPixelIndex(vec2 octNorm, int resolution) {
    // Clamp coords to avoid issues at the 1.0 edge when flooring
    octNorm = clamp(octNorm, 0.0, 1.0 - 1e-6);
    ivec2 angularPixelOffset = ivec2(floor(octNorm * float(resolution)));
    // Clamp just in case, although previous clamp should handle it
    angularPixelOffset = clamp(angularPixelOffset, 0, resolution - 1);
    return angularPixelOffset.y * resolution + angularPixelOffset.x;
}

// Calculate the 2D pixel coordinates in the atlas for a specific probe and pixel within that probe
ivec2 getAtlasPixelCoord(int probeIdx1D, int pixelIdxInProbe, int resolution, ivec2 atlasPixelDim) {
    int angularResolutionSq = resolution * resolution;
    int basePixelLinearIndex = probeIdx1D * angularResolutionSq;
    int pixelLinearIndex = basePixelLinearIndex + pixelIdxInProbe;

    // Calculate 2D coordinates from linear index
    ivec2 pixelCoord;
    pixelCoord.x = pixelLinearIndex % atlasPixelDim.x;
    pixelCoord.y = pixelLinearIndex / atlasPixelDim.x;
    return pixelCoord;
}

// Helper for linear interpolation (lerp)
vec4 mix_vec4(vec4 x, vec4 y, float a) {
    return x * (1.0 - a) + y * a;
}

// Calculate the 3D grid coordinates and interpolation weights (UVW) for a world position
// OUT: baseGridCoords - Integer coordinates of the grid cell minimum corner
// OUT: uvw - Fractional part [0,1] indicating position within the cell
void worldToProbeGridCoordsAndUVW(vec3 worldPos, RadianceCascadeShaderData cascade, out ivec3 baseGridCoords, out vec3 uvw) {
    // Calculate position relative to the grid origin
    vec3 relativePos = worldPos - cascade.gridMinWorldPos;
    // Calculate floating-point grid coordinates by dividing by cell size
    vec3 gridCoordsFloat = relativePos / cascade.gridCellSizeWorld;

    // Integer part is the base coordinate (floor)
    baseGridCoords = ivec3(floor(gridCoordsFloat));
    // Fractional part is the interpolation weight
    uvw = fract(gridCoordsFloat);

    // Clamp base coordinates to ensure they (and base + 1 neighbors) are potentially valid
    // Note: We clamp up to dim-2 because we need base+1 neighbors.
    // If dim=1, clamp to 0. Clamping needs care at grid edges.
    baseGridCoords = clamp(baseGridCoords, ivec3(0), max(ivec3(0), cascade.gridDimensions - 2));
    // For dimensions of size 1, baseCoord will be 0, uvw handles position.
}

// Samples the cascade atlas using trilinear interpolation for a given world position and direction
vec4 sampleCascadeAtlasTrilinear(vec3 worldPos, vec3 omega_s, RadianceCascadeShaderData cascade, sampler2D atlasSampler) {

    ivec3 baseCoords; // Integer coords of the cell min corner (e.g., floor(x), floor(y), floor(z))
    vec3 uvw;         // Interpolation weights (fract(x), fract(y), fract(z))
    worldToProbeGridCoordsAndUVW(worldPos, cascade, baseCoords, uvw);

    // Get angular pixel index for the direction omega_s (same for all 8 probes)
    vec2 octNorm = octEncode(omega_s);
    int pixelIdxInProbe = octCoordNormToPixelIndex(octNorm, cascade.angularResolution);

    // Sample the 8 corner probes of the cell
    vec4 samples[8];
    for (int i = 0; i < 8; ++i) {
        // Calculate neighbor grid coords (offset by 0 or 1 in x,y,z based on i)
        ivec3 neighborOffset = ivec3((i & 1), (i & 2) >> 1, (i & 4) >> 2);
        ivec3 neighborCoords = baseCoords + neighborOffset;

        // Clamp neighbor coords explicitly to handle edge cases where base is near dim-1
        neighborCoords = clamp(neighborCoords, ivec3(0), cascade.gridDimensions - 1);

        // Get probe index and atlas coordinates
        int probeIdx1D = gridCoordsToProbeIndex(neighborCoords, cascade.gridDimensions);
        ivec2 pixelCoord = getAtlasPixelCoord(probeIdx1D, pixelIdxInProbe, cascade.angularResolution, cascade.atlasPixelDim);

        // Sample atlas
        samples[i] = textureLod(atlasSampler, (vec2(pixelCoord) + 0.5) / vec2(cascade.atlasPixelDim), 0.0);
    }

    // Perform trilinear interpolation
    // Lerp along X
    vec4 c00 = mix_vec4(samples[0], samples[1], uvw.x); // lerp(000, 100, u)
    vec4 c10 = mix_vec4(samples[2], samples[3], uvw.x); // lerp(010, 110, u)
    vec4 c01 = mix_vec4(samples[4], samples[5], uvw.x); // lerp(001, 101, u)
    vec4 c11 = mix_vec4(samples[6], samples[7], uvw.x); // lerp(011, 111, u)

    // Lerp along Y
    vec4 c0 = mix_vec4(c00, c10, uvw.y); // lerp(c00, c10, v)
    vec4 c1 = mix_vec4(c01, c11, uvw.y); // lerp(c01, c11, v)

    // Lerp along Z
    return mix_vec4(c0, c1, uvw.z); // lerp(c0, c1, w)
}

// Gets the normalized octahedral coordinates [0,1] corresponding to the center of a pixel index within a probe's angular map
vec2 pixelIndexToOctCoordNorm(int index, int resolution) {
    ivec2 coord2D;
    coord2D.x = index % resolution;
    coord2D.y = index / resolution;
    // Add 0.5 to sample the center of the texel area
    return (vec2(coord2D) + 0.5) / float(resolution);
}

// Gets the world-space sample direction corresponding to a specific sample index,
// using the centers of the angular texels from cascade 0.
vec3 getCascade0SampleDirection(int sampleIndex, int cascade0Resolution) {
    int resSq = cascade0Resolution * cascade0Resolution;
     if (sampleIndex >= resSq) {
        sampleIndex %= resSq; // Wrap if needed, though loop should prevent this
    }
     vec2 octNorm = pixelIndexToOctCoordNorm(sampleIndex, cascade0Resolution);
     return octDecode(octNorm); // Decode the octahedral coords to a 3D direction
}


void main() {
    // 1. Sample G-Buffer
    vec4 posDepthSample = texture(gPositionDepth, TexCoord);
    vec3 worldPos = posDepthSample.rgb;
    // float linearDepth = posDepthSample.a; // Available if needed
    vec3 normal = normalize(texture(gNormal, TexCoord).rgb);
    vec4 albedoSpec = texture(gAlbedoSpec, TexCoord);
    vec3 albedo = albedoSpec.rgb;
    // float roughness = albedoSpec.a; // For specular later
    vec3 directLighting = texture(gDirectLighting, TexCoord).rgb; // Sample direct lighting

    // Basic check for sky/background pixels (adjust threshold as needed)
    // Use normal length as a simple check if background normal is (0,0,0)
    if (dot(normal, normal) < 0.1) {
         discard; // Use discard for simplicity, or set to sky color
         // outColor = vec4(0.0, 0.0, 0.0, 1.0); // Example: Black
         // return;
    }

    // Handle case where there are no cascades
    if (u_NumCascades <= 0) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0); // No indirect light
        return;
    }

    // 2. Calculate Indirect Diffuse Lighting via Monte Carlo Integration
    vec3 totalIndirectDiffuse = vec3(0.0);

    // Determine number of samples based on cascade 0's angular resolution
    // This aligns with the paper's suggestion for efficient diffuse integration.
    int cascade0Resolution = cascadeData.cascades[0].angularResolution;
    int numSamples = cascade0Resolution * cascade0Resolution;

    // Check if cascade 0 has valid resolution
    if (numSamples <= 0) {
        outColor = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }

    for (int s = 0; s < numSamples; ++s) {
        // 2a. Get Sample Direction (using cascade 0 texel centers for distribution)
        vec3 omega_s = getCascade0SampleDirection(s, cascade0Resolution);

        // 2b. Sample Radiance Cascades for this direction and merge results
        vec3 radianceForOmegaS = vec3(0.0);
        float transparencyCascadeChain = 1.0; // Tracks transparency through the cascade chain for merging

        // Loop cascades back-to-front (N-1 down to 0) as per Eq.11 logic (La,c = La,b + beta_a,b * Lb,c)
        for (int i = u_NumCascades - 1; i >= 0; --i) {
            RadianceCascadeShaderData cascade = cascadeData.cascades[i];
            sampler2D atlasSampler = sampler2D(cascade.atlasTextureHandle);

            // Sample the cascade using trilinear interpolation
            vec4 probeSample = sampleCascadeAtlasTrilinear(worldPos, omega_s, cascade, atlasSampler);

            // v. Extract radiance interval (L_i) and transparency (beta_i)
            vec3 L_i = probeSample.rgb;       // Radiance stored in the interval [ti, ti+1]
            float alpha_i = probeSample.a;    // Alpha channel stores (1.0 - transparency)
            float beta_i = 1.0 - alpha_i;     // Transparency of the interval [ti, ti+1]

            // vi. Merge using Eq. 11: La,c = La,b + beta_a,b * Lb,c
            //   La,c is the updated radianceForOmegaS (target interval)
            //   La,b is L_i (current interval's contribution)
            //   beta_a,b is beta_i (current interval's transparency)
            //   Lb,c is the previously accumulated radianceForOmegaS (contribution from farther intervals)
            radianceForOmegaS = L_i + beta_i * radianceForOmegaS;

            // We don't explicitly need transparencyForOmegaS, the radiance merge handles it.
        }

        // 2c. Accumulate cosine-weighted radiance for the diffuse integral
        // N dot L term (L is omega_s here)
        float cosTheta = max(0.0, dot(normal, omega_s));
        totalIndirectDiffuse += radianceForOmegaS * cosTheta;
    }

    // 3. Finalize Diffuse Calculation
    // The loop calculated Sum( L(wi) * cos(theta_i) )
    // To get irradiance E = Integral( L(w) * cos(theta) dw ), we approximate:
    // E approx = Sum( L(wi) * cos(theta_i) ) * delta_omega_i
    // Assuming uniform sampling, delta_omega_i approx = 4*PI / numSamples
    float solidAnglePerSample = 4.0 * PI / float(numSamples);
    vec3 irradiance = totalIndirectDiffuse * solidAnglePerSample;

    // Apply the Lambertian BRDF: Outgoing Radiance = Irradiance * albedo / PI
    vec3 finalIndirectColor = irradiance * albedo * INV_PI;

    // Combine Direct and Indirect Lighting
    vec3 finalColor = directLighting + finalIndirectColor;

    // Clamp result to avoid negative colors
    finalColor = max(finalColor, 0.0);

    #if defined(DEBUG_VIEW_DIFFUSE)
        outColor = vec4(max(finalIndirectColor, 0.0), 1.0); // Show only indirect diffuse
    #elif defined(DEBUG_VIEW_CASCADE_INDEX) && DEBUG_VIEW_CASCADE_INDEX >= 0
        vec3 cascadeDebugColor = vec3(0.0);
        if (DEBUG_VIEW_CASCADE_INDEX < u_NumCascades && numSamples > 0) {
            // Recalculate average contribution from the specified cascade
            // This involves resampling just that cascade's interval using trilinear filtering
            RadianceCascadeShaderData cascade = cascadeData.cascades[DEBUG_VIEW_CASCADE_INDEX];
            sampler2D atlasSampler = sampler2D(cascade.atlasTextureHandle);
            vec3 accumulatedCascadeRadiance = vec3(0.0);

            for (int s = 0; s < numSamples; ++s) {
                vec3 omega_s = getCascade0SampleDirection(s, cascade0Resolution);
                float cosTheta = max(0.0, dot(normal, omega_s));

                // Sample using trilinear interpolation for the debug view as well
                vec4 probeSample = sampleCascadeAtlasTrilinear(worldPos, omega_s, cascade, atlasSampler);
                vec3 L_i = probeSample.rgb;
                // Note: We only care about L_i from this cascade for debug view
                // We multiply by cosTheta here to see its weighted contribution to the integral
                accumulatedCascadeRadiance += L_i * cosTheta;
            }
            // Average the weighted radiance and apply BRDF
            vec3 avgWeightedRadiance = accumulatedCascadeRadiance / float(numSamples);
            // Don't multiply by 4*PI here, just view the average weighted sample radiance * BRDF factor
            cascadeDebugColor = avgWeightedRadiance * albedo * INV_PI;
        }
        outColor = vec4(max(cascadeDebugColor, 0.0), 1.0);
    #elif defined(DEBUG_VIEW_DIRECT)
        outColor = vec4(max(directLighting, 0.0), 1.0); // Show only direct lighting
    #else
        outColor = vec4(finalColor, 1.0); // Show combined direct + indirect
    #endif

    // --- DEBUG OUTPUTS ---
    // outColor = vec4(albedo, 1.0); // Check G-Buffer Albedo
    // outColor = vec4(normal * 0.5 + 0.5, 1.0); // Check G-Buffer Normal
    //if(u_NumCascades > 0) outColor = textureLod(sampler2D(cascadeData.cascades[0].atlasTextureHandle), TexCoord, 0.0); // Check Atlas 0 texture
    //if (numSamples > 0) outColor = vec4(totalIndirectDiffuse / (numSamples * PI), 1.0); // Check average radiance * cosine term
     //if (numSamples > 0) outColor = vec4(irradiance, 1.0); // Check calculated irradiance
}
