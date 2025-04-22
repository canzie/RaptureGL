#version 430 core

// Define the local work group size (e.g., 8x8x1 threads per group)
// Note: For 3D textures, use a 3D local size (e.g., 4x4x4 or 8x8x1)
layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

// Output Image - Binding 0 for simplicity in this example
// In the multi-dispatch C++ loop, this binding would point to the correct cascade's texture
layout(rgba16f, binding = 6) uniform restrict writeonly image3D imgOutput; // Changed to image3D

struct RadianceCascadeShaderData {
    vec3 gridMinWorldPos;
    float rangeStart;
    vec3 gridCellSizeWorld;
    float rangeEnd;
    ivec3 gridDimensions;
    int angularResolution;
    mat4 worldToGridTransform;
    mat4 inverseProjectionMatrix;
    mat4 inverseViewMatrix;
    vec2 screenDimensions;

    int numRayDirections;    // Number of directions to trace per probe
    int numStepsPerRay;      // Max steps for screenspace marching
    float jitterStrength;    // Optional jitter for ray origins/directions
};


layout(std430, binding = 0) readonly buffer CascadeInfoBlock {
    RadianceCascadeShaderData cascades[]; // Unsized array to match the SSBO
} cascadeData;

// Uniform set by C++ before each dispatch to indicate the current cascade
uniform int u_CurrentCascadeIndex;


void main() {
    // Get global invocation ID (index of the probe *within this cascade's grid*)
    ivec3 probeIndex = ivec3(gl_GlobalInvocationID.xyz);

    // Access the data for the current cascade
    RadianceCascadeShaderData currentCascade = cascadeData.cascades[u_CurrentCascadeIndex];
    ivec3 gridDims = currentCascade.gridDimensions;

    // Check if the grid dimensions are valid
    if (gridDims.x <= 0 || gridDims.y <= 0 || gridDims.z <= 0) {
        // Handle invalid dimensions, e.g., store black or skip
        imageStore(imgOutput, probeIndex, vec4(0.0));
        return;
    }

    // Calculate the center of the grid
    vec3 center = vec3(gridDims) / 2.0f;

    // Define the sphere radius (relative to the smallest dimension)
    float radius = min(min(float(gridDims.x), float(gridDims.y)), float(gridDims.z)) * 0.45f; // 45% of the smallest dimension

    // Calculate the distance from the current voxel center to the grid center
    float dist = distance(vec3(probeIndex) + 0.5f, center); // Add 0.5f to use voxel center

    vec4 fragColor;

    // If inside the sphere
    if (dist <= radius) {
        // Calculate normalized coordinates within the grid [0, 1]
        vec3 normCoords = (vec3(probeIndex) + 0.5f) / vec3(gridDims);
        // Use normalized coordinates as color
        fragColor = vec4(normCoords, 1.0);
    } else {
        // Outside the sphere, set to transparent black
        fragColor = vec4(0.0, 0.0, 0.0, 0.0);
    }

    // Write the color to the output 3D image texture
    imageStore(imgOutput, probeIndex, fragColor);
}

