#version 450 core
#extension GL_ARB_bindless_texture : require // Using bindless for cascade atlases
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_gpu_shader_int64 : require // Needed for uint64_t

layout (location = 0) out vec4 outColor;

// --- G-Buffer Textures --- (Match bindings with PopulateCascade_SSR)
layout (binding = 3) uniform sampler2D gPositionDepth; // World Position (RGB), Linear Depth (A)
layout (binding = 1) uniform sampler2D gNormal;       // World Normal (RGB)
layout (binding = 0) uniform sampler2D gAlbedoSpec; // Albedo (RGB), Specular/Roughness (A)

// --- Cascade Data --- (Matches PopulateCascade_SSR)
struct RadianceCascadeShaderData {
    // Grid Definition
    vec3 gridMinWorldPos;
    vec3 gridCellSizeWorld;
    ivec3 gridDimensions;
    mat4 worldToGridTransform; // Unused here

    float rangeStart;          // Unused here
    float rangeEnd;            // Unused here
    int angularResolution;     // N for NxN oct map

    // Atlas Info
    ivec2 atlasProbeGridDim;   // Unused here
    ivec2 atlasPixelDim;       // Total atlas pixels WxH (Needed!)

    // Camera Info
    mat4 inverseProjectionMatrix; // Unused here
    mat4 inverseViewMatrix;     // Unused here
    mat4 projectionMatrix;      // Unused here
    mat4 viewMatrix;            // Unused here
    vec3 cameraWorldPos;        // Needed!

    // Screen & Ray Params (Unused here)
    int numRayDirections;
    int numStepsPerRay;
    float jitterStrength;

    // New field for bindless texture handle
    uint64_t atlasTextureHandle;
};

layout(std430, binding = 0) readonly buffer CascadeInfoBlock {
    RadianceCascadeShaderData cascades[]; // Unsized array
} cascadeData;

in vec2 TexCoord;


uniform int u_NumCascades;
// Removed u_atlasPixelDim - assuming it's in RadianceCascadeShaderData now
// Removed u_cameraWorldPos - assuming it's in RadianceCascadeShaderData now

uniform int u_screenDimensionsX;
uniform int u_screenDimensionsY;

// --- Constants ---
const float PI = 3.14159265359;
const int MAX_ANGULAR_RES_0 = 8; // Max angular resolution expected for cascade 0 (adjust if needed)

// --- Helper Functions (Adapted from PopulateCascade_SSR & Paper) ---

// Decodes a 2D point on an octahedral map [0, 1] to a 3D direction vector
vec3 octDecode(vec2 f) {
    f = f * 2.0 - 1.0; // Map from [0, 1] to [-1, 1]
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.x += (n.x >= 0.0 ? -t : t);
    n.y += (n.y >= 0.0 ? -t : t);
    return normalize(n);
}

// Encodes a 3D direction vector to a 2D point on an octahedral map [0, 1]
// Based on https://jcgt.org/published/0003/02/01/paper.pdf (Cigolle et al. 2014)
vec2 octEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 encoded = (n.z >= 0.0) ? n.xy : vec2(
        (1.0 - abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0),
        (1.0 - abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0)
    );
    return encoded * 0.5 + 0.5; // Map from [-1, 1] to [0, 1]
}

// Calculate the world position of a probe's center based on its grid coordinates
vec3 getProbeWorldPosition(ivec3 probeGridCoords, RadianceCascadeShaderData cascade) {
    return cascade.gridMinWorldPos + (vec3(probeGridCoords) + 0.5) * cascade.gridCellSizeWorld;
}

// Convert a 3D grid coordinate into its 1D probe index
int gridCoordsToProbeIndex(ivec3 coords, ivec3 gridDimensions) {
    // Clamp coordinates to be safe
    coords = clamp(coords, ivec3(0), gridDimensions - 1);
    return coords.z * (gridDimensions.x * gridDimensions.y) + coords.y * gridDimensions.x + coords.x;
}

// Calculates the global pixel coordinate in the atlas for a given probe and direction
ivec2 getAtlasPixelCoord(int probeIndex1D, vec3 direction, RadianceCascadeShaderData cascade) {
    int resolution = cascade.angularResolution;
    ivec2 atlasDim = ivec2(u_screenDimensionsX, u_screenDimensionsY);

    // Direction to OctCoord [0, 1]
    vec2 octCoordNorm = octEncode(direction);

    // OctCoord to Pixel index within the probe's NxN map [0, N*N - 1]
    ivec2 pixelOffset = ivec2(floor(octCoordNorm * resolution));
    int pixelIndexInProbe = pixelOffset.y * resolution + pixelOffset.x;
    pixelIndexInProbe = clamp(pixelIndexInProbe, 0, resolution * resolution - 1);

    // Global linear index in the atlas
    int probeSize = resolution * resolution;
    int globalLinearIndex = probeIndex1D * probeSize + pixelIndexInProbe;

    // Linear index to 2D atlas coordinates
    ivec2 globalPixelCoord;
    globalPixelCoord.x = globalLinearIndex % atlasDim.x;
    globalPixelCoord.y = globalLinearIndex / atlasDim.x;

    return globalPixelCoord;
}

// Samples the radiance interval (radiance, transparency) from the correct cascade atlas
vec4 sampleCascadeProbeInterval(int cascadeIndex, int probeIndex1D, vec3 direction) {
    RadianceCascadeShaderData cascade = cascadeData.cascades[cascadeIndex];
    ivec2 pixelCoord = getAtlasPixelCoord(probeIndex1D, direction, cascade);

    // Use bindless texture handle
    sampler2D atlasSampler = sampler2D(cascade.atlasTextureHandle);

    // Sample the atlas (rgba16f)
    return texelFetch(atlasSampler, pixelCoord, 0); // LOD 0
}

// Interpolates the radiance interval for a given world position and direction within a cascade
vec4 interpolateCascadeInterval(int cascadeIndex, vec3 worldPos, vec3 direction) {
    RadianceCascadeShaderData cascade = cascadeData.cascades[cascadeIndex];

    // Find cell containing worldPos and interpolation weights
    vec3 relativePos = (worldPos - cascade.gridMinWorldPos) / cascade.gridCellSizeWorld;
    ivec3 baseGridCoords = ivec3(floor(relativePos - 0.5)); // Get base corner grid index
    vec3 weights = fract(relativePos - 0.5); // Trilinear interpolation weights

    vec4 interpolatedValue = vec4(0.0);

    // Trilinear interpolation over 8 neighboring probes
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                ivec3 offset = ivec3(x, y, z);
                ivec3 probeGridCoords = baseGridCoords + offset;

                // Clamp grid coords to valid range
                probeGridCoords = clamp(probeGridCoords, ivec3(0), cascade.gridDimensions - 1);

                int probeIndex1D = gridCoordsToProbeIndex(probeGridCoords, cascade.gridDimensions);
                vec4 probeValue = sampleCascadeProbeInterval(cascadeIndex, probeIndex1D, direction);

                float weight = (offset.x == 1 ? weights.x : 1.0 - weights.x) *
                               (offset.y == 1 ? weights.y : 1.0 - weights.y) *
                               (offset.z == 1 ? weights.z : 1.0 - weights.z);

                interpolatedValue += probeValue * weight;
            }
        }
    }
    return interpolatedValue;
}

// Gets the direction vector for a given index in cascade 0's angular resolution
vec3 getCascade0Direction(int index, int resolution) {
    ivec2 coord2D;
    coord2D.x = index % resolution;
    coord2D.y = index / resolution;
    vec2 octCoordNorm = (vec2(coord2D) + 0.5) / float(resolution);
    return octDecode(octCoordNorm);
}

void main() {
    // 1. Sample G-Buffer
    vec3 worldPos = texture(gPositionDepth, TexCoord).rgb;
    vec3 normal = normalize(texture(gNormal, TexCoord).rgb);
    vec4 albedoSpec = texture(gAlbedoSpec, TexCoord);
    vec3 albedo = albedoSpec.rgb;
    // float roughness = albedoSpec.a;

    // Check for sky or invalid pixels (optional, based on depth/normal encoding)
    if (worldPos == vec3(0.0) && normal == vec3(0.0)) {
        outColor = vec4(0.0); // Or sky color
        return;
    }

    // 2. Merge Cascades (Back-to-Front)
    int finalAngRes = cascadeData.cascades[0].angularResolution;
    int numDirections = finalAngRes * finalAngRes;
    vec4 mergedRadiance[MAX_ANGULAR_RES_0 * MAX_ANGULAR_RES_0]; // Store final (radiance, transparency)

    // Initialize merged data (black, fully transparent)
    for (int j = 0; j < numDirections; ++j) {
        mergedRadiance[j] = vec4(0.0, 0.0, 0.0, 1.0);
    }

    for (int i = u_NumCascades - 1; i >= 0; --i) {
        RadianceCascadeShaderData currentCascade = cascadeData.cascades[i];
        int currentAngRes = currentCascade.angularResolution;
        int currentNumDirections = currentAngRes * currentAngRes;

        for (int j = 0; j < numDirections; ++j) { // Iterate through final resolution directions
            // Get direction for the final resolution
            vec3 direction = getCascade0Direction(j, finalAngRes);

            // Interpolate radiance interval for this cascade at worldPos and direction
            vec4 interval_i = interpolateCascadeInterval(i, worldPos, direction);

            // Merge based on formula: L_accum = L_i + beta_i * L_accum
            mergedRadiance[j].rgb = interval_i.rgb + interval_i.a * mergedRadiance[j].rgb;
            mergedRadiance[j].a *= interval_i.a; // beta_accum = beta_i * beta_accum
        }
    }

    // 3. Calculate Diffuse Irradiance
    vec3 irradiance = vec3(0.0);
    float totalWeight = 0.0;

    for (int j = 0; j < numDirections; ++j) {
        vec3 direction = getCascade0Direction(j, finalAngRes);
        float cosTheta = max(0.0, dot(normal, direction));

        if (cosTheta > 0.0) {
            // Approximating solid angle weight. Assumes uniform distribution over hemisphere.
            // More accurate methods exist (e.g. based on Octahedral mapping distortion)
            // For simplicity, use cosine weight (importance sampling Monte Carlo)
            // float weight = cosTheta; // Or simpler: 1.0 if uniform sampling
             float weight = 1.0;

            irradiance += mergedRadiance[j].rgb * cosTheta * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0) {
         irradiance /= totalWeight; // Normalize if using non-uniform weights
         // If using uniform weights (weight = 1.0), need normalization by solid angle approximation
         // float solidAnglePerSample = 2.0 * PI / float(numDirections); // Crude approximation for hemisphere
         // irradiance *= solidAnglePerSample;
    } else {
        irradiance = vec3(0.0);
    }


    // Lambertian BRDF: L_out = albedo/PI * E
    vec3 indirectDiffuse = albedo * irradiance / PI;

    // Add specular later if needed

    outColor = vec4(indirectDiffuse, 1.0);
}
