#version 450 core

#extension GL_ARB_shader_storage_buffer_object : require

// --- Workgroup Size ---
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

#define INVALID_POINTER 0x0

struct LBVHConstructionInfo {
    uint parent;// pointer to the parent
    int visitationCount;// number of threads that arrived
};

struct LBVHNode {
    int left;// pointer to the left child or INVALID_POINTER in case of leaf
    int right;// pointer to the right child or INVALID_POINTER in case of leaf
    uint primitiveIdx;// custom value that is copied from the input Element or 0 in case of inner node
    float aabbMinX;// aabb of the node
    float aabbMinY;
    float aabbMinZ;
    float aabbMaxX;
    float aabbMaxY;
    float aabbMaxZ;
};

// --- Uniforms ---
uniform uint u_numPrimitives; // Total number of primitives (N)
uniform uint g_absolute_pointers = 1;// 1 for absolute, 0 for relative pointers

// --- Main Logic ---

layout (std430, binding = 0) coherent buffer lbvh {
    LBVHNode g_lbvh[];// |g_lbvh| == #leafnodes + #internalnodes = g_num_elements + g_num_elements - 1
};

layout (std430,  binding = 1) buffer lbvh_construction_infos {
    LBVHConstructionInfo g_lbvh_construction_infos[];
};

void aabbUnion(vec3 minA, vec3 maxA, vec3 minB, vec3 maxB, out vec3 minAABB, out vec3 maxAABB) {
    minAABB = min(minA, minB);
    maxAABB = max(maxA, maxB);
}

// construct bounding boxes
void main() {
    uint gID = gl_GlobalInvocationID.x;
    uint lID = gl_LocalInvocationID.x;
    const int LEAF_OFFSET = int(u_numPrimitives) - 1;

    if (gID >= u_numPrimitives) {
        return;
    }

    uint nodeIdx = g_lbvh_construction_infos[LEAF_OFFSET + gID].parent;
    while (true) {
        int visitations = atomicAdd(g_lbvh_construction_infos[nodeIdx].visitationCount, 1);
        if (visitations < 1) {
            // this is the first thread that arrived at this node -> finished
            return;
        }
        // this is the second thread that arrived at this node, both children are computed -> compute aabb union and continue
        LBVHNode bvhNode = g_lbvh[nodeIdx];
        LBVHNode bvhNodeChildA;
        LBVHNode bvhNodeChildB;
        if (g_absolute_pointers != 0) {
            bvhNodeChildA = g_lbvh[bvhNode.left];
            bvhNodeChildB = g_lbvh[bvhNode.right];
        } else {
            bvhNodeChildA = g_lbvh[nodeIdx + bvhNode.left];
            bvhNodeChildB = g_lbvh[nodeIdx + bvhNode.right];
        }
        vec3 minA = vec3(bvhNodeChildA.aabbMinX, bvhNodeChildA.aabbMinY, bvhNodeChildA.aabbMinZ);
        vec3 maxA = vec3(bvhNodeChildA.aabbMaxX, bvhNodeChildA.aabbMaxY, bvhNodeChildA.aabbMaxZ);
        vec3 minB = vec3(bvhNodeChildB.aabbMinX, bvhNodeChildB.aabbMinY, bvhNodeChildB.aabbMinZ);
        vec3 maxB = vec3(bvhNodeChildB.aabbMaxX, bvhNodeChildB.aabbMaxY, bvhNodeChildB.aabbMaxZ);
        vec3 minAABB;
        vec3 maxAABB;
        aabbUnion(minA, maxA, minB, maxB, minAABB, maxAABB);
        bvhNode.aabbMinX = minAABB.x;
        bvhNode.aabbMinY = minAABB.y;
        bvhNode.aabbMinZ = minAABB.z;
        bvhNode.aabbMaxX = maxAABB.x;
        bvhNode.aabbMaxY = maxAABB.y;
        bvhNode.aabbMaxZ = maxAABB.z;
        g_lbvh[nodeIdx] = bvhNode;
        if (nodeIdx == 0) {
            return;
        }
        nodeIdx = g_lbvh_construction_infos[nodeIdx].parent;
    }
}