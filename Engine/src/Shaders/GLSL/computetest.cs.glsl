#version 460 core

#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_bindless_texture : require // Needed for getTriangle
#extension GL_ARB_gpu_shader_int64 : require   // Needed for getTriangle
#extension GL_NV_shader_buffer_load : require  // Needed for getTriangle
#extension GL_NV_gpu_shader5 : require         // Needed for getTriangle

// Output Image (instead of buffer)
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba8, binding = 0) uniform restrict writeonly image2D outputImage;

// --- Constants ---
const uint TARGET_ROOT_INDEX = 221091u;
const vec3 RAY_ORIGIN = vec3(2.0, 0.0, 4.0); // Placeholder for probe (1,0,2) world pos - USER NEEDS TO SET CORRECTLY
const uvec2 TEXTURE_DIMS = uvec2(1024, 1024);

// --- Struct Definitions (Copied/Adapted from PopulateProbesDDGI.cs.glsl) ---

struct BVHNode {
    int left;
    int right;
    uint primitiveIdx;
    vec3 aabbMin;
    vec3 aabbMax;
};

struct BufferMetadata {
    uint positionAttributeOffsetBytes;
    uint texCoordAttributeOffsetBytes; // Not used in this test
    uint normalAttributeOffsetBytes;   // Not used in this test
    uint tangentAttributeOffsetBytes;  // Not used in this test
    uint vertexStrideBytes;
    uint indexType; // GL_UNSIGNED_INT (5125) or GL_UNSIGNED_SHORT (5123)
    uint64_t VBOHandle;
    uint64_t IBOHandle;
};

struct MeshInfo {
    uint RootIndex; // index of the root node in the BVH
    uint64_t AlbedoTextureHandle;        // Not used
    uint64_t NormalTextureHandle;        // Not used
    uint64_t MetallicRoughnessTextureHandle; // Not used
    uint bufferMetadataIDX;
    uint vertexOffsetBytes;
    uint indexOffsetBytes;
    mat4 Transform;
    mat4 InvTransform;
};

struct Ray {
    vec3 origin;
    vec3 direction;
    vec3 invDir;
};

// Minimal Triangle for this test
struct Triangle {
    vec3 v0;
    vec3 v1;
    vec3 v2;
};


// --- Buffers (Bindings need to match C++) ---

// Contains the actual BVH node data for the target mesh
layout(std430, binding = 1) readonly buffer LBVHNodes {
    BVHNode lbvh[];
} u_lbvh;

// Contains BufferMetadata structs
layout(std430, binding = 2) readonly buffer BufferMetadataStorage {
    BufferMetadata AllBufferMetadata[];
} u_bufferMetadata;

// Contains MeshInfo structs (only need the one for TARGET_ROOT_INDEX)
// Option 1: Bind buffer with all MeshInfos
layout(std430, binding = 3) readonly buffer SceneInfo {
    MeshInfo MeshInfos[];
} u_sceneInfo;
// Option 2: Bind only the single target MeshInfo struct (simpler if possible)
// layout(std430, binding = 3) readonly buffer TargetMeshInfo { MeshInfo targetMesh; } u_targetMeshInfo;

// --- Helper Functions (Copied/Adapted) ---

vec2 octEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 encoded = (n.z >= 0.0) ? n.xy : vec2(
        (1.0 - abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0),
        (1.0 - abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0)
    );
    return encoded * 0.5 + 0.5;
}

vec3 octDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.x += (n.x >= 0.0 ? -t : t);
    n.y += (n.y >= 0.0 ? -t : t);
    return normalize(n);
}

#define UNSIGNED_INT 5125
#define UNSIGNED_SHORT 5123
#define INFINITY_FLOAT 0x7FFFFFFF
#define INVALID_POINTER 0x0

// getTriangle using bindless (copied from PopulateProbesDDGI)
// Only fetches position data for simplicity
Triangle getTriangle(MeshInfo meshInfo, uint primitiveIndex) {
    Triangle tri = Triangle(vec3(0.0), vec3(0.0), vec3(0.0));
    BufferMetadata bufferMetadata = u_bufferMetadata.AllBufferMetadata[meshInfo.bufferMetadataIDX];
    uint64_t vboHandle = bufferMetadata.VBOHandle;
    uint64_t iboHandle = bufferMetadata.IBOHandle;
    uint indexType = bufferMetadata.indexType;
    uint indexSize = indexType == UNSIGNED_INT ? 4 : 2;

    uint indexOffsetBytes = meshInfo.indexOffsetBytes + (indexSize * primitiveIndex * 3);
    uint indices[3];
    uint8_t *iboPtr = (uint8_t *)iboHandle;

    if (indexType == UNSIGNED_INT) {
        indices[0] = *((uint *)(iboPtr + indexOffsetBytes + 0));
        indices[1] = *((uint *)(iboPtr + indexOffsetBytes + 4));
        indices[2] = *((uint *)(iboPtr + indexOffsetBytes + 8));
    } else {
        indices[0] = uint(*((uint16_t *)(iboPtr + indexOffsetBytes + 0)));
        indices[1] = uint(*((uint16_t *)(iboPtr + indexOffsetBytes + 2)));
        indices[2] = uint(*((uint16_t *)(iboPtr + indexOffsetBytes + 4)));
    }

    uvec3 vertexStartByteOffset;
    vertexStartByteOffset[0] = meshInfo.vertexOffsetBytes + (indices[0] * bufferMetadata.vertexStrideBytes);
    vertexStartByteOffset[1] = meshInfo.vertexOffsetBytes + (indices[1] * bufferMetadata.vertexStrideBytes);
    vertexStartByteOffset[2] = meshInfo.vertexOffsetBytes + (indices[2] * bufferMetadata.vertexStrideBytes);

    uvec3 positionStartByteOffset = vertexStartByteOffset + bufferMetadata.positionAttributeOffsetBytes;
    uint8_t *vboBasePtr = (uint8_t *)vboHandle;

    vec3 v0_local = vec3(
        *((float *)(vboBasePtr + positionStartByteOffset[0] + 0)),
        *((float *)(vboBasePtr + positionStartByteOffset[0] + 4)),
        *((float *)(vboBasePtr + positionStartByteOffset[0] + 8))
    );
     vec3 v1_local = vec3(
        *((float *)(vboBasePtr + positionStartByteOffset[1] + 0)),
        *((float *)(vboBasePtr + positionStartByteOffset[1] + 4)),
        *((float *)(vboBasePtr + positionStartByteOffset[1] + 8))
    );
     vec3 v2_local = vec3(
        *((float *)(vboBasePtr + positionStartByteOffset[2] + 0)),
        *((float *)(vboBasePtr + positionStartByteOffset[2] + 4)),
        *((float *)(vboBasePtr + positionStartByteOffset[2] + 8))
    );

    // Transform to world space
    tri.v0 = (meshInfo.Transform * vec4(v0_local, 1.0)).xyz;
    tri.v1 = (meshInfo.Transform * vec4(v1_local, 1.0)).xyz;
    tri.v2 = (meshInfo.Transform * vec4(v2_local, 1.0)).xyz;

    return tri;
}


bool RayBoundingBoxDst(Ray ray, vec3 aabbMin, vec3 aabbMax, out float tmin_out) {
    vec3 tMin = (aabbMin - ray.origin) * ray.invDir;
	vec3 tMax = (aabbMax - ray.origin) * ray.invDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
	float tNear = max(max(t1.x, t1.y), t1.z);
	float tFar = min(min(t2.x, t2.y), t2.z);
	bool hit = tFar >= tNear && tFar > 0;
	tmin_out = hit ? (tNear > 0 ? tNear : 0) : INFINITY_FLOAT;
    return hit;
}

// traceBVH (copied from PopulateProbesDDGI)
bool traceBVH(Ray ray, uint index, out BVHNode result) {
    uint currentNodeIndex = 0;
    #define MAX_STACK_SIZE 128
    uint nodeStack[MAX_STACK_SIZE];
    uint stackPointer = 0;
    nodeStack[stackPointer++] = index; // Start with the mesh's root index

    float closestLeafHitDist = INFINITY_FLOAT;
    bool leafFound = false;

    while (stackPointer > 0) {
        currentNodeIndex = nodeStack[--stackPointer];
        // Check if currentNodeIndex is valid within the bound buffer length (important!)
        // This requires knowing the total number of nodes in the u_lbvh buffer or careful indexing.
        // Assuming currentNodeIndex is a valid absolute index for now.
        BVHNode node = u_lbvh.lbvh[currentNodeIndex];

        float tHitCurrent;
        if (RayBoundingBoxDst(ray, node.aabbMin, node.aabbMax, tHitCurrent)) {
            // If this node is closer than the closest leaf found so far, process it.
            // If it's a leaf, record it. If internal, push children.
             if (tHitCurrent < closestLeafHitDist) {
                 // Check if it's a leaf node
                 if (node.left == INVALID_POINTER && node.right == INVALID_POINTER) {
                     result = node;
                     closestLeafHitDist = tHitCurrent; // Found a leaf, update closest distance
                     leafFound = true;
                     // Don't return yet, another closer leaf might be found down a different path
                     // if the AABBs overlap significantly. Continue search in stack.
                 } else {
                     // Internal node, push children if they are intersected and closer
                     uint childIndexA = node.left; // Assuming absolute pointers for simplicity
                     uint childIndexB = node.right;

                     // Basic check if indices seem valid (adjust based on actual index range)
                     // This requires knowing the size of the u_lbvh buffer. Add checks if possible.
                     // bool childAValid = childIndexA < u_lbvh_buffer_size;
                     // bool childBValid = childIndexB < u_lbvh_buffer_size;

                     // Simple check for now:
                     bool childAValid = true;
                     bool childBValid = true;

                     BVHNode childA;
                     BVHNode childB;
                     if (childAValid) childA = u_lbvh.lbvh[childIndexA];
                     if (childBValid) childB = u_lbvh.lbvh[childIndexB];

                     float dstA = INFINITY_FLOAT, dstB = INFINITY_FLOAT;
                     bool hitA = childAValid && RayBoundingBoxDst(ray, childA.aabbMin, childA.aabbMax, dstA);
                     bool hitB = childBValid && RayBoundingBoxDst(ray, childB.aabbMin, childB.aabbMax, dstB);

                     // Push children onto stack, farther one first
                     if (hitA && hitB) {
                         if (dstA < dstB) { // A is nearer
                             if (stackPointer < MAX_STACK_SIZE) nodeStack[stackPointer++] = childIndexB;
                             if (stackPointer < MAX_STACK_SIZE) nodeStack[stackPointer++] = childIndexA;
                         } else { // B is nearer or equal
                             if (stackPointer < MAX_STACK_SIZE) nodeStack[stackPointer++] = childIndexA;
                             if (stackPointer < MAX_STACK_SIZE) nodeStack[stackPointer++] = childIndexB;
                         }
                     } else if (hitA) {
                         if (stackPointer < MAX_STACK_SIZE) nodeStack[stackPointer++] = childIndexA;
                     } else if (hitB) {
                         if (stackPointer < MAX_STACK_SIZE) nodeStack[stackPointer++] = childIndexB;
                     }
                }
             } // end if (tHitCurrent < closestLeafHitDist)
        } // end if RayBoundingBoxDst
    } // end while

    return leafFound; // Return true if any leaf node was hit
}

// --- Main Test Logic ---

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    // Check if pixel is out of bounds for the output image
    if (pixelCoord.x >= TEXTURE_DIMS.x || pixelCoord.y >= TEXTURE_DIMS.y) {
        return;
    }

    // Calculate ray direction based on pixel coordinate (like octDecode)
    vec2 normCoord = vec2(pixelCoord) / vec2(TEXTURE_DIMS);
    vec3 rayDirection = octDecode(normCoord);
    vec3 invDir = 1.0 / rayDirection;

    Ray ray = Ray(RAY_ORIGIN, rayDirection, invDir);

    // Find the target MeshInfo (adjust based on how MeshInfos are bound)
    // Option 1: Iterate through u_sceneInfo.MeshInfos[]
    int targetMeshBufferIndex = -1;
    for (int i = 0; i < u_sceneInfo.MeshInfos.length(); ++i) { // Requires knowing length or a count uniform
        if (u_sceneInfo.MeshInfos[i].RootIndex == TARGET_ROOT_INDEX) {
            targetMeshBufferIndex = i;
            break;
        }
    }
     MeshInfo meshInfo;
     bool meshFound = false;
     if(targetMeshBufferIndex != -1) {
         meshInfo = u_sceneInfo.MeshInfos[targetMeshBufferIndex];
         meshFound = true;
     }

    // Option 2: Directly use bound target mesh info (if using binding 3 like that)
    // MeshInfo meshInfo = u_targetMeshInfo.targetMesh;
    // bool meshFound = true; // Assume it's always found if bound directly

    vec4 outputColor = vec4(0.0, 0.0, 0.0, 1.0); // Default: Black for miss

    if (meshFound) {
        // --- Transform Ray to Mesh Local Space ---
        // BVH trace should happen in world space if BVH nodes store world space AABBs
        // OR ray needs to be transformed if BVH uses local space AABBs.
        // Assuming BVH nodes are world space based on LBVH.cpp adding transform.
        // If not, transform the ray here using meshInfo.InvTransform.

        BVHNode hitNode;
        if (traceBVH(ray, meshInfo.RootIndex, hitNode)) {
            // BVH reported a hit with a leaf node 'hitNode'

            // We don't need to call getTriangle for this specific test,
            // as the goal is to visualize if the BVH trace *finds* a leaf.
            // The 'every other triangle missing' might be *after* this stage.
            // We visualize based on the primitive index found by the BVH.

            // Visualize the primitive index
            // Normalize or scale it to fit in a color channel
            float colorVal = float(hitNode.primitiveIdx % 255) / 255.0f;
            outputColor = vec4(colorVal, colorVal, colorVal, 1.0);

            // Alternative: Visualize hit distance (might need T from traceBVH)
            // float hitDist = ...; // Get distance from traceBVH if possible
            // outputColor = vec4(fract(hitDist * 0.1), fract(hitDist), fract(hitDist * 10.0), 1.0);

            // Alternative: Just white for hit
            // outputColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
    } else {
        // Target mesh info wasn't found in the buffer (if using iteration)
        outputColor = vec4(1.0, 0.0, 1.0, 1.0); // Magenta to indicate mesh not found
    }

    // Write result to the output image
    imageStore(outputImage, pixelCoord, outputColor);
}