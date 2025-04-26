#version 450 core

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_storage_buffer_object : require

// --- Workgroup Size ---
// Must match the value used in the C++ dispatch call
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// --- Constants ---
const uint RADIX_BITS = 4; // Process 4 bits per pass
const uint RADIX_SIZE = 1 << RADIX_BITS; // 2^4 = 16 buckets
const uint BITS_PER_PASS = RADIX_BITS;
const uint TOTAL_BITS = 64; // Sorting uint64_t Morton codes
const uint NUM_PASSES = (TOTAL_BITS + BITS_PER_PASS - 1) / BITS_PER_PASS; // ceil(64 / 4) = 16 passes

// --- Data Structures (Matching C++) ---
struct GpuOutputMortonElement {
    uint64_t mortonCode;
    uint originalTriangleIndex; // Index within its mesh
    uint meshIndex;
};

// --- Buffers ---

// Input for the current pass
layout(std430, binding = 0) readonly buffer InputBuffer {
    GpuOutputMortonElement elements[];
} inputBuffer;

// Output for the current pass
layout(std430, binding = 1) writeonly buffer OutputBuffer {
    GpuOutputMortonElement elements[];
} outputBuffer;

// Contains the global starting offset for each radix value (pre-calculated via prefix sum)
// This needs to be read-write now for atomic increments during scatter.
layout(std430, binding = 2) buffer GlobalOffsets {
    uint offsets[]; // Size = RADIX_SIZE
} globalOffsets;

// --- Uniforms ---

uniform uint u_numElements; // Total number of elements to sort
uniform uint u_pass;        // Current pass number (0 to NUM_PASSES - 1)

// --- Shared Memory ---

// No shared memory needed for this simplified scatter approach


// --- Main Logic ---

void main() {
    uint localId = gl_LocalInvocationID.x;
    uint globalId = gl_GlobalInvocationID.x;
    uint workGroupId = gl_WorkGroupID.x;
    uint workGroupSize = gl_WorkGroupSize.x;

    uint bitOffset = u_pass * BITS_PER_PASS;
    uint64_t mask = (uint64_t(RADIX_SIZE) - 1) << bitOffset;

    // Determine the range of elements this thread should process
    uint elementsPerWorkgroup = (u_numElements + gl_NumWorkGroups.x - 1) / gl_NumWorkGroups.x;
    uint startElement = workGroupId * elementsPerWorkgroup;
    uint endElement = min(startElement + elementsPerWorkgroup, u_numElements);

    // --- Scatter Elements to Output Buffer --- //

    // Each thread processes its assigned elements from the input buffer.
    // It calculates the destination index by atomically incrementing the global offset
    // corresponding to the element's radix value.
    for (uint i = startElement + localId; i < endElement; i += workGroupSize) {
        GpuOutputMortonElement element = inputBuffer.elements[i];
        uint64_t morton = element.mortonCode;
        uint radixValue = uint((morton & mask) >> bitOffset);

        // Atomically increment the global offset for this radix value.
        // The returned value (before the increment) is the correct destination index.
        uint destinationIndex = atomicAdd(globalOffsets.offsets[radixValue], 1);

        // Write the element to its sorted position in the output buffer.
        outputBuffer.elements[destinationIndex] = element;
    }
    // No barriers needed within the shader for this approach.
    // Barriers between dispatches in C++ are still required.
}
