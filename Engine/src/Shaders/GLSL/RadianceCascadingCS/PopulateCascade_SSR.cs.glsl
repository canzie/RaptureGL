#version 450 core
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require

// --- Debugging Macros ---
// Set one of these to 1 to enable the corresponding debug visualization
#define DEBUG_VISUALIZE_PROBE_INDEX 0
#define DEBUG_VISUALIZE_PROBE_UV 0
// ------------------------

// Work group size - Adjust based on angular resolution and performance.
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// --- G-Buffer Textures (Read-Only) ---
layout (binding = 3) uniform sampler2D gPositionDepth; // World Position (RGB), Camera Distance/Linear Depth (A)
layout (binding = 1) uniform sampler2D gNormal;       // World Normal (RGB), encoded if necessary
layout (binding = 5) uniform sampler2D gDirectLighting; // Direct Lighting (RGB) from surfaces visible in G-Buffer
layout (binding = 0) uniform sampler2D gAlbedoSpec; // Albedo (RGB), Specular (A)

// --- Output Cascade Atlas ---
// This is a 2D texture where each probe's NxN octahedral map is stored.
layout (binding = 6, rgba16f) uniform restrict writeonly image2D cascadeAtlas;

// --- Cascade Data Structure (Matches C++ struct) ---
struct RadianceCascadeShaderData {
    // Grid Definition
    vec3 gridMinWorldPos; // not used
    vec3 gridCellSizeWorld; // not used
    ivec3 gridDimensions3D; // not used
    mat4 worldToGridTransform; // not used

    float rangeStart;
    float rangeEnd;
    int angularResolution; // N for NxN oct map

    ivec2 gridDimensions;

    // Atlas Info
    ivec2 atlasProbeGridDim; // Num probes WxH in atlas // not used
    ivec2 atlasPixelDim;     // Total atlas pixels WxH

    // Camera Info
    mat4 inverseProjectionMatrix;
    mat4 inverseViewMatrix;
    mat4 projectionMatrix;
    mat4 viewMatrix;
    vec3 cameraWorldPos;

    // Screen & Ray Params
    int numRayDirections; // Total directions = angularResolution * angularResolution
    int numStepsPerRay;
    float jitterStrength;

    uint64_t atlasTextureHandle;

};


// angular
// resolution

// SSBO containing data for all cascades
layout(std430, binding = 0) readonly buffer CascadeInfoBlock {
    RadianceCascadeShaderData cascades[]; // Unsized array
} cascadeData;

// Uniform set by C++ before each dispatch to indicate the current cascade
uniform int u_CurrentCascadeIndex;
uniform int u_screenDimensionsX;
uniform int u_screenDimensionsY;

// --- Constants ---
const float PI = 3.14159265359;
const float TWO_PI = 2.0 * PI;
const float EPSILON = 0.01; // Small offset for ray marching depth comparisons
const vec4 DEFAULT_GBUFFER_VALUE = vec4(1.0, 0.0, 1.0, 1.0); // Default value for out-of-bounds G-Buffer reads

// --- Helper Functions ---

// Decodes a 2D point on an octahedral map [-1, 1]x[-1, 1] to a 3D direction vector
// Based on https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
vec3 octDecode(vec2 f) {
    f = f * 2.0 - 1.0; // Map from [0, 1] to [-1, 1]
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0); // Equivalent to saturate(-n.z)
    n.x += (n.x >= 0.0 ? -t : t);
    n.y += (n.y >= 0.0 ? -t : t);
    return normalize(n);
}

// Maps a global pixel coordinate in the atlas to the 1D probe index and the 1D index
// of the pixel (texel) within that probe's angular map.
// Assumes probes are arranged in a 2D grid within the atlas, defined by atlasProbeGridDim.
// Each probe occupies an angularResolution x angularResolution block of pixels.
ivec2 PixelToProbeAndTexelIndices(ivec2 globalPixelCoord, int angularResolution, ivec2 atlasProbeGridDim) {
    if (angularResolution <= 0) return ivec2(-1, -1); // Avoid division by zero

    // Calculate the 2D grid coordinates of the probe within the atlas
    ivec2 probeAtlasGridCoord = globalPixelCoord / angularResolution;

    // Calculate the linear 1D index of the probe based on its grid position
    // Clamp to ensure it's within the expected dimensions, although ideally should not be necessary
    probeAtlasGridCoord = clamp(probeAtlasGridCoord, ivec2(0), atlasProbeGridDim - 1);
    int probeIndex1D = probeAtlasGridCoord.y * atlasProbeGridDim.x + probeAtlasGridCoord.x;

    // Calculate the 2D coordinates of the texel within the probe's local NxN grid
    ivec2 texelInProbeCoord = globalPixelCoord % angularResolution;

    // Calculate the linear index of the pixel within the probe's map
    int pixelIndexInProbe = texelInProbeCoord.y * angularResolution + texelInProbeCoord.x;

    return ivec2(probeIndex1D, pixelIndexInProbe);
}


// Converts the 1D pixel index within a probe's angular map to normalized 2D octahedral coordinates [0, 1].
vec2 PixelIndexToOctCoordNorm(int pixelIndexInProbe, int angularResolution) {
    if (angularResolution == 0) return vec2(0.5); // Avoid division by zero
    ivec2 coord2D;
    coord2D.x = pixelIndexInProbe % angularResolution;
    coord2D.y = pixelIndexInProbe / angularResolution;
    return (vec2(coord2D) + 0.5) / float(angularResolution);
}

// Calculates the screen coordinate (pixel location) where a screen-space probe is conceptually located.
// Assumes probes form a grid across the screen defined by cascade.gridDimensions.
// probeIndex1D: The linear index of the probe.
// probeGridDim: The dimensions (width, height) of the conceptual probe grid on screen for this cascade.
// screenDimensions: The full resolution of the screen.
ivec2 GetScreenProbeScreenCoord(int probeIndex1D, ivec2 probeGridDim, ivec2 screenDimensions) {
    if (probeGridDim.x == 0 || probeGridDim.y == 0) return screenDimensions / 2; // Avoid division by zero, place in center

    // Calculate the 2D grid coordinates of the probe on screen
    ivec2 probeGridCoord;
    probeGridCoord.x = probeIndex1D % probeGridDim.x;
    probeGridCoord.y = probeIndex1D / probeGridDim.x; // int/int should result in a default floored int?


    vec2 probeSpacing = vec2(screenDimensions) / vec2(probeGridDim);

    // Calculate the screen coordinate, centering the probe within its grid cell
    ivec2 screenCoord = ivec2(vec2(probeGridCoord) * probeSpacing + probeSpacing * 0.5);

    // Clamp to screen bounds just in case
    return clamp(screenCoord, ivec2(0), screenDimensions - 1);
}

// Reconstructs world space position from screen UV and linear depth.
// Assumes depth is linear view space depth (positive Z).
vec3 ReconstructWorldPosFromDepth(vec2 screenUV, float linearDepth, mat4 invProj, mat4 invView) {
    // Convert UV to NDC [-1, 1]
    vec2 ndc = screenUV * 2.0 - 1.0;

    // Create clip space position (assuming far plane at infinity setup or depth already handled)
    // We need W to reconstruct. If linearDepth is view space Z, we can use projection params,
    // but a simpler way is to unproject a point on the near plane and scale by depth.
    // Let's unproject NDC coordinates at near plane (z=-1 in NDC) and far plane (z=1 in NDC)
    vec4 nearClip = vec4(ndc, -1.0, 1.0);
    vec4 farClip = vec4(ndc, 1.0, 1.0);

    vec4 nearView = invProj * nearClip;
    vec4 farView = invProj * farClip;
    nearView /= nearView.w;
    farView /= farView.w;

    // Direction vector in view space
    vec3 viewDir = normalize(farView.xyz - nearView.xyz);

    // Position in view space (depth is positive Z distance)
    // Scale direction by linearDepth (assuming linearDepth is distance along view ray)
    // Origin is camera pos in view space (0,0,0)
    vec3 viewPos = viewDir * linearDepth; // Assumes depth is distance along ray, not just Z
    //viewPos.z = -viewPos.z;
    // Convert to world space
    vec4 worldPos = invView * vec4(viewPos, 1.0);
    return worldPos.xyz;


    /* Alternative using direct Z:
       If linearDepth IS view space Z (usually negative):
       float viewZ = -linearDepth; // Make it negative for typical view space
       vec4 clipPos = vec4(ndc, ???, 1.0); // We don't know clip Z easily from linear view Z
       // Need relationship P[2][2] and P[3][2] from projection matrix
       // clip.z = (P[2][2] * view.z + P[3][2])
       // clip.w = -view.z
       // ndc.z = clip.z / clip.w = -(P[2][2] * view.z + P[3][2]) / view.z
       // This gets complicated. Sticking to ray scaling method above.
    */
}


// Function to safely sample G-Buffer textures, returning default values for invalid UVs
vec4 safeTexture(sampler2D tex, vec2 uv, vec4 defaultValue) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return defaultValue;
    }
    return texture(tex, uv);
}


// Determines the 3D world position origin for a given screen-space probe.
vec3 GetProbeOriginWorldPos(ivec2 probeScreenCoord, ivec2 screenDimensions, sampler2D positionDepthTex, mat4 invProj, mat4 invView) {
    // Convert screen coordinates to UVs [0, 1]
    vec2 probeUV = (vec2(probeScreenCoord) + 0.5) / vec2(screenDimensions);

    // Sample G-Buffer depth at the probe's location
    // Assuming Alpha channel stores linear view depth
    vec4 gbufferSample = safeTexture(positionDepthTex, probeUV, DEFAULT_GBUFFER_VALUE);
    float linearDepth = gbufferSample.a;

    // Handle cases where depth is invalid (e.g., skybox)
    // If depth is near zero or very large, place probe far away or handle appropriately
    if (linearDepth <= 0.0 || linearDepth > 10000.0) { // Adjust max depth threshold as needed
         // Option 1: Place far away along camera forward vector (approximate)
         vec3 viewDir = normalize((invView * vec4(0,0,-1,0)).xyz); // Camera forward in world
         return cascadeData.cascades[u_CurrentCascadeIndex].cameraWorldPos + viewDir * 1000.0;
         // Option 2: Return a sentinel value or handle differently
         // return vec3(0.0); // Might cause issues
    }

    // Reconstruct world position from UV and depth
    return ReconstructWorldPosFromDepth(probeUV, linearDepth, invProj, invView);
}

// Calculates the 3D ray direction vector from normalized octahedral coordinates.
vec3 CalculateRayDirection(vec2 octCoordNorm) {
    return octDecode(octCoordNorm);
}

// Projects world space position to screen UV coordinates [0, 1]
// Returns vec3: xy = UV coords, z = clip space W (positive if in front of camera)
vec3 worldToScreenUV(vec3 worldPos, mat4 view, mat4 proj) {
    vec4 clipPos = proj * view * vec4(worldPos, 1.0);
    // Check if behind camera (clip W <= 0)
    if (clipPos.w <= 0.0) {
        return vec3(-1.0, -1.0, clipPos.w); // Invalid UVs, negative W
    }
    vec3 ndcPos = clipPos.xyz / clipPos.w; // Perspective divide
    vec2 screenUV = ndcPos.xy * 0.5 + 0.5; // Map from [-1, 1] to [0, 1]
    return vec3(screenUV, clipPos.w);
}


// Basic pseudo-random number generator for jitter
// Use invocation ID for seed variation
float random(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}


void main() {
    // --- 1. Identify Target Probe and Output Pixel ---

    // Get data for the current cascade
    RadianceCascadeShaderData cascade = cascadeData.cascades[u_CurrentCascadeIndex];

    // Global invocation ID identifies the unique pixel within the entire atlas this thread works on
    ivec2 globalPixelCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 screenDimensions = ivec2(u_screenDimensionsX, u_screenDimensionsY);

    // Check if this invocation is outside the bounds of the atlas texture
    if (globalPixelCoord.x >= cascade.atlasPixelDim.x || globalPixelCoord.y >= cascade.atlasPixelDim.y) {
       return; // Exit if outside atlas bounds
    }

    // Find which probe and which texel using the updated function
    // Make sure cascade.atlasProbeGridDim is correctly set from C++
    ivec2 probeAndTexel = PixelToProbeAndTexelIndices(globalPixelCoord, cascade.angularResolution, cascade.atlasProbeGridDim);
    int probeIndex1D = probeAndTexel.x;
    int pixelIndexInProbe = probeAndTexel.y;

    // If indices are invalid (e.g., due to zero resolution), exit
    if (probeIndex1D < 0 || pixelIndexInProbe < 0) {
        imageStore(cascadeAtlas, globalPixelCoord, vec4(0.0, 1.0, 1.0, 1.0)); // Cyan for error
        return;
    }

    // --- Debugging Visualizations ---
#if DEBUG_VISUALIZE_PROBE_INDEX == 1
    // Visualize the probe index
    float totalProbes = float(cascade.gridDimensions.x * cascade.gridDimensions.y);
    if (totalProbes > 0.0) {
        float normalizedIndex = float(probeIndex1D) / totalProbes;
        // Simple visualization: use fract to get repeating patterns
        vec3 debugColor = vec3(fract(normalizedIndex * 10.0), fract(normalizedIndex * 5.0), fract(normalizedIndex * 2.0));
        imageStore(cascadeAtlas, globalPixelCoord, vec4(debugColor, 1.0));
    } else {
        imageStore(cascadeAtlas, globalPixelCoord, vec4(1.0, 0.0, 1.0, 1.0)); // Magenta for error (zero probes)
    }
    return;

#elif DEBUG_VISUALIZE_PROBE_UV == 1
    // Visualize the probe's screen UV
    ivec2 probeScreenCoord = GetScreenProbeScreenCoord(probeIndex1D, cascade.gridDimensions, screenDimensions);
    vec2 probeUV = (vec2(probeScreenCoord) + 0.5) / vec2(screenDimensions);
    imageStore(cascadeAtlas, globalPixelCoord, vec4(probeUV, 0.0, 1.0));
    return;

#else
    // --- Normal Execution Path ---
#endif // End of debug/normal path selection

    // --- 2. Determine Probe Origin and Ray Direction ---

    // Get the screen coordinate where this probe is conceptually located
    ivec2 probeScreenCoord = GetScreenProbeScreenCoord(probeIndex1D, cascade.gridDimensions, screenDimensions);

    // Get the 3D world position origin for this probe by reading the G-Buffer
    vec3 probeWorldOrigin = GetProbeOriginWorldPos(probeScreenCoord, screenDimensions, gPositionDepth, cascade.inverseProjectionMatrix, cascade.inverseViewMatrix);

    // Calculate the normalized octahedral coordinates for this specific texel
    vec2 octCoordNorm = PixelIndexToOctCoordNorm(pixelIndexInProbe, cascade.angularResolution);

    // Calculate the 3D direction for the ray associated with this texel
    vec3 rayDirection = CalculateRayDirection(octCoordNorm);

    // --- 3. Perform Screen Space Raymarching ---
    vec3 radiance = vec3(0.0);
    float transparency = 1.0; // Start fully transparent (no hit)

    // Calculate step size based on the cascade's range and number of steps
    float totalRange = cascade.rangeEnd - cascade.rangeStart;
    int numSteps = max(1, cascade.numStepsPerRay); // Ensure at least one step
    float stepSize = totalRange / float(numSteps);

    // Apply jitter to the starting distance (optional but good for reducing banding)
    float jitterOffset = random(vec2(gl_GlobalInvocationID.xy) + u_CurrentCascadeIndex) * stepSize * cascade.jitterStrength;
    float startDist = cascade.rangeStart + jitterOffset;

    // Raymarching loop
    for (int i = 0; i < numSteps; ++i) {
        float currentRayDist = startDist + float(i) * stepSize;

        // Stop if we exceed the cascade's range (shouldn't happen with correct step calc, but good practice)
        if (currentRayDist > cascade.rangeEnd) {
            break;
        }

        // Calculate current sample position in world space
        vec3 currentWorldPos = probeWorldOrigin + rayDirection * currentRayDist;

        // Project this world position to screen space UVs
        vec3 screenPosInfo = worldToScreenUV(currentWorldPos, cascade.viewMatrix, cascade.projectionMatrix);
        vec2 screenUV = screenPosInfo.xy;
        float clipW = screenPosInfo.z;

        // Check if the projected point is outside the screen or behind the camera
        if (screenUV.x < 0.0 || screenUV.x > 1.0 || screenUV.y < 0.0 || screenUV.y > 1.0 || clipW <= 0.0) {
            // Sample is off-screen or behind camera, continue (or break if assuming void outside screen)
            // For simplicity, let's assume empty space and continue the ray
             continue;
            // break; // Alternative: Stop ray if it goes off-screen
        }

        // Sample the depth buffer at the projected screen UV
        // Assuming gPositionDepth.a stores linear view space depth (distance along camera view ray)
        float gBufferDepth = safeTexture(gPositionDepth, screenUV, DEFAULT_GBUFFER_VALUE).a;

        // Calculate the linear view space depth of the current sample point
        vec4 viewPos = cascade.viewMatrix * vec4(currentWorldPos, 1.0);
        // Use absolute value as view Z is typically negative, depth is positive distance
        float currentViewDepth = abs(viewPos.z);

        // Intersection Test: Check if the current ray sample is at or behind the depth stored in the G-Buffer
        if (currentViewDepth >= gBufferDepth - EPSILON) {
            // We hit something according to the G-Buffer!
            // Sample lighting and material properties from the hit point in the G-Buffer
            vec3 directLighting = safeTexture(gDirectLighting, screenUV, DEFAULT_GBUFFER_VALUE).rgb;
            vec4 albedoSpecSample = safeTexture(gAlbedoSpec, screenUV, DEFAULT_GBUFFER_VALUE);
            vec3 albedo = albedoSpecSample.rgb;
            // float specular = albedoSpecSample.a; // Could use this later

            // Basic diffuse reflection model for indirect light contribution
            // More complex BRDFs could be used here.
            radiance = directLighting * albedo; // Simple Lambertian reflection of direct light

            // Mark as opaque hit
            transparency = 0.0;

            // Exit the ray marching loop as we found the first intersection within the range
            break;
        }
    }

    // --- 4. Write Output ---
    // Store accumulated radiance and opacity (1.0 - transparency)
    // Alpha channel stores opacity (1 if empty space/no hit, 0 if opaque hit)
    imageStore(cascadeAtlas, globalPixelCoord, vec4(radiance, 1.0 - transparency));


}