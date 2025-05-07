#version 450 core

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_ARB_shader_storage_buffer_object : require

// #define USE_BINDLESS_BUFFERS 0

#ifdef USE_BINDLESS_BUFFERS
    #extension GL_NV_shader_buffer_load : require
    #extension GL_NV_gpu_shader5 : require
#endif


// Define workgroup size (tune based on GPU architecture)
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// --- Data Structures (Matching C++) ---

struct GpuMeshMetadata {
    uint vertexOffsetBytes; // Starting *byte* offset in the global vertex buffer
    uint indexOffsetBytes;  // Starting *byte* offset in the global index buffer
    uint triangleCount;     // Number of triangles in this mesh

    uint positionAttributeOffsetBytes; // Offset of position *within* the stride
    uint vertexStrideBytes;            // Stride of the vertex buffer in bytes
    uint indexType;                    // GL_UNSIGNED_INT (5125) or GL_UNSIGNED_SHORT (5123)

#ifdef USE_BINDLESS_BUFFERS
    uint64_t vboHandle;
    uint64_t iboHandle;
#endif

    uint meshIndex;
};

struct GpuOutputMortonElement {
    uint mortonCode;
    uint originalTriangleIndex; // Index within its mesh
    uint meshIndex;
    // uint padding; // Add if needed for std430 16-byte alignment
};

#define UNSIGNED_INT 5125
#define UNSIGNED_SHORT 5123

// --- Buffers ---

// Input: Contains vertex data for all meshes contiguously
layout(std430, binding = 0) readonly buffer VertexBuffer {
    uint data[]; // Access raw bytes via uints
} vertexBuffer;

// Input: Contains index data for all meshes contiguously
layout(std430, binding = 1) readonly buffer IndexBuffer {
    uint data[]; // Access raw bytes via uints
} indexBuffer;

// Input: Metadata for the specific mesh being processed
layout(std430, binding = 2) readonly buffer MeshMetadataBuffer {
    GpuMeshMetadata mesh; // Reads the single metadata struct
} meshMetadataBuffer;

// Output: Stores the calculated Morton code + mesh index for each triangle
layout(std430, binding = 3) writeonly buffer OutputMortonBuffer {
    GpuOutputMortonElement mortonElements[];
} outputMortonBuffer;

struct Element {
    uint primitiveIdx;// the id of the primitive; this primitive id is copied to the leaf nodes of the  LBVHNode
    float aabbMinX;// aabb of the primitive
    float aabbMinY;
    float aabbMinZ;
    float aabbMaxX;
    float aabbMaxY;
    float aabbMaxZ;
};

layout(std430, binding = 4) buffer PrimitiveAABBs {
    Element bounds[];
} primitiveAABBs;

// --- Uniforms ---

uniform uvec3 u_sceneAABBQuantizationBits = uvec3(10, 10, 10); // e.g., 10 bits per axis for 30-bit code
uniform vec3 u_sceneAABBMin = vec3(0.0); // Default, should be set from C++
uniform vec3 u_sceneAABBMax = vec3(1.0); // Default, should be set from C++
// uniform uint u_totalTriangleCount; // Should match dispatch size

// --- Helper Functions ---

// Reads a 32-bit or 16-bit index from the index buffer given byte offsets.
// baseIndexByteOffset: Starting byte offset of this mesh's indices in the global IndexBuffer.
// relativeTriangleIndex: The index of the triangle within this mesh (0 to triangleCount-1).
// indexWithinTriangle: Which vertex index within the triangle (0, 1, or 2).
// indexType: GL_UNSIGNED_INT or GL_UNSIGNED_SHORT.
uint readIndex(uint baseIndexByteOffset, uint relativeTriangleIndex, uint indexWithinTriangle, uint indexType) {
    uint indexSize = (indexType == UNSIGNED_INT) ? 4 : 2;
    // Calculate the absolute byte offset for the specific index value
    uint absoluteByteOffset = baseIndexByteOffset + (relativeTriangleIndex * 3 + indexWithinTriangle) * indexSize;

    uint uintAddress = absoluteByteOffset / 4;
    uint byteWithinUint = absoluteByteOffset % 4;

    // Load the uint(s) containing the index data
    uint word0 = indexBuffer.data[uintAddress];
    // Pre-fetch next word only if needed for unaligned 32-bit read
    uint word1 = (indexType == UNSIGNED_INT && byteWithinUint > 0) ? indexBuffer.data[uintAddress + 1] : 0;

    if (indexType == UNSIGNED_INT) { // uint32
        if (byteWithinUint == 0) {
            return word0; // Aligned read
        } else {
             // Handle unaligned 32-bit read (assumes little-endian)
             // This should ideally be avoided by ensuring source data alignment
             uint lowerBits = word0 >> (byteWithinUint * 8);
             uint upperBits = word1 << (32 - (byteWithinUint * 8));
             return lowerBits | upperBits;
        }
    } else { // uint16
        // Extract the 16-bit value based on its byte offset within the uint
        uint shift = byteWithinUint * 8;
        return (word0 >> shift) & 0xFFFFu;
    }
}

// Reads a vec3 position attribute from the vertex buffer given byte offsets.
// baseVertexByteOffset: Starting byte offset of this mesh's vertices in the global VertexBuffer.
// relativeVertexIndex: The actual vertex index value read from the index buffer.
// vertexStrideBytes: The byte stride between vertices.
// positionAttributeOffsetBytes: The byte offset of the position attribute *within* the vertex stride.
vec3 readPosition(uint baseVertexByteOffset, uint relativeVertexIndex, uint vertexStrideBytes, uint positionAttributeOffsetBytes) {
    // Calculate the absolute byte offset for the start of the specific vertex's data
    uint vertexStartByteOffset = baseVertexByteOffset + (relativeVertexIndex * vertexStrideBytes);
    // Calculate the absolute byte offset for the start of the position attribute within that vertex
    uint positionStartByteOffset = vertexStartByteOffset + positionAttributeOffsetBytes;

    // Calculate the uint array index and offset for the position data
    // Assuming position is vec3 (3 floats = 12 bytes) and is reasonably aligned
    uint uintAddress = positionStartByteOffset / 4;
    uint byteOffsetInUint = positionStartByteOffset % 4;

    // Handle potential unaligned reads if position doesn't start on a 4-byte boundary
    // Note: This simplified version assumes alignment for clarity. Robust handling is more complex.
    if (byteOffsetInUint != 0) {
        // TODO: Implement robust handling for unaligned vec3 reads if necessary.
        // This might involve reading multiple uints and bit-shifting/combining.
        // For now, return a placeholder or assume alignment.
        return vec3(0.0, 0.0, 0.0); // Placeholder for unaligned data
    }

    // Read the 3 floats assuming they are aligned starting at uintAddress
    return vec3(
        uintBitsToFloat(vertexBuffer.data[uintAddress + 0]),
        uintBitsToFloat(vertexBuffer.data[uintAddress + 1]),
        uintBitsToFloat(vertexBuffer.data[uintAddress + 2])
    );
}


#define UNSIGNED_INT 5125
#define UNSIGNED_SHORT 5123
#define INFINITY_FLOAT 0x7FFFFFFF


#ifdef USE_BINDLESS_BUFFERS
struct Triangle {
    vec3 v0;
    vec3 v1;
    vec3 v2;
};

// meshinfo contains needed metadata about where to get the vertex data, the index is for the specific triangle
Triangle getTriangle(GpuMeshMetadata meshInfo, uint primitiveIndex) {
    Triangle tri = Triangle(vec3(0.0), vec3(0.0), vec3(0.0));
    uint64_t vboHandle = meshInfo.vboHandle;
    uint64_t iboHandle = meshInfo.iboHandle;
    uint indexType = meshInfo.indexType;
    uint indexSize = indexType == UNSIGNED_INT ? 4 : 2;


    uint indexOffsetBytes = meshInfo.indexOffsetBytes + (indexSize * primitiveIndex * 3);

    uint indices[3];

    uint8_t *iboPtr = (uint8_t *)iboHandle;


    // Read indices based on indexType
    if (indexType == UNSIGNED_INT) { // GL_UNSIGNED_INT

        uint ind0 = *((uint *)(iboPtr + indexOffsetBytes + 0)); // Offset 12 is 4-byte aligned
        uint ind1 = *((uint *)(iboPtr + indexOffsetBytes + 4)); // Offset 16 is 4-byte aligned
        uint ind2 = *((uint *)(iboPtr + indexOffsetBytes + 8)); // Offset 20 is 4-byte aligned
        indices[0] = ind0;
        indices[1] = ind1;
        indices[2] = ind2;
    } else { // Assuming GL_UNSIGNED_SHORT (5123)

        uint16_t ind0 = *((uint16_t *)(iboPtr + indexOffsetBytes + 0)); // Offset 12 is 4-byte aligned
        uint16_t ind1 = *((uint16_t *)(iboPtr + indexOffsetBytes + 2)); // Offset 16 is 4-byte aligned
        uint16_t ind2 = *((uint16_t *)(iboPtr + indexOffsetBytes + 4)); // Offset 20 is 4-byte aligned
        indices[0] = uint(ind0);
        indices[1] = uint(ind1);
        indices[2] = uint(ind2);
    }

    uvec3 vertexStartByteOffset;
    vertexStartByteOffset[0] = meshInfo.vertexOffsetBytes + (indices[0] * meshInfo.vertexStrideBytes);
    vertexStartByteOffset[1] = meshInfo.vertexOffsetBytes + (indices[1] * meshInfo.vertexStrideBytes);
    vertexStartByteOffset[2] = meshInfo.vertexOffsetBytes + (indices[2] * meshInfo.vertexStrideBytes);

    // Calculate the absolute byte offset for the start of the position attribute within that vertex
    uvec3 positionStartByteOffset = vertexStartByteOffset + meshInfo.positionAttributeOffsetBytes;



    uint8_t *vboBasePtr = (uint8_t *)vboHandle;

    
    float v0x = *((float *)(vboBasePtr + positionStartByteOffset[0] + 0)); // Offset 12 is 4-byte aligned
    float v0y = *((float *)(vboBasePtr + positionStartByteOffset[0] + 4)); // Offset 16 is 4-byte aligned
    float v0z = *((float *)(vboBasePtr + positionStartByteOffset[0] + 8)); // Offset 20 is 4-byte aligned
    vec3 v0_local = vec3(v0x, v0y, v0z);


    // --- Vertex 1 ---
    float v1x = *((float *)(vboBasePtr + positionStartByteOffset[1] + 0));
    float v1y = *((float *)(vboBasePtr + positionStartByteOffset[1] + 4));
    float v1z = *((float *)(vboBasePtr + positionStartByteOffset[1] + 8));
    vec3 v1_local = vec3(v1x, v1y, v1z);

    // --- Vertex 2 ---
    float v2x = *((float *)(vboBasePtr + positionStartByteOffset[2] + 0));
    float v2y = *((float *)(vboBasePtr + positionStartByteOffset[2] + 4));
    float v2z = *((float *)(vboBasePtr + positionStartByteOffset[2] + 8));
    vec3 v2_local = vec3(v2x, v2y, v2z);
    

    
    tri.v0 = v0_local;
    tri.v1 = v1_local;
    tri.v2 = v2_local;

    

    return tri;
}
#endif

// Expands a 10-bit integer into 30 bits by inserting 2 zeros after each bit.
// Used for interleaving bits for Morton codes.
uint expandBits10(uint v) {
    v = (v * 0x00010001u) & 0xFF0000FFu;
    v = (v * 0x00000101u) & 0x0F00F00Fu;
    v = (v * 0x00000011u) & 0xC30C30C3u;
    v = (v * 0x00000005u) & 0x49249249u;
    return v;
}

uint morton3D(float x, float y, float z) {
    x = min(max(x * 1024.0f, 0.0f), 1023.0f);
    y = min(max(y * 1024.0f, 0.0f), 1023.0f);
    z = min(max(z * 1024.0f, 0.0f), 1023.0f);
    uint xx = expandBits10(uint(x));
    uint yy = expandBits10(uint(y));
    uint zz = expandBits10(uint(z));
    return xx * 4 + yy * 2 + zz;
}

// --- Main Logic ---

void main() {
    uint globalTriangleIndex = gl_GlobalInvocationID.x;

    // Get metadata for the specific mesh being processed by this dispatch
    GpuMeshMetadata mesh = meshMetadataBuffer.mesh;

    // Prevent processing beyond the actual number of triangles for this mesh
    if (globalTriangleIndex >= mesh.triangleCount) {
        return;
    }


    uint baseIndexByteOffset = mesh.indexOffsetBytes;
    uint indexType = mesh.indexType;

    // Read vertex indices for this triangle using byte offsets
    uint i0 = readIndex(baseIndexByteOffset, globalTriangleIndex, 0, indexType);
    uint i1 = readIndex(baseIndexByteOffset, globalTriangleIndex, 1, indexType);
    uint i2 = readIndex(baseIndexByteOffset, globalTriangleIndex, 2, indexType);

    // Read vertex positions (local space) using byte offsets
    uint baseVertexByteOffset = mesh.vertexOffsetBytes;
    uint stride = mesh.vertexStrideBytes;
    uint posAttrOffset = mesh.positionAttributeOffsetBytes; // Offset *within* stride
    vec3 v0 = readPosition(baseVertexByteOffset, i0, stride, posAttrOffset);
    vec3 v1 = readPosition(baseVertexByteOffset, i1, stride, posAttrOffset);
    vec3 v2 = readPosition(baseVertexByteOffset, i2, stride, posAttrOffset);

#ifdef USE_BINDLESS_BUFFERS
    Triangle tri = getTriangle(mesh, globalTriangleIndex);

    v0 = tri.v0;
    v1 = tri.v1;
    v2 = tri.v2;
#endif


    // Calculate AABB for the triangle
    vec3 minBounds;
    minBounds.x = min(v0.x, min(v1.x, v2.x));
    minBounds.y = min(v0.y, min(v1.y, v2.y));
    minBounds.z = min(v0.z, min(v1.z, v2.z));

    vec3 maxBounds;
    maxBounds.x = max(v0.x, max(v1.x, v2.x));
    maxBounds.y = max(v0.y, max(v1.y, v2.y));
    maxBounds.z = max(v0.z, max(v1.z, v2.z));

    Element bounds = Element(globalTriangleIndex, minBounds.x, minBounds.y, minBounds.z, maxBounds.x, maxBounds.y, maxBounds.z);
    primitiveAABBs.bounds[globalTriangleIndex] = bounds;

    // Calculate centroid (local space)
    vec3 _centroid = (v0 + v1 + v2) / 3.0f;

    // --- Morton Code Calculation (Quantization + Interleaving) ---
    vec3 sceneExtent = max(u_sceneAABBMax - u_sceneAABBMin, vec3(0.0001f));
    vec3 normalizedPos = clamp((_centroid - u_sceneAABBMin) / sceneExtent, 0.0, 1.0); // Clamp normalized pos
    uvec3 quantizationMask = (uvec3(1) << u_sceneAABBQuantizationBits) - uvec3(1);
    uvec3 quantizedPos = uvec3(normalizedPos * vec3(quantizationMask));

   // uint mortonCode = (expandBits10(quantizedPos.z) << 2) |
    //                      (expandBits10(quantizedPos.y) << 1) |
     //                      expandBits10(quantizedPos.x);
    uint mortonCode = morton3D(normalizedPos.x, normalizedPos.y, normalizedPos.z);
    // --- End Morton Code Calculation ---

    // Write the output
    GpuOutputMortonElement outputElement;
    outputElement.mortonCode = mortonCode;
    outputElement.originalTriangleIndex = globalTriangleIndex; // Index within the mesh
    outputElement.meshIndex = mesh.meshIndex;

    // Write to the output buffer at the global triangle index
    // This assumes the dispatch size matches the total number of triangles
    // being processed across all potential meshes in a larger system,
    // or matches mesh.triangleCount if only one mesh is processed.
    outputMortonBuffer.mortonElements[globalTriangleIndex] = outputElement;
}


