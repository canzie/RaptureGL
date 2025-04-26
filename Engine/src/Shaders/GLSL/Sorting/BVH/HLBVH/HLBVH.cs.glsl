#version 460 core
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_storage_buffer_object : require

// --- Workgroup Size ---
// Should be tuned. A power of 2 is usually good.
// Must be >= (1 << p) if using shared memory for block descriptor traversal efficiently.
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

// --- Data Structures ---

// Matches RadixSort/MortonCode output
struct GpuOutputMortonElement {
    uint64_t mortonCode;
    uint originalTriangleIndex; // Index within its mesh
    uint meshIndex;
};

// BVH Node Structure (Focus on Hierarchy)
// primitiveCount > 0 indicates a leaf node.
// For leaves: leftChild = firstPrimitiveIndex, rightChild = 0xFFFFFFFF
// For internal: leftChild/rightChild point to node indices.
struct BVHNode {
    uint parent;         // Index of the parent node
    uint leftChild;      // Left child index or first primitive index
    uint rightChild;     // Right child index or 0xFFFFFFFF
    uint primitiveCount; // 0 for internal nodes, > 0 for leaf nodes
    // AABB data would go here in a full implementation
};

// --- Uniforms ---
uniform uint u_level;       // Starting bit level for this pass (e.g., 0, p, 2p, ...)
uniform uint u_p;           // Number of bit levels to process in this pass (e.g., 3)
uniform uint u_numSegmentsIn; // Number of segments (leaves from previous pass) = number of work groups to dispatch

// --- Buffers ---

// Input: Sorted Morton codes (needed only for primitive count if not provided separately)
// layout(std430, binding = 0) readonly buffer SortedMortonCodes {
//     GpuOutputMortonElement elements[];
// } sortedMortonCodes;

// Output: BVH nodes (read/write as parent pointers are updated)
layout(std430, binding = 1) buffer Nodes {
    BVHNode nodes[];
} bvhNodes;

// Input: Block descriptors computed in a prior step. Size = u_numSegmentsIn * P, where P = (1 << u_p) - 1.
// Contains relative split indices within each segment, or -1 if no split.
layout(std430, binding = 2) readonly buffer BlockSplits {
    int splits[]; // Use int to store -1 easily
} blockSplits;

// Input: Offsets for allocating nodes for each segment's treelet. Size = u_numSegmentsIn + 1.
// blockOffsets[s] = starting node index offset for segment s treelet.
layout(std430, binding = 3) readonly buffer BlockOffsets {
    uint offsets[];
} blockOffsets;

// Input: Index of the first primitive for each incoming segment. Size = u_numSegmentsIn.
layout(std430, binding = 4) readonly buffer SegmentHeadsIn {
    uint heads[];
} segmentHeadsIn;

// Input: Number of primitives in each incoming segment. Size = u_numSegmentsIn.
layout(std430, binding = 5) readonly buffer NumPrimitivesInSegment {
    uint counts[];
} numPrimitivesInSegment;

// Input: Mapping from incoming segment head index -> node index in 'nodes' buffer. Size = N (sparse).
// This node is the parent for the treelet being generated.
layout(std430, binding = 6) readonly buffer HeadToNodeIn {
    uint map[]; // Use a compact map or hash table if N is too large
} headToNodeIn;

// Output: Segment heads for the *next* pass. Size >= u_numSegmentsIn. Needs atomic counter.
layout(std430, binding = 7) writeonly buffer SegmentHeadsOut {
    uint heads[];
} segmentHeadsOut;

// Output: Head-to-node map for the *next* pass. Size >= u_numSegmentsIn. Matches SegmentHeadsOut.
layout(std430, binding = 8) writeonly buffer HeadToNodeOut {
    uint map[];
} headToNodeOut;

// --- Atomic Counters ---
// Provided by the C++ side, bound appropriately.
// Counter for allocating new node indices.
layout(binding = 0, offset = 0) uniform atomic_uint atomicNodeCounter;
// Counter for allocating space in SegmentHeadsOut/HeadToNodeOut.
layout(binding = 0, offset = 4) uniform atomic_uint atomicNewSegmentCounter; // Assuming 4-byte alignment

// --- Shared Memory ---
// Store the block descriptor for the current segment. Size = P = (1 << p) - 1.
// Requires p to be reasonably small (e.g., p <= 6 for 63 entries * 4 bytes = 252 bytes).
shared int sharedBlockSplits[64]; // Max P for p=6. Adjust if p is larger.

// --- Helper Functions ---

// Calculates the index into the flat blockSplits array for a given segment,
// treelet level (0 to p-1), and path offset within that level.
uint getBlockSplitIndex(uint segmentIdx, uint levelInTreelet, uint offsetInLevel, uint p) {
    // P = (1 << p) - 1
    // Total indices before this level = (1 << levelInTreelet) - 1
    uint baseIndex = (1 << levelInTreelet) - 1;
    uint P = (1 << p) - 1;
    return segmentIdx * P + baseIndex + offsetInLevel;
}

// --- Main Logic ---
// Each workgroup processes one segment from the previous pass.
void main() {
    uint segmentIndex = gl_WorkGroupID.x;
    if (segmentIndex >= u_numSegmentsIn) {
        return;
    }

    uint localId = gl_LocalInvocationID.x;
    uint workGroupSize = gl_WorkGroupSize.x;

    // --- Load Block Descriptor into Shared Memory ---
    uint P = (1 << u_p) - 1; // Number of potential splits in the descriptor
    for (uint i = localId; i < P; i += workGroupSize) {
        uint flatIndex = segmentIndex * P + i;
        sharedBlockSplits[i] = blockSplits.splits[flatIndex];
    }
    barrier(); // Ensure all threads have loaded their part

    // --- Segment Info ---
    uint firstPrimitiveAbs = segmentHeadsIn.heads[segmentIndex];
    uint numPrimsInSegment = numPrimitivesInSegment.counts[segmentIndex];
    // Find the node index that is the parent/root for this segment's treelet
    // This requires accessing the sparse headToNodeIn map using the absolute primitive index
    // This access pattern might be slow if N is very large. Consider alternatives C++ side.
    uint parentNodeIndex = headToNodeIn.map[firstPrimitiveAbs];


    // --- Check if Segment Splits Further ---
    // Thread 0 checks the root split (index 0 in the shared descriptor).
    // If sharedBlockSplits[0] == -1, no splits occur in these p bits for this segment.
    bool splitsFurther = (sharedBlockSplits[0] != -1);
    // A more robust check would count *all* valid splits in the descriptor.
    // Let's assume for now if the root doesn't split, nothing below it does either
    // (This implies the morton codes in the segment are identical in these p bits).


    // --- Build Treelet Iteratively (Simulating Recursion) ---
    // Only proceed if the segment has primitives and potentially splits.
    if (numPrimsInSegment > 0 && splitsFurther) {
        // Node allocation offset for this segment's new nodes.
        uint nodeAllocOffset = blockOffsets.offsets[segmentIndex];

        // Stack for iterative traversal [nodeIndex, levelInTreelet, pathOffset, primStartRel, primEndRel]
        uvec4 taskStack[16]; // Max depth p=4. Adjust size if needed. Max treelet nodes = 2*P.
        uint stackPtr = 0;

        // Push initial task: process the root of the treelet (parent node)
        taskStack[stackPtr++] = uvec4(parentNodeIndex, 0, 0, 0); // NodeIdx, Level, PathOffset, PrimStartRel=0
        // Add primEndRel separately or pack differently if needed.
        uint primEndRelStack[16];
        primEndRelStack[0] = numPrimsInSegment;


        uint nodesEmittedCount = 0; // Track nodes allocated from blockOffsets

        while (stackPtr > 0) {
            uvec4 task = taskStack[--stackPtr];
            uint currentNodeIndex = task.x;
            uint currentLevel = task.y;
            uint currentPathOffset = task.z;
            uint currentPrimStartRel = task.w;
            uint currentPrimEndRel = primEndRelStack[stackPtr];


            // Check if we have reached the depth limit for this treelet
            if (currentLevel >= u_p) {
                // This node is a leaf for the *next* pass
                uint nextSegmentHeadIndex = atomicAdd(atomicNewSegmentCounter, 1);
                segmentHeadsOut.heads[nextSegmentHeadIndex] = firstPrimitiveAbs + currentPrimStartRel;
                headToNodeOut.map[nextSegmentHeadIndex] = currentNodeIndex;
                // Mark the node itself as a leaf if it wasn't already
                 if(currentPrimEndRel > currentPrimStartRel) { // Only if it contains primitives
                    bvhNodes.nodes[currentNodeIndex].primitiveCount = currentPrimEndRel - currentPrimStartRel;
                    bvhNodes.nodes[currentNodeIndex].leftChild = firstPrimitiveAbs + currentPrimStartRel; // Store first primitive index
                    bvhNodes.nodes[currentNodeIndex].rightChild = 0xFFFFFFFF; // Mark as leaf
                 } else {
                     // Handle empty leaf case if necessary (shouldn't happen with valid splits)
                      bvhNodes.nodes[currentNodeIndex].primitiveCount = 0;
                 }
                continue;
            }

            // Find the split index in the block descriptor for the current level/path
            uint splitDescIndex = (1 << currentLevel) - 1 + currentPathOffset;
            int splitPrimRel = -1; // Relative index within segment where split occurs
            if (splitDescIndex < P) { // Check bounds; P = (1 << u_p) - 1
                 splitPrimRel = sharedBlockSplits[splitDescIndex];
            }


            if (splitPrimRel != -1 && splitPrimRel > currentPrimStartRel && splitPrimRel < currentPrimEndRel) {
                // --- Internal Node ---
                // Allocate two child nodes using the precomputed offset
                uint leftChildIndex = nodeAllocOffset + nodesEmittedCount++;
                uint rightChildIndex = nodeAllocOffset + nodesEmittedCount++;

                // Update current node to be internal, linking children
                bvhNodes.nodes[currentNodeIndex].leftChild = leftChildIndex;
                bvhNodes.nodes[currentNodeIndex].rightChild = rightChildIndex;
                bvhNodes.nodes[currentNodeIndex].primitiveCount = 0; // Mark as internal

                // Update children's parent pointer
                bvhNodes.nodes[leftChildIndex].parent = currentNodeIndex;
                bvhNodes.nodes[rightChildIndex].parent = currentNodeIndex;
                 // Initialize children as leaves initially, may be overwritten if they split further
                bvhNodes.nodes[leftChildIndex].primitiveCount = splitPrimRel - currentPrimStartRel;
                bvhNodes.nodes[leftChildIndex].leftChild = firstPrimitiveAbs + currentPrimStartRel;
                bvhNodes.nodes[leftChildIndex].rightChild = 0xFFFFFFFF;
                bvhNodes.nodes[rightChildIndex].primitiveCount = currentPrimEndRel - splitPrimRel;
                bvhNodes.nodes[rightChildIndex].leftChild = firstPrimitiveAbs + splitPrimRel;
                bvhNodes.nodes[rightChildIndex].rightChild = 0xFFFFFFFF;


                // Push tasks for children (process next level)
                // Left child task
                if (stackPtr < 16 && (splitPrimRel > currentPrimStartRel)) {
                   taskStack[stackPtr++] = uvec4(leftChildIndex, currentLevel + 1, currentPathOffset * 2, currentPrimStartRel);
                   primEndRelStack[stackPtr - 1] = splitPrimRel; // Range [start, split)
                }
                 // Right child task
                if (stackPtr < 16 && (currentPrimEndRel > splitPrimRel)) {
                    taskStack[stackPtr++] = uvec4(rightChildIndex, currentLevel + 1, currentPathOffset * 2 + 1, splitPrimRel);
                     primEndRelStack[stackPtr - 1] = currentPrimEndRel; // Range [split, end)
                }

            } else {
                // --- No Split at this level ---
                // This node remains a leaf for the next pass, or continue down the single path if p > 1.
                // If we allow p > 1, we need to handle descending without splitting.
                // For simplicity, let's assume if splitPrimRel == -1, we stop here for this pass.

                uint nextSegmentHeadIndex = atomicAdd(atomicNewSegmentCounter, 1);
                segmentHeadsOut.heads[nextSegmentHeadIndex] = firstPrimitiveAbs + currentPrimStartRel;
                headToNodeOut.map[nextSegmentHeadIndex] = currentNodeIndex;
                 // Mark the node itself as a leaf
                 if(currentPrimEndRel > currentPrimStartRel) {
                     bvhNodes.nodes[currentNodeIndex].primitiveCount = currentPrimEndRel - currentPrimStartRel;
                     bvhNodes.nodes[currentNodeIndex].leftChild = firstPrimitiveAbs + currentPrimStartRel;
                     bvhNodes.nodes[currentNodeIndex].rightChild = 0xFFFFFFFF;
                 } else {
                      bvhNodes.nodes[currentNodeIndex].primitiveCount = 0; // Should not happen
                 }
            }
        } // End while loop

    } else if (numPrimsInSegment > 0) {
         // --- Segment does not split further in these p bits ---
         // It remains a single leaf for the next pass. Output it.
         uint nextSegmentHeadIndex = atomicAdd(atomicNewSegmentCounter, 1);
         segmentHeadsOut.heads[nextSegmentHeadIndex] = firstPrimitiveAbs;
         headToNodeOut.map[nextSegmentHeadIndex] = parentNodeIndex; // Point to the same parent node

         // Ensure the parent node is correctly marked as a leaf containing these primitives
         bvhNodes.nodes[parentNodeIndex].primitiveCount = numPrimsInSegment;
         bvhNodes.nodes[parentNodeIndex].leftChild = firstPrimitiveAbs;
         bvhNodes.nodes[parentNodeIndex].rightChild = 0xFFFFFFFF;
    }
    // Else: numPrimsInSegment == 0, do nothing.

} // End main
