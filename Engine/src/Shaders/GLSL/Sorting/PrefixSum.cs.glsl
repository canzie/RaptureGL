#version 450 core

// --- Workgroup Size ---
// Needs to be at least RADIX_SIZE. Power of 2 is good.
// We only need one workgroup as RADIX_SIZE is small (16).
layout(local_size_x = 16, local_size_y = 1, local_size_z = 1) in;

// --- Constants ---
// Must match the RadixSort shader
const uint RADIX_BITS = 4;
const uint RADIX_SIZE = 1 << RADIX_BITS; // 16

// --- Buffers ---

// Input: The global histogram counts (size RADIX_SIZE)
// This would be computed by a previous pass that sums up
// all the localHistograms from the RadixSort shader.
layout(std430, binding = 0) readonly buffer InputHistogramBuffer {
    uint counts[];
} inputHistogram;

// Output: The exclusive prefix sum results (size RADIX_SIZE)
// This will be used as the globalOffsets in the RadixSort shader.
layout(std430, binding = 1) writeonly buffer OutputOffsetsBuffer {
    uint offsets[];
} outputOffsets;

// --- Shared Memory ---
// We need enough space to hold the data for the scan within the workgroup.
// Since the input size is small (16), this is easy.
shared uint sharedScanData[RADIX_SIZE];

// --- Main Logic (Blelloch Scan) ---

void main() {
    uint localId = gl_LocalInvocationID.x;

    // Safety check: Ensure we don't access buffers out of bounds
    // Although with local_size_x = 16 and RADIX_SIZE = 16, this isn't strictly
    // necessary, it's good practice if workgroup size might change.
    if (localId >= RADIX_SIZE) {
        return;
    }

    // --- 1. Load data into Shared Memory ---
    sharedScanData[localId] = inputHistogram.counts[localId];
    barrier(); // Ensure all data is loaded before scan starts

    // --- 2. Up-Sweep (Reduction) Phase ---
    // Reduce the elements in shared memory.
    // After this, sharedScanData[RADIX_SIZE - 1] will hold the total sum.
    for (uint stride = 1; stride < RADIX_SIZE; stride *= 2) {
        uint index = (localId + 1) * stride * 2 - 1;
        if (index < RADIX_SIZE) {
            sharedScanData[index] += sharedScanData[index - stride];
        }
        barrier(); // Synchronize after each step of the reduction
    }

    // --- 3. Clear Last Element for Exclusive Scan ---
    // The last element now holds the total sum. Clear it for the down-sweep.
    if (localId == RADIX_SIZE - 1) {
        sharedScanData[localId] = 0;
    }
    barrier(); // Ensure the last element is cleared before down-sweep

    // --- 4. Down-Sweep Phase ---
    // Propagate the sums downwards to get the exclusive scan result.
    for (uint stride = RADIX_SIZE / 2; stride > 0; stride /= 2) {
        uint index = (localId + 1) * stride * 2 - 1;
        if (index < RADIX_SIZE) {
            uint temp = sharedScanData[index - stride];
            sharedScanData[index - stride] = sharedScanData[index];
            sharedScanData[index] += temp;
        }
        barrier(); // Synchronize after each step of the down-sweep
    }

    // --- 5. Write results to Output Buffer ---
    outputOffsets.offsets[localId] = sharedScanData[localId];
    // No final barrier needed within the shader if it's the last operation.
}
