#version 450 core
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require

// Work group size - Adjust based on angular resolution and performance.
// Aim for local_size_x * local_size_y to roughly match angularResolution * angularResolution if possible,
// but keep total size (x*y*z) reasonable (e.g., 64, 128, 256).
// Example: For 8x8 angular resolution, could use 8x8x1. For 16x16, maybe 8x8x4 or 16x16x1.
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
    vec3 gridMinWorldPos;
    vec3 gridCellSizeWorld;
    ivec3 gridDimensions;
    mat4 worldToGridTransform; // May not be needed for SSR

    float rangeStart;
    float rangeEnd;
    int angularResolution; // N for NxN oct map

    // Atlas Info
    ivec2 atlasProbeGridDim; // Num probes WxH in atlas
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
const float EPSILON = 0.1; // Small offset for ray marching depth comparisons, adjust based on scene scale/depth precision

// --- Helper Functions ---

// Decodes a 2D point on an octahedral map [-1, 1]x[-1, 1] to a 3D direction vector
// Based on https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
vec3 octDecode(vec2 f) {
    f = f * 2.0 - 1.0; // Map from [0, 1] to [-1, 1]
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0); // Equivalent to saturate(-n.z)
    // n.xy += (n.xy >= 0.0 ? -t : t); // Invalid GLSL
    // Apply the sign-based offset component-wise
    n.x += (n.x >= 0.0 ? -t : t);
    n.y += (n.y >= 0.0 ? -t : t);
    return normalize(n);
}

// based on angular resolution we calculate the current probe index, given the pixel coordinate of the atlas
// the pixels should for each probe, be layed out next to each other, so not in a grid form
// this helps reduce the minimum atlas texture resolution needed
// OUT: index of the probe e.g. (1, 1, 0), (12, 7, 5)
//      this will help us identify the probes origin
ivec2 PixelToProbeIndex(ivec2 pixelCoord, int resolution, ivec2 atlasDimensions) {
    // angular resolution squared is amount of pixels per probe
    // to get the start we do

    int globalLinearIndex = pixelCoord.y * atlasDimensions.x + pixelCoord.x;

    float probeSize = resolution * resolution;
    float probeIndex = globalLinearIndex / probeSize;
    int relativeProbeIndex = int(floor(probeIndex));
    float fractionalPart = probeIndex - float(relativeProbeIndex);
    int pixelIndexInProbe = int(floor(fractionalPart * probeSize));

    return ivec2(relativeProbeIndex, pixelIndexInProbe);
}

// goes from the relative 1d index to a relative 2d index in the probe
vec2 pixelIndexToOctCoordNorm(int index, int resolution) {
    ivec2 coord2D;
    coord2D.x = index % resolution;
    coord2D.y = index / resolution;
    return (vec2(coord2D) + 0.5) / float(resolution);
}

// Projects world space position to screen UV coordinates [0, 1]
// Returns vec3: xy = UV coords, z = clip space W (positive if in front of camera)
vec3 worldToScreenUV(vec3 worldPos, mat4 view, mat4 proj) {
    vec4 clipPos = proj * view * vec4(worldPos, 1.0);
    vec3 ndcPos = clipPos.xyz / clipPos.w; // Perspective divide
    vec2 screenUV = ndcPos.xy * 0.5 + 0.5; // Map from [-1, 1] to [0, 1]
    return vec3(screenUV, clipPos.w);
}

// Calculate the world position of a probe's center based on its grid coordinates
vec3 getProbeWorldPosition(ivec3 probeGridCoords, RadianceCascadeShaderData cascade) {
    // Add 0.5 to center the probe origin within its grid cell
    return cascade.gridMinWorldPos + (vec3(probeGridCoords) + 0.5) * cascade.gridCellSizeWorld;
}

// Convert a 1D probe index into its 3D grid coordinates within the cascade
ivec3 probeIndexToGridCoords(int probeIdx, ivec3 gridDimensions) {
    ivec3 coords;
    int layerSize = gridDimensions.x * gridDimensions.y;
    coords.z = probeIdx / layerSize;
    int indexInLayer = probeIdx % layerSize;
    coords.y = indexInLayer / gridDimensions.x;
    coords.x = indexInLayer % gridDimensions.x;
    // Clamp coordinates to be safe, though probeIdx should always be valid
    coords = clamp(coords, ivec3(0), gridDimensions - 1);
    return coords;
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
    
    if (globalPixelCoord.x >= screenDimensions.x || globalPixelCoord.y >= screenDimensions.y) {
        return;
    }



    ivec2 indices = PixelToProbeIndex(globalPixelCoord, cascade.angularResolution, cascade.atlasPixelDim);
    int probeIdx1D = indices.x;
    int pixelIdxInProbe = indices.y;

    vec2 octCoordNorm = pixelIndexToOctCoordNorm(pixelIdxInProbe, cascade.angularResolution);
    vec3 rayDirection = octDecode(octCoordNorm);

    // calculate the probe world position
    ivec3 probeGridCoords = probeIndexToGridCoords(probeIdx1D, cascade.gridDimensions);
    vec3 probeWorldPos = getProbeWorldPosition(probeGridCoords, cascade);


    vec3 radiance = vec3(0.0);
    float transparency = 1.0;
    // ray loop
    float totalRange = cascade.rangeEnd - cascade.rangeStart;
    float stepSize = totalRange / float(max(1, cascade.numStepsPerRay));
    float startDist = cascade.rangeStart;

    for (int i = 0; i < cascade.numStepsPerRay; i++) {
        float currentRayDist = startDist + float(i) * stepSize;
        
        vec3 currentWorldPos = probeWorldPos + rayDirection * currentRayDist;


        vec3 screenPos = worldToScreenUV(currentWorldPos, cascade.viewMatrix, cascade.projectionMatrix);
        vec2 screenUV = screenPos.xy;
        float clipW = screenPos.z; // If <= 0, the point is behind or on the camera's near plane


        if (clipW <= 0.0) {
            break;
        }


        float gBufferDepth = texture(gPositionDepth, screenUV).a;
        vec4 viewPos = cascade.viewMatrix * vec4(currentWorldPos, 1.0);
        float currentViewDepth = -viewPos.z;

        vec3 normal = texture(gNormal, screenUV).rgb;
        float eps = EPSILON * abs(dot(rayDirection, normal));

        if (currentViewDepth >= gBufferDepth - eps) {
                // Sample direct lighting from the G-buffer surface we hit
                vec3 directLighting = texture(gDirectLighting, screenUV).rgb;
                vec3 albedo = texture(gAlbedoSpec, screenUV).rgb;

                radiance = directLighting * albedo;
                // Set transparency to indicate an opaque hit
                transparency = 0.0;

                // Exit the ray marching loop as we found the first intersection
                break;
        }
        
        
    }



    //vec4 debugColor = vec4(1.0, 0.0, 0.0, 1.0);
    //imageStore(cascadeAtlas, globalPixelCoord, debugColor);






    // --- 5. Write Output ---
    imageStore(cascadeAtlas, globalPixelCoord, vec4(radiance, 1.0-transparency));
    
}