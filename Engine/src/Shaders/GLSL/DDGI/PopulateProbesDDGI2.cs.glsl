#version 460 core

#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_NV_shader_buffer_load : require
#extension GL_NV_gpu_shader5 : require

// --- Debug Macros ---
// Uncomment one of the following lines to enable a specific debug view.
// #define DEBUG_VIEW_PROBE_INDEX        // Visualize the 3D probe index (normalized)
// #define DEBUG_VIEW_PROBE_POSITION     // Visualize the world-space probe position (normalized/wrapped)
// #define DEBUG_VIEW_RAY_DIRECTION      // Visualize the ray direction (normalized)
// #define DEBUG_VIEW_HIT                // Visualize hit success (red for hit, blue for miss)
// #define DEBUG_VIEW_UV                 // Visualize interpolated UV coordinates at hit (R=U, G=V)
// #define DEBUG_VIEW_BVH_HIT_NODE       // Visualize if BVH trace found a leaf node (Green=hit, Red=miss)
// #define DEBUG_VIEW_TRIANGLE_HIT       // Visualize if triangle intersection succeeded (Green=hit, Red=miss)
//#define DEBUG_VIEW_ALBEDO             // Default: Visualize sampled albedo color
#define DEBUG_OUTPUT_BUFFER


layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;


// Output Images
layout (binding = 0, r11f_g11f_b10f) uniform restrict writeonly image2D probeAtlas;
layout (binding = 1, rg16f) uniform restrict writeonly image2D probeDepthAtlas;

layout (binding = 2, r11f_g11f_b10f) uniform restrict readonly image2D prevProbeAtlas;
layout (binding = 3, rg16f) uniform restrict readonly image2D prevProbeDepthAtlas;

// Skybox Cubemap
layout (binding = 4) uniform samplerCube u_skyboxCubemap;

// output of the builder; it is necessary to allocate the (empty) buffer
struct LBVHNode {
    int left;
    int right;
    uint primitiveIdx;
    float aabbMinX; 
    float aabbMinY;
    float aabbMinZ;
    float aabbMaxX;
    float aabbMaxY;
    float aabbMaxZ;
};

struct BVHNode {
    int left;
    int right;
    uint primitiveIdx;
    vec3 aabbMin; 
    vec3 aabbMax;

};

struct BufferMetadata {

    uint positionAttributeOffsetBytes; // Offset of position *within* the stride
    uint texCoordAttributeOffsetBytes;
    uint normalAttributeOffsetBytes;
    uint tangentAttributeOffsetBytes;


    uint vertexStrideBytes;            // Stride of the vertex buffer in bytes
    uint indexType;                    // GL_UNSIGNED_INT (5125) or GL_UNSIGNED_SHORT (5123)

    uint64_t VBOHandle;
    uint64_t IBOHandle;
};

// Input Uniforms / Buffers
// Contains global information about the probe grid
layout(std140, binding = 0) uniform ProbeInfo {
    uvec3 probeGridDimensions; // Number of probes in each dimension (X, Y, Z)
    uvec2 probeResolution; // Resolution of each probe texture (e.g., 8x8)
    vec3 probeSpacing;
    vec3 probeOrigin; // will probably be camera position
};

// Contains the actual BVH node data
layout(std430, binding = 1) readonly buffer LBVHNodes {
    BVHNode lbvh[];
} u_lbvh;


struct MeshInfo {
    uint RootIndex; // index of the root node in the BVH
    uint64_t AlbedoTextureHandle;
    uint64_t NormalTextureHandle;
    uint64_t MetallicRoughnessTextureHandle;
    uint bufferMetadataIDX; // index for BufferMetadata array

    uint vertexOffsetBytes;
    uint indexOffsetBytes;

    mat4 Transform;
};

layout(std430, binding = 2) readonly buffer BufferMetadataStorage {
    BufferMetadata AllBufferMetadata[];
} u_bufferMetadata;

layout(std430, binding = 3) readonly buffer SceneInfo {
    MeshInfo MeshInfos[];
} u_sceneInfo;


#ifdef DEBUG_OUTPUT_BUFFER

struct DebugData {
    uint leafHits;
    uint triangleHits;
    float closestHit;
    uint closestHitMeshIndex;
};

layout(std430, binding = 4) writeonly buffer debugBuffer {
    DebugData u_debugData[];
};
#endif

// --- Light Definitions ---
struct DirectionalLight {
    vec3 direction;
    float intensity;
};

// Use std140 layout for UBOs
layout(std140, binding = 1) uniform LightInfo {
    DirectionalLight u_SunLight;
};

// Define PI constant
#define PI 3.14159265359

uniform vec2 u_AtlasTextureResolution; // atlas pixel resolution in screen size, not ndc
uniform uint u_meshCount;
uniform uint u_probesPerRow; // Number of probes along the X-axis of the atlas texture
uniform float u_hysteresis;

struct Ray {
    vec3 origin;
    vec3 direction;
    vec3 invDir;
};

struct Triangle {
    vec3 v0, v1, v2;  // Vertices in world space
    vec3 n0, n1, n2;  // Normals in world space
    vec3 t0, t1, t2;  // Tangents in world space
    vec2 uv0, uv1, uv2; // Texture coordinates
};

// Placeholder struct for trace result
struct HitInfo {
    bool hit;
    float t;            // Hit distance
    vec2 barycentricUV; // Barycentric coords at hit point
    uint primitiveIndex; // Index within the mesh's index buffer (e.g., first index of the triangle)
    uint meshIndex;      // Index into u_sceneInfo.MeshInfos array
};


#define UNSIGNED_INT 5125
#define UNSIGNED_SHORT 5123
#define INFINITY_FLOAT 0x7FFFFFFF

// meshinfo contains needed metadata about where to get the vertex data, the index is for the specific triangle
Triangle getTriangle(MeshInfo meshInfo, uint primitiveIndex) {
    Triangle tri = Triangle(vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec3(0.0), vec2(0.0), vec2(0.0), vec2(0.0));
    BufferMetadata bufferMetadata = u_bufferMetadata.AllBufferMetadata[meshInfo.bufferMetadataIDX];
    uint64_t vboHandle = bufferMetadata.VBOHandle;
    uint64_t iboHandle = bufferMetadata.IBOHandle;
    uint indexType = bufferMetadata.indexType;
    uint indexSize = indexType == UNSIGNED_INT ? 4 : 2;


    uint indexOffset = (meshInfo.indexOffsetBytes/indexSize) + primitiveIndex * 3;

    uint indices[3];
    
    // Read indices based on indexType
    if (indexType == UNSIGNED_INT) { // GL_UNSIGNED_INT
        uint *iboPtr = (uint *)iboHandle;
        indices[0] = iboPtr[indexOffset];
        indices[1] = iboPtr[indexOffset + 1];
        indices[2] = iboPtr[indexOffset + 2];
    } else { // Assuming GL_UNSIGNED_SHORT (5123)
        // Requires GL_EXT_shader_explicit_arithmetic_types or similar for uint16_t
        uint16_t *iboPtr = (uint16_t *)iboHandle;
        indices[0] = uint(iboPtr[indexOffset]);     // Cast uint16_t to uint
        indices[1] = uint(iboPtr[indexOffset + 1]); // Cast uint16_t to uint
        indices[2] = uint(iboPtr[indexOffset + 2]); // Cast uint16_t to uint
    }

    uvec3 vertexStartByteOffset;
    vertexStartByteOffset[0] = meshInfo.vertexOffsetBytes + (indices[0] * bufferMetadata.vertexStrideBytes);
    vertexStartByteOffset[1] = meshInfo.vertexOffsetBytes + (indices[1] * bufferMetadata.vertexStrideBytes);
    vertexStartByteOffset[2] = meshInfo.vertexOffsetBytes + (indices[2] * bufferMetadata.vertexStrideBytes);

    // Calculate the absolute byte offset for the start of the position attribute within that vertex
    uvec3 positionStartByteOffset;
    positionStartByteOffset[0] = vertexStartByteOffset[0] + bufferMetadata.positionAttributeOffsetBytes;
    positionStartByteOffset[1] = vertexStartByteOffset[1] + bufferMetadata.positionAttributeOffsetBytes;
    positionStartByteOffset[2] = vertexStartByteOffset[2] + bufferMetadata.positionAttributeOffsetBytes;

    uvec3 normalStartByteOffset;
    normalStartByteOffset[0] = vertexStartByteOffset[0] + bufferMetadata.normalAttributeOffsetBytes;
    normalStartByteOffset[1] = vertexStartByteOffset[1] + bufferMetadata.normalAttributeOffsetBytes;
    normalStartByteOffset[2] = vertexStartByteOffset[2] + bufferMetadata.normalAttributeOffsetBytes;

    uvec3 tangentStartByteOffset;
    tangentStartByteOffset[0] = vertexStartByteOffset[0] + bufferMetadata.tangentAttributeOffsetBytes;
    tangentStartByteOffset[1] = vertexStartByteOffset[1] + bufferMetadata.tangentAttributeOffsetBytes;
    tangentStartByteOffset[2] = vertexStartByteOffset[2] + bufferMetadata.tangentAttributeOffsetBytes;

    uvec3 textureStartByteOffset;
    textureStartByteOffset[0] = vertexStartByteOffset[0] + bufferMetadata.texCoordAttributeOffsetBytes;
    textureStartByteOffset[1] = vertexStartByteOffset[1] + bufferMetadata.texCoordAttributeOffsetBytes;
    textureStartByteOffset[2] = vertexStartByteOffset[2] + bufferMetadata.texCoordAttributeOffsetBytes;

    // dont need to divide by 4 because we will read from the buffer per byte
    //positionStartByteOffset = positionStartByteOffset;
    //textureStartByteOffset = textureStartByteOffset;

    // Access vertex data using byte offsets and pointer casting
    // Conceptual base byte pointer from the VBO handle
    // Using uint8_t* requires an appropriate extension, but the arithmetic works
    // by adding byte offsets to the base uint64_t handle.
    uint8_t *vboBasePtr = (uint8_t *)vboHandle;

    
    float v0x = *((float *)(vboBasePtr + positionStartByteOffset[0] + 0)); // Offset 12 is 4-byte aligned
    float v0y = *((float *)(vboBasePtr + positionStartByteOffset[0] + 4)); // Offset 16 is 4-byte aligned
    float v0z = *((float *)(vboBasePtr + positionStartByteOffset[0] + 8)); // Offset 20 is 4-byte aligned
    vec3 v0_local = vec3(v0x, v0y, v0z);

    float n0x = *((float *)(vboBasePtr + normalStartByteOffset[0] + 0)); // Offset 12 is 4-byte aligned
    float n0y = *((float *)(vboBasePtr + normalStartByteOffset[0] + 4)); // Offset 16 is 4-byte aligned
    float n0z = *((float *)(vboBasePtr + normalStartByteOffset[0] + 8)); // Offset 20 is 4-byte aligned
    vec3 n0_local = vec3(n0x, n0y, n0z);

    float t0x = *((float *)(vboBasePtr + tangentStartByteOffset[0] + 0)); // Offset 12 is 4-byte aligned
    float t0y = *((float *)(vboBasePtr + tangentStartByteOffset[0] + 4)); // Offset 16 is 4-byte aligned
    float t0z = *((float *)(vboBasePtr + tangentStartByteOffset[0] + 16)); // Offset 20 is 4-byte aligned
    vec3 t0_local = vec3(t0x, t0y, t0z);

    vec2 uv0  = *((vec2 *)(vboBasePtr + textureStartByteOffset[0])); // Offset 40 is 8-byte aligned (OK for vec2)
    
    // --- Vertex 1 ---
    float v1x = *((float *)(vboBasePtr + positionStartByteOffset[1] + 0));
    float v1y = *((float *)(vboBasePtr + positionStartByteOffset[1] + 4));
    float v1z = *((float *)(vboBasePtr + positionStartByteOffset[1] + 8));
    vec3 v1_local = vec3(v1x, v1y, v1z);

    float n1x = *((float *)(vboBasePtr + normalStartByteOffset[1] + 0)); // Offset 12 is 4-byte aligned
    float n1y = *((float *)(vboBasePtr + normalStartByteOffset[1] + 4)); // Offset 16 is 4-byte aligned
    float n1z = *((float *)(vboBasePtr + normalStartByteOffset[1] + 8)); // Offset 20 is 4-byte aligned
    vec3 n1_local = vec3(n1x, n1y, n1z);

    float t1x = *((float *)(vboBasePtr + tangentStartByteOffset[1] + 0)); // Offset 12 is 4-byte aligned
    float t1y = *((float *)(vboBasePtr + tangentStartByteOffset[1] + 4)); // Offset 16 is 4-byte aligned
    float t1z = *((float *)(vboBasePtr + tangentStartByteOffset[1] + 8)); // Offset 20 is 4-byte aligned
    vec3 t1_local = vec3(t1x, t1y, t1z);

    vec2 uv1  = *((vec2 *)(vboBasePtr + textureStartByteOffset[1]));

    // --- Vertex 2 ---
    float v2x = *((float *)(vboBasePtr + positionStartByteOffset[2] + 0));
    float v2y = *((float *)(vboBasePtr + positionStartByteOffset[2] + 4));
    float v2z = *((float *)(vboBasePtr + positionStartByteOffset[2] + 8));
    vec3 v2_local = vec3(v2x, v2y, v2z);
    
    float n2x = *((float *)(vboBasePtr + normalStartByteOffset[2] + 0)); // Offset 12 is 4-byte aligned
    float n2y = *((float *)(vboBasePtr + normalStartByteOffset[2] + 4)); // Offset 16 is 4-byte aligned
    float n2z = *((float *)(vboBasePtr + normalStartByteOffset[2] + 8)); // Offset 20 is 4-byte aligned
    vec3 n2_local = vec3(n2x, n2y, n2z);
    
    float t2x = *((float *)(vboBasePtr + tangentStartByteOffset[2] + 0)); // Offset 12 is 4-byte aligned
    float t2y = *((float *)(vboBasePtr + tangentStartByteOffset[2] + 4)); // Offset 16 is 4-byte aligned
    float t2z = *((float *)(vboBasePtr + tangentStartByteOffset[2] + 8)); // Offset 20 is 4-byte aligned
    vec3 t2_local = vec3(t2x, t2y, t2z);

    vec2 uv2  = *((vec2 *)(vboBasePtr + textureStartByteOffset[2]));

    
    tri.v0 = (meshInfo.Transform * vec4(v0_local, 1.0)).xyz;
    tri.v1 = (meshInfo.Transform * vec4(v1_local, 1.0)).xyz;
    tri.v2 = (meshInfo.Transform * vec4(v2_local, 1.0)).xyz;

    tri.n0 = (meshInfo.Transform * vec4(n0_local, 0.0)).xyz;
    tri.n1 = (meshInfo.Transform * vec4(n1_local, 0.0)).xyz;
    tri.n2 = (meshInfo.Transform * vec4(n2_local, 0.0)).xyz;

    tri.t0 = (meshInfo.Transform * vec4(t0_local, 0.0)).xyz;
    tri.t1 = (meshInfo.Transform * vec4(t1_local, 0.0)).xyz;
    tri.t2 = (meshInfo.Transform * vec4(t2_local, 0.0)).xyz;

    
    tri.uv0 = uv0;
    tri.uv1 = uv1;
    tri.uv2 = uv2;
    

    return tri;
}

// Returns true if there's an intersection, and outputs the barycentric coordinates and distance
bool intersectTriangle(Ray ray, Triangle tri, out vec2 barycentricUV, out float t) {
    const float EPSILON = 0.001;
    
    t=INFINITY_FLOAT;
    barycentricUV = vec2(0.0, 0.0);

    
    // Calculate edges
    vec3 edge1 = tri.v1 - tri.v0;
    vec3 edge2 = tri.v2 - tri.v0;
    
    // Calculate P vector
    vec3 P = cross(ray.direction, edge2);
    // Calculate determinant
    float det = dot(edge1, P);
    
    // If determinant is near zero, ray lies in plane of triangle
    if (abs(det) < EPSILON) {
        return false;
    }
    
    float invDet = 1.0 / det;
    // Calculate distance from v0 to ray origin
    vec3 T = ray.origin - tri.v0;
    // Calculate u parameter
    float u = dot(T, P) * invDet;
    
    // Check bounds
    if (u < 0.0 || u > 1.0) {
        return false;
    }
    
    // Calculate Q vector
    vec3 Q = cross(T, edge1);
    // Calculate v parameter
    float v = dot(ray.direction, Q) * invDet;
    
    // Check bounds
    if (v < 0.0 || u + v > 1.0) {
        return false;
    }
    
    // Calculate t (distance along ray)
    t = dot(edge2, Q) * invDet;
    // Ensure hit point is in front of ray origin
    if (t < EPSILON) {
        return false;
    }
    
    barycentricUV = vec2(u, v);
    return true;
}

// Usage example for texture sampling:
vec2 interpolateUV(vec2 barycentricUV, Triangle tri) {
    float u = barycentricUV.x;
    float v = barycentricUV.y;
    float w = 1.0 - u - v;  // Third barycentric coordinate
    
    // Interpolate texture coordinates
    return normalize(tri.uv0 * w + tri.uv1 * u + tri.uv2 * v);
}

// Interpolates world-space normals using barycentric coordinates
vec3 interpolateNormal(vec2 barycentricUV, Triangle tri) {
    float u = barycentricUV.x;
    float v = barycentricUV.y;
    float w = 1.0 - u - v;  // Third barycentric coordinate

    return normalize(tri.n0 * w + tri.n1 * u + tri.n2 * v); 
}

vec3 interpolateTangent(vec2 barycentricUV, Triangle tri) {
    float u = barycentricUV.x;
    float v = barycentricUV.y;
    float w = 1.0 - u - v;
    return normalize(tri.t0 * w + tri.t1 * u + tri.t2 * v);
}

vec2 octEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 encoded = (n.z >= 0.0) ? n.xy : vec2(
        (1.0 - abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0),
        (1.0 - abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0)
    );
    return encoded * 0.5 + 0.5;
}

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



bool RayBoundingBoxDst(Ray ray, vec3 aabbMin, vec3 aabbMax, out float tmin_out) {
    vec3 tMin = (aabbMin - ray.origin) * ray.invDir;
	vec3 tMax = (aabbMax - ray.origin) * ray.invDir;

	//vec3 t1 = min(tMin, tMax);
	//vec3 t2 = max(tMin, tMax);

    vec3 t1 = vec3(
        min(tMin.x, tMax.x),
        min(tMin.y, tMax.y),
        min(tMin.z, tMax.z)
    );
    vec3 t2 = vec3(
        max(tMin.x, tMax.x),
        max(tMin.y, tMax.y),
        max(tMin.z, tMax.z)
    );

	float tNear = max(max(t1.x, t1.y), t1.z);
	float tFar = min(min(t2.x, t2.y), t2.z);

	bool hit = tFar >= tNear && tFar > 0;
	tmin_out = hit ? tNear > 0 ? tNear : 0 : INFINITY_FLOAT;

    return hit;

    
}

#define INVALID_POINTER 0x0

// Iteratively traverses the BVH for a given ray.
// Returns the index of the first leaf node encountered whose AABB is intersected by the ray,
// prioritizing traversal towards closer AABBs. Returns -1 if no leaf node AABB is hit.
bool traceBVH(Ray ray, uint index, out BVHNode result) {
    uint currentNodeIndex = 0; 

    // stack
    #define MAX_STACK_SIZE 128
    uint nodeStack[MAX_STACK_SIZE];
    uint stackPointer = 0;
    nodeStack[stackPointer++] = index;


    while (stackPointer > 0) {
        currentNodeIndex = nodeStack[--stackPointer];
        BVHNode node = u_lbvh.lbvh[currentNodeIndex];

        float tHitCurrent;
        if (RayBoundingBoxDst(ray, node.aabbMin, node.aabbMax, tHitCurrent)) {

            if (node.left == INVALID_POINTER && node.right == INVALID_POINTER) {

                result = node; // should check the triangle intersection here

                return true;

            } else {
                uint childIndexA = node.left+index;
                uint childIndexB = node.right+index;

                BVHNode childA = u_lbvh.lbvh[childIndexA];
                BVHNode childB = u_lbvh.lbvh[childIndexB];

                float dstA;
                float dstB;
                bool hitA = RayBoundingBoxDst(ray, childA.aabbMin, childA.aabbMax, dstA);
                bool hitB = RayBoundingBoxDst(ray, childB.aabbMin, childB.aabbMax, dstB);

                // Determine order for stack push (process nearer first)
                uint childIndexNear, childIndexFar;
                bool nearHit, farHit;

                if (hitA && hitB) {
                    if (dstA < dstB) {
                        childIndexNear = childIndexA; nearHit = true;
                        childIndexFar = childIndexB; farHit = true;
                    } else {
                        childIndexNear = childIndexB; nearHit = true;
                        childIndexFar = childIndexA; farHit = true;
                    }
                } else if (hitA) {
                    childIndexNear = childIndexA; nearHit = true;
                    childIndexFar = childIndexB; farHit = false; // Mark far as miss
                } else if (hitB) {
                    childIndexNear = childIndexB; nearHit = true;
                    childIndexFar = childIndexA; farHit = false; // Mark far as miss
                } else {
                    continue; // Neither child hit
                }

                // Push far child first if hit and stack has space (avoids overflow)
                if (farHit && stackPointer < MAX_STACK_SIZE) {
                    nodeStack[stackPointer++] = childIndexFar;
                }
                // Push near child if hit and stack has space (avoids overflow)
                if (nearHit && stackPointer < MAX_STACK_SIZE) {
                    nodeStack[stackPointer++] = childIndexNear;
                }
            }
        }
    }

    return false;
}


// Placeholder: Samples albedo texture using bindless handle
// Requires GL_ARB_bindless_texture
vec3 sampleAlbedo(uint meshIndex, vec2 uv) {
    MeshInfo meshInfo = u_sceneInfo.MeshInfos[meshIndex];
    uint64_t albedoTextureHandle = meshInfo.AlbedoTextureHandle;
    if (albedoTextureHandle == 0) {
        return vec3(1.0, 0.0, 1.0); // Return constant grey for now
    }

    sampler2D albedoSampler = sampler2D(albedoTextureHandle);
    return texture(albedoSampler, uv).rgb;
}

vec3 sampleNormal(uint meshIndex, vec2 uv) {
    MeshInfo meshInfo = u_sceneInfo.MeshInfos[meshIndex];
    uint64_t normalTextureHandle = meshInfo.NormalTextureHandle;
    if (normalTextureHandle == 0) {
        return vec3(0.0, 0.0, 0.0); // Return constant grey for now
    }

    sampler2D normalSampler = sampler2D(normalTextureHandle);
    return texture(normalSampler, uv).rgb;
}

// Calculates the final world-space shading normal using interpolated TBN and normal map
vec3 calculateShadingNormal(
    uint meshIndex,
    vec2 uv,
    vec3 N_geom,   // Interpolated geometric normal (world space)
    vec3 T_geom    // Interpolated tangent (world space)
) {
    MeshInfo meshInfo = u_sceneInfo.MeshInfos[meshIndex];
    vec3 finalNormal = N_geom; // Default to geometric normal

    // Check if a normal map exists for this mesh
    if (meshInfo.NormalTextureHandle != 0) {
        // Sample the normal map
        sampler2D normalSampler = sampler2D(meshInfo.NormalTextureHandle);
        vec3 tangentNormal = texture(normalSampler, uv).xyz;

        // Unpack the normal from [0, 1] to [-1, 1]
        tangentNormal = normalize(tangentNormal * 2.0 - 1.0);

        // Gram-Schmidt orthogonalization to create a robust TBN basis
        // Ensure T is orthogonal to N
        vec3 T = normalize(T_geom - dot(T_geom, N_geom) * N_geom);
        // Calculate Bitangent B (orthogonal to N and T)
        // The direction can depend on UV orientation (handedness), often stored implicitly or with tangent.w
        // Assuming standard right-handed:
        vec3 B = normalize(cross(N_geom, T));

        // Create TBN matrix (transpose of standard view matrix math)
        mat3 TBN = mat3(T, B, N_geom);

        // Transform tangent-space normal to world space
        finalNormal = normalize(TBN * tangentNormal);
    }

    return finalNormal;
}


vec3 texelToProbeIndex(vec2 texelCoord, vec2 probeResolution, vec3 probeGridDimensions, uint probesPerRow) {
    // Calculate which probe grid cell the texel belongs to (in the 2D atlas layout)
    uint atlasProbeCoordX = uint(floor(texelCoord.x / probeResolution.x));
    uint atlasProbeCoordY = uint(floor(texelCoord.y / probeResolution.y));

    // Calculate the linear index of the probe based on its 2D position in the atlas grid
    uint linearAtlasIndex = atlasProbeCoordY * probesPerRow + atlasProbeCoordX;

    // De-linearize the index back into 3D probe grid coordinates (X, Y, Z)
    // This assumes the 3D grid was linearized as: Z * (dimX * dimY) + Y * dimX + X
    uint probeIndexX = linearAtlasIndex % uint(probeGridDimensions.x);

    uint tempIndex = uint(floor(float(linearAtlasIndex) / probeGridDimensions.x));
    uint probeIndexY = tempIndex % uint(probeGridDimensions.y);
    uint probeIndexZ = uint(floor(float(tempIndex) / probeGridDimensions.y));

    return vec3(probeIndexX, probeIndexY, probeIndexZ);
}

vec2 normalizeTexelCoord(vec2 texelCoord, vec2 probeResolution) {
    return vec2(texelCoord.x / probeResolution.x, texelCoord.y / probeResolution.y);
}



void main() {
    // texel in the atlas
    ivec2 globalIndex2D = ivec2(gl_GlobalInvocationID.xy);



    // get the probe related to the texel 
    // DEBUG: OK
    vec3 probeIndex = texelToProbeIndex(globalIndex2D, probeResolution, probeGridDimensions, u_probesPerRow);


    //get probe position
    // Calculate the total size of the grid and the starting offset
    vec3 totalGridSize = vec3(probeGridDimensions) * probeSpacing;
    vec3 startOffset = probeOrigin - (totalGridSize / 2.0f) + (probeSpacing / 2.0f); // Offset by half spacing to center probes
    // Calculate the position based on the starting offset and index
    vec3 probePosition = startOffset + probeIndex * probeSpacing;

    //vec3 maxProbePosition = probeOrigin + probeGridDimensions * probeSpacing;


    // Calculate LOCAL texel coordinates within the current probe tile
    ivec2 localTexelCoord = globalIndex2D % ivec2(probeResolution);
    // Normalize local coordinates to [0, 1) for octDecode input
    vec2 normLocalTexCoord = vec2(localTexelCoord) / vec2(probeResolution);

    // Decode ray direction using correctly normalized local coordinates
    // DEBUG: OK
    vec3 rayDirection = octDecode(normLocalTexCoord); 
    vec3 invDir = 1.0 / rayDirection;



    Ray ray = Ray(probePosition, rayDirection, invDir);
    BVHNode hitNode; // Renamed from 'result' to avoid conflict
    vec3 calculatedIrradiance = vec3(0.0); // Store the calculated light energy here

    // Structure to store the closest hit information found across all meshes
    HitInfo closestHitInfo;
    closestHitInfo.hit = false;
    closestHitInfo.t = INFINITY_FLOAT; // Initialize distance to max float (infinity)
    
#ifdef DEBUG_OUTPUT_BUFFER
    DebugData debugData;
    debugData.leafHits = 0;
    debugData.triangleHits = 0;
    debugData.closestHit = 0;
    debugData.closestHitMeshIndex = 0;
#endif

    for (int i = 0; i < u_meshCount; i++) {
        MeshInfo meshInfo = u_sceneInfo.MeshInfos[i];
        uint rootIndex = meshInfo.RootIndex;
        

        // --- Transform Ray to Mesh Local Space for BVH Traversal ---
        mat4 invTransform = inverse(meshInfo.Transform);
        vec4 localRayOrigin4 = invTransform * vec4(ray.origin, 1.0);
        vec3 localRayOrigin = localRayOrigin4.xyz / localRayOrigin4.w;
        vec3 localRayDirection = normalize((invTransform * vec4(ray.direction, 0.0)).xyz);
        vec3 localInvDir = 1.0 / localRayDirection;
        Ray localRay = Ray(localRayOrigin, localRayDirection, localInvDir);
        // -----------------------------------------------------------
    

        // Trace using the local space ray against the local space BVH AABBs
        if (traceBVH(localRay, rootIndex, hitNode)) {
#ifdef DEBUG_OUTPUT_BUFFER
            debugData.leafHits = debugData.leafHits + 1;
#endif

            Triangle tri = getTriangle(meshInfo, hitNode.primitiveIdx);

            vec2 barycentricUV;
            float t=INFINITY_FLOAT;
            imageStore(probeAtlas, globalIndex2D, vec4(0.0, 0.0, 1.0, 1.0));


            if (intersectTriangle(ray, tri, barycentricUV, t)) {
#ifdef DEBUG_OUTPUT_BUFFER
                debugData.triangleHits = debugData.triangleHits + 1;
#endif


                if (t < closestHitInfo.t) {
                    // Yes, this is the new closest hit.
                    closestHitInfo.hit = true;
                    closestHitInfo.t = t;
                    closestHitInfo.barycentricUV = barycentricUV;
                    closestHitInfo.primitiveIndex = hitNode.primitiveIdx; // Store primitive index relative to this mesh
                    closestHitInfo.meshIndex = i;
                }
            }
        }
    }

    // --- Determine Final Color (After checking ALL meshes) ---
    if (closestHitInfo.hit) {

        // Get the mesh info for the mesh that contained the closest hit.
        MeshInfo closestMeshInfo = u_sceneInfo.MeshInfos[closestHitInfo.meshIndex];
        // Get the specific triangle from that mesh using the stored primitive index.
        Triangle closestTri = getTriangle(closestMeshInfo, closestHitInfo.primitiveIndex);

        // Interpolate UV coordinates using the stored barycentrics.
        vec2 uv = interpolateUV(closestHitInfo.barycentricUV, closestTri);
        vec3 worldNormal = interpolateNormal(closestHitInfo.barycentricUV, closestTri);
        vec3 worldTangent = interpolateTangent(closestHitInfo.barycentricUV, closestTri);

        vec3 worldShadingNormal = calculateShadingNormal(
            closestHitInfo.meshIndex,
            uv,
            worldNormal,
            worldTangent
        );

        // Sample the albedo texture using the mesh index and interpolated UVs.
        vec3 albedo = sampleAlbedo(closestHitInfo.meshIndex, uv);

        float NdotL = max(0.0, dot(worldShadingNormal, -normalize(u_SunLight.direction))); // Light direction points TO the light
        vec3 directLighting = albedo * u_SunLight.intensity * NdotL / PI;

        // --- Calculate Indirect Lighting (Placeholder - Add later) ---
        // vec3 indirectLighting = samplePreviousProbes(hitPosition, worldNormal);
        vec3 indirectLighting = vec3(0.0); // No indirect yet

        // Combine lighting contributions
        // This represents the light reflecting OFF the surface TOWARDS the probe
        calculatedIrradiance = directLighting + indirectLighting;

#ifdef DEBUG_OUTPUT_BUFFER
        debugData.closestHit = closestHitInfo.t;
        debugData.closestHitMeshIndex = closestHitInfo.meshIndex;
#endif

         // --- Debug Views (Optional - Add #ifdef blocks if needed) ---
        // e.g., finalColor = vec3(0.0, 1.0, 0.0); // Green for hit

    } else {
        // Ray missed all geometry, sample from the skybox cubemap
        // The ray direction is already normalized from octDecode
        calculatedIrradiance = texture(u_skyboxCubemap, ray.direction).rgb;
    }


    vec2 currentDepthData;
    if (closestHitInfo.hit) {
        float hitDistance = closestHitInfo.t;
        // Clamp distance to avoid potential issues with huge numbers if geometry is very far
        hitDistance = min(hitDistance, 10000.0); // Adjust max distance as needed
        currentDepthData = vec2(hitDistance, hitDistance * hitDistance);
    } else {
        // Use defined infinity for miss
        currentDepthData = vec2(INFINITY_FLOAT, INFINITY_FLOAT);
    }

    // --- Hysteresis for Depth/Visibility ---
    vec2 oldDepthData = imageLoad(prevProbeDepthAtlas, globalIndex2D).rg;
    vec2 finalDepthData;

    // Handle potential NaN/Inf from previous frames or initial state
    // If old data is invalid (e.g., Inf), use current data directly. Otherwise, blend.
    if (isinf(oldDepthData.x) || isnan(oldDepthData.x)) {
         finalDepthData = currentDepthData;
    } else {
         // Blend using mix: mix(newValue, oldValue, hysteresisFactor)
         // Ensure currentDepthData is not Inf before mixing if old data was valid
         if(isinf(currentDepthData.x)) {
            finalDepthData = currentDepthData; // If new hit is Inf, override directly
         } else {
            finalDepthData = mix(currentDepthData, oldDepthData, u_hysteresis);
         }
    }

    imageStore(probeDepthAtlas, globalIndex2D, vec4(finalDepthData, 0.0, 0.0));


    // --- Store Final Color (Radiance Atlas) ---
    // Apply hysteresis to color too (assuming you want temporal blending for lighting)
    vec3 oldColor = imageLoad(prevProbeAtlas, globalIndex2D).rgb; // Requires probeAtlas to be read/write too
    vec3 finalBlendedIrradiance;
    
    if (dot(oldColor, oldColor) < 0.00001) { // Or check against miss color
      finalBlendedIrradiance = calculatedIrradiance;
    } else {
      finalBlendedIrradiance = mix(calculatedIrradiance, oldColor, u_hysteresis);
    }

    finalBlendedIrradiance = max(finalBlendedIrradiance, vec3(0.0)); 
    imageStore(probeAtlas, globalIndex2D, vec4(finalBlendedIrradiance, 1.0)); 


#ifdef DEBUG_OUTPUT_BUFFER
    uint indexID = uint(globalIndex2D.y * u_AtlasTextureResolution.x + globalIndex2D.x);
    u_debugData[indexID] = debugData;
#endif

}







