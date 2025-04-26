#version 450 core

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_storage_buffer_object : require


// --- Workgroup Size ---
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// --- Constants ---
// Must match the RadixSort shader
const uint RADIX_BITS = 4;
const uint RADIX_SIZE = 1 << RADIX_BITS; // 16
const uint BITS_PER_PASS = RADIX_BITS;

// --- Data Structures ---
struct GpuOutputMortonElement {
    uint64_t mortonCode;
    uint originalTriangleIndex;
    uint meshIndex;
};

// --- Buffers ---

// Input Morton codes for the current pass
layout(std430, binding = 0) readonly buffer InputBuffer {
    GpuOutputMortonElement elements[];
} inputBuffer;

// Output global histogram (atomic counters)
// Must be cleared to zero before each pass
layout(std430, binding = 1) buffer GlobalHistogramBuffer {
    uint counts[]; // Size = RADIX_SIZE
} globalHistogram;

// --- Uniforms ---
uniform uint u_numElements; // Total number of elements to sort
uniform uint u_pass;        // Current pass number (0 to NUM_PASSES - 1)

// --- Main Logic ---
void main() {
    uint globalId = gl_GlobalInvocationID.x;
    uint bitOffset = u_pass * BITS_PER_PASS;
    uint64_t mask = (uint64_t(RADIX_SIZE) - 1) << bitOffset;

    // Each thread processes one element and atomically increments the corresponding histogram bin
    if (globalId < u_numElements) {
        uint64_t morton = inputBuffer.elements[globalId].mortonCode;
        uint radixValue = uint((morton & mask) >> bitOffset);

        // Ensure radixValue is within bounds (although mask should guarantee this)
        if (radixValue < RADIX_SIZE) {
            atomicAdd(globalHistogram.counts[radixValue], 1);
        }
    }
}
