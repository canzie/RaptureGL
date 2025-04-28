#version 450 core

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_storage_buffer_object : require

// --- Workgroup Size ---
// Processing one segment per thread seems reasonable for the emit_treelets step.
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// --- Constants ---
const uint INVALID_NODE = 0xFFFFFFFFu;
const int INVALID_SPLIT = -1; // Sentinel for non-existent splits in descriptor
const uint P_BITS = 3; // Number of bit planes processed per pass (p)
const uint P_SPLITS = (1 << P_BITS) - 1; // Max splits per block descriptor (2^p - 1 = 7)
const uint N_BITS = 30; // Total meaningful bits in Morton code (adjust if different)

// --- Data Structures ---

// Input: Sorted Morton codes and original indices
struct GpuOutputMortonElement {
    uint64_t mortonCode;
    uint originalTriangleIndex; // Index within its mesh (or global original index)
    uint meshIndex;
    // uint padding; // If needed
};

// Output: BVH Node (Cache-friendly 32-byte layout)
struct BVHNode {
    // Layout: [min.x, min.y, min.z, firstChildOrPrimitive] [max.x, max.y, max.z, primitiveCount]
    vec3 minBounds;             // 12 bytes
    uint firstChildOrPrimitive; // 4 bytes (Index of left child OR start index of primitives in sortedMortonElements)

    vec3 maxBounds;             // 12 bytes
    uint primitiveCount;        // 4 bytes (0 for internal, >0 for leaf)

    // Total = 32 bytes
};

// Auxiliary Structure for Block Descriptors (Computed in a prior pass)
// Stores the *relative index within the segment* where a split occurs for the current 'p' bits.
// Size = P_SPLITS = 2^p - 1
struct BlockDescriptor {
    // splits[0] = highest level split (bit p-1)
    // splits[1..2] = next level splits (bit p-2)
    // splits[3..6] = lowest level splits (bit p-3)
    int splits[P_SPLITS]; // Use int, INVALID_SPLIT indicates no split found
};


// --- Buffers ---

// Input: Sorted Morton Elements from Radix Sort
layout(std430, binding = 0) readonly buffer SortedMortonElements {
    GpuOutputMortonElement elements[];
} sortedMortonElements;

// Input: Describes the splits within each segment for the current 'p' bits
// Size = u_numSegments. ASSUMED to be computed in a previous shader pass.
layout(std430, binding = 1) readonly buffer BlockSplitDescriptors {
    BlockDescriptor descriptors[]; // Array of descriptors, one per segment
} blockSplitDescriptors;

// Input: Starting offset in the BVHNode array for each segment's treelet nodes
// Size = u_numSegments + 1. ASSUMED to be computed by a scan in a previous pass.
layout(std430, binding = 2) readonly buffer BlockOffsets {
    uint offsets[]; // offsets[segmentIndex] = where nodes for this segment START
} blockOffsets;

// Input/Output: Maps the start primitive index of a segment to its node index in BVHNodes
// Size = u_numPrimitives. Initialized with root at index 0, others -1.
// Needs careful handling for updates across passes (likely done outside this specific shader).
layout(std430, binding = 3) buffer HeadToNodeMap {
    int map[]; // Use int to allow -1 for invalid/not-a-head
} headToNodeMap;

// Input: Stores the start primitive index of each segment for the CURRENT pass.
// Size = u_numSegments. Needs update for the NEXT pass (outside this shader).
layout(std430, binding = 4) readonly buffer SegmentHeads {
    uint heads[];
} segmentHeads;

// Input: Maps each primitive index to the segment ID it belongs to for the CURRENT pass.
// Size = u_numPrimitives. ASSUMED computed in a previous pass.
// layout(std430, binding = 5) readonly buffer SegmentIDs {
//     uint ids[];
// } segmentIDs; // Not directly used in this simplified treelet emission logic

// Output: The BVH node array being constructed
layout(std430, binding = 6) buffer BVHNodes {
    BVHNode nodes[];
} bvhNodes;

// --- Uniforms ---
uniform uint u_numPrimitives;       // Total number of primitives (N)
uniform uint u_numSegments;         // Number of segments (leaves) from the previous pass
uniform uint u_currentBitLevel;     // Starting bit level for this pass (e.g., 0, p, 2p, ...) - Needed for AABB/Morton checks if used
uniform atomic_uint u_nextNodeIndex; // Atomic counter for allocating new nodes (See Caveat #3)

// --- TODO: Buffers for AABB calculation ---
// You MUST provide these buffers and implement calculateAABB
// layout(std430, binding = 7) readonly buffer VertexBuffer { float data[]; } vertexBuffer;
// layout(std430, binding = 8) readonly buffer IndexBuffer { uint data[]; } indexBuffer;
// struct MeshMetadata { uint vertexOffsetBytes; uint indexOffsetBytes; ... };
// layout(std430, binding = 9) readonly buffer MeshInfo { MeshMetadata mesh[]; } meshInfo;
// layout(std430, binding = 10) readonly buffer PrimitiveAABBs { vec4 minP; vec4 maxP; } primitiveAABBs; // Alternative if precomputed


// --- Helper Functions ---

// TODO: Implement AABB calculation for a range of primitives in the sorted buffer.
// Needs access to sortedMortonElements to get original indices/mesh indices,
// then vertex/index buffers (using mesh metadata) OR a precomputed primitive AABB buffer.
void calculateAABB(uint primitiveStart, uint primitiveCount, out vec3 minB, out vec3 maxB) {
    // --- Placeholder ---
    minB = vec3(0.0); // Invalid AABB, replace with proper calculation
    maxB = vec3(0.0);
    if (primitiveCount == 0) return;

    // --- Actual Implementation Sketch ---
    // minB = vec3( uintBitsToFloat(0x7F800000u)); // Positive Infinity
    // maxB = vec3(-uintBitsToFloat(0x7F800000u)); // Negative Infinity

    // for (uint i = 0; i < primitiveCount; ++i) {
    //     uint elementIndex = primitiveStart + i;
    //     uint origTriIndex = sortedMortonElements.elements[elementIndex].originalTriangleIndex;
    //     uint meshIndex    = sortedMortonElements.elements[elementIndex].meshIndex;

    //     // --- Option A: Calculate from Vertices ---
    //     // 1. Get mesh metadata (vertex/index offsets, stride, index type) using meshIndex
    //     // 2. Read triangle vertex indices (i0, i1, i2) from IndexBuffer using origTriIndex
    //     // 3. Read vertex positions (v0, v1, v2) from VertexBuffer using i0,i1,i2 and stride/offset
    //     // 4. Compute triangle AABB (min(v0,v1,v2), max(v0,v1,v2))
    //     // 5. Union with overall minB, maxB

    //     // --- Option B: Use Precomputed Primitive AABBs ---
    //     // Assuming PrimitiveAABBs stores AABB for each *original* triangle index
    //     // Fetch AABB using origTriIndex (potentially need a global primitive index if multiple meshes)
    //     // Union with overall minB, maxB
    // }
    // --- End Sketch ---

    // Ensure min <= max even if only one primitive
    // maxB = max(minB, maxB);
}

// Recursive function to build a treelet for a segment
// Returns the index of the root node created for this sub-problem.
// rangeStart/End: Primitive indices (in the sortedMortonElements buffer) for this sub-problem.
// bitLevelInPass: Current bit level being considered within the p-bit pass (0 to p-1, relative to u_currentBitLevel).
// descriptorSplitIndex: Index into the BlockDescriptor's splits array for the current node (0 for root, 1/2 for children, etc.).
// segmentIndex: Index of the current segment being processed.
// segmentStartPrimitive: Absolute start index of the segment in sortedMortonElements.
uint buildSubTreelet(
    uint rangeStart,
    uint rangeEnd,
    uint bitLevelInPass,
    uint descriptorSplitIndex,
    uint segmentIndex,
    uint segmentStartPrimitive // Needed to convert relative split index to absolute
) {
    // --- Base case: Reached max depth for this pass or range is empty/invalid ---
    if (bitLevelInPass >= P_BITS || rangeStart >= rangeEnd) {
        uint count = (rangeEnd > rangeStart) ? (rangeEnd - rangeStart) : 0;

        if (count == 0) {
             return INVALID_NODE; // Empty leaf
        }

        // --- Create Leaf Node ---
        uint nodeIndex = atomicAdd(u_nextNodeIndex, 1); // Allocate node index (See Caveat #3)

        BVHNode leaf;
        leaf.primitiveCount = count;
        leaf.firstChildOrPrimitive = rangeStart; // Store start primitive index
        calculateAABB(rangeStart, count, leaf.minBounds, leaf.maxBounds);

        // Ensure AABB is valid even if calculation failed
        leaf.maxBounds = max(leaf.minBounds, leaf.maxBounds);

        bvhNodes.nodes[nodeIndex] = leaf;

        // --- Update maps for the new leaf (Needed for NEXT pass) ---
        // This update should ideally happen collectively after the pass finishes.
        // headToNodeMap.map[rangeStart] = int(nodeIndex);
        // segmentHeads.heads[new_segment_id] = rangeStart;

        return nodeIndex;
    }

    // --- Internal Node ---

    // Find the split point for the current bit level from the descriptor
    // descriptorSplitIndex maps to the p-bit structure (e.g., 0->level p-1, 1/2->level p-2, etc.)
    int splitPointRelative = blockSplitDescriptors.descriptors[segmentIndex].splits[descriptorSplitIndex];

    uint splitPointAbsolute;
    if (splitPointRelative == INVALID_SPLIT) {
        // No split found at this level in the descriptor, all primitives go down one side.
        // Determine which side based on the bit value (requires checking a primitive's morton code)
        // For simplicity here, assume they go right if no split (could be wrong).
        // A more robust way is needed, maybe the descriptor stores which side?
        // Or, if INVALID_SPLIT, we know all primitives share the same bit here.
        // We could read one primitive's Morton code at this bit level.
        // uint64_t sampleMorton = sortedMortonElements.elements[rangeStart].mortonCode;
        // uint currentBitAbsolute = u_currentBitLevel + P_BITS - 1 - bitLevelInPass; // Check this bit logic
        // bool goesRight = ((sampleMorton >> currentBitAbsolute) & 1) != 0;
        // splitPointAbsolute = goesRight ? rangeStart : rangeEnd; // Push all to one side
         splitPointAbsolute = rangeEnd; // Simplistic: push all right if no split info
    } else {
        // Convert relative segment split index to absolute primitive index
        splitPointAbsolute = segmentStartPrimitive + uint(splitPointRelative);
    }


    // Clamp split point to the current range being processed
    splitPointAbsolute = max(rangeStart, min(rangeEnd, splitPointAbsolute));

    // Recursive calls for left and right children
    // Calculate descriptor indices for children: left = 2*i + 1, right = 2*i + 2
    uint leftChildDescIdx = descriptorSplitIndex * 2 + 1;
    uint rightChildDescIdx = descriptorSplitIndex * 2 + 2;

    uint leftChildNode = buildSubTreelet(rangeStart, splitPointAbsolute, bitLevelInPass + 1, leftChildDescIdx, segmentIndex, segmentStartPrimitive);
    uint rightChildNode = buildSubTreelet(splitPointAbsolute, rangeEnd, bitLevelInPass + 1, rightChildDescIdx, segmentIndex, segmentStartPrimitive);

    // Handle cases where one child might be empty/invalid
    if (leftChildNode == INVALID_NODE && rightChildNode == INVALID_NODE) return INVALID_NODE;

    // If one child is invalid, the current node becomes the other child (effectively collapses the node)
    // This handles the singletons mentioned in the paper implicitly IF the recursion returns the valid child index.
    if (leftChildNode == INVALID_NODE) return rightChildNode;
    if (rightChildNode == INVALID_NODE) return leftChildNode;

    // --- Create Internal Node ---
    uint nodeIndex = atomicAdd(u_nextNodeIndex, 1); // Allocate node index (See Caveat #3)

    BVHNode internalNode;
    internalNode.primitiveCount = 0; // Mark as internal
    internalNode.firstChildOrPrimitive = leftChildNode; // Index of left child (right is implicit +1)

    // Calculate AABB (union of children)
    internalNode.minBounds = min(bvhNodes.nodes[leftChildNode].minBounds, bvhNodes.nodes[rightChildNode].minBounds);
    internalNode.maxBounds = max(bvhNodes.nodes[leftChildNode].maxBounds, bvhNodes.nodes[rightChildNode].maxBounds);

     // Ensure AABB is valid
    internalNode.maxBounds = max(internalNode.minBounds, internalNode.maxBounds);

    bvhNodes.nodes[nodeIndex] = internalNode;

    return nodeIndex;
}


// --- Main Logic ---

void main() {
    uint segmentIndex = gl_GlobalInvocationID.x; // Assuming one thread per segment

    if (segmentIndex >= u_numSegments) {
        return;
    }

    // --- This shader focuses ONLY on the Emit Treelets step for the current level 'p' ---
    // It ASSUMES Block Descriptors and Block Offsets are pre-computed for this pass.

    uint segmentStartPrimitive = segmentHeads.heads[segmentIndex];
    // Find the end primitive index for this segment
    uint segmentEndPrimitive = (segmentIndex == u_numSegments - 1)
                               ? u_numPrimitives
                               : segmentHeads.heads[segmentIndex + 1];

    // Get the node index that this segment represented *before* this pass
    // (This might be the root node initially, or a leaf from a previous pass)
    int parentNodeIndex = headToNodeMap.map[segmentStartPrimitive]; // Needs careful handling across passes

    // Basic validation
    if (/*parentNodeIndex == -1 ||*/ segmentStartPrimitive >= segmentEndPrimitive) {
        // This segment is invalid or empty, skip processing.
        // parentNodeIndex might be -1 if it's beyond the initial heads mapping, needs robust check.
        return;
    }

    // --- Build the treelet for this segment ---
    uint treeletRootNodeIndex = buildSubTreelet(
        segmentStartPrimitive,      // Start primitive index for this segment
        segmentEndPrimitive,        // End primitive index for this segment
        0,                          // Start at bit level 0 within the p-bit pass
        0,                          // Start at the root of the block descriptor (split index 0)
        segmentIndex,               // Pass the segment index
        segmentStartPrimitive       // Pass segment start for relative index conversion
    );

    // --- Update Parent / Link Treelet ---
    // This is where the generated treelet needs to be connected into the main hierarchy.
    // The paper's pseudocode is abstract here. Two possibilities:
    // 1. Overwrite the parent node: If parentNodeIndex is valid, fetch bvhNodes.nodes[parentNodeIndex]
    //    and update its fields to become the treeletRootNode (if internal) or point to it.
    // 2. Implicit update: The structure is built such that node indices line up correctly based
    //    on the blockOffsets scan (This requires the node allocation within buildSubTreelet
    //    to use the blockOffsets correctly, which the current atomicAdd doesn't do).

    // If using the pre-calculated offsets properly (Caveat #3), the treeletRootNodeIndex
    // should correspond to the node index expected for this segment's root, replacing
    // the previous leaf node implicitly.

    // For the current simple atomic allocation, we might try overwriting if parent is known:
    // if (treeletRootNodeIndex != INVALID_NODE && parentNodeIndex >= 0 && parentNodeIndex < /* max nodes */) {
    //    bvhNodes.nodes[parentNodeIndex] = bvhNodes.nodes[treeletRootNodeIndex]; // Simplistic overwrite
    //    // THIS IS LIKELY WRONG - the parent might need to point TO the treelet root,
    //    // or the treelet root index SHOULD BE parentNodeIndex if allocation worked differently.
    // }

    // ---> The connection logic requires clarification and depends heavily on correct node allocation.


    // --- Prepare for Next Pass (Conceptual) ---
    // The updates to headToNodeMap and segmentHeads based on the new leaves
    // created by buildSubTreelet need to happen *after* all segments are processed.
    // This typically requires another shader pass involving compaction/scan.
}
