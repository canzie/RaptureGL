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
// #define DEBUG_OUTPUT_BUFFER


layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;


// Output Images
layout (binding = 0, r11f_g11f_b10f) uniform restrict writeonly image2D probeAtlas;
layout (binding = 1, rg16f) uniform restrict writeonly image2D probeDepthAtlas;

layout (binding = 2, r11f_g11f_b10f) uniform restrict readonly image2D prevProbeAtlas;
layout (binding = 3, rg16f) uniform restrict readonly image2D prevProbeDepthAtlas;

// Skybox Cubemap
layout (binding = 4) uniform samplerCube u_skyboxCubemap;

precision highp float;


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
    mat4 InvTransform;
};

layout(std430, binding = 2) readonly buffer BufferMetadataStorage {
    BufferMetadata AllBufferMetadata[];
} u_bufferMetadata;

layout(std430, binding = 3) readonly buffer SceneInfo {
    MeshInfo MeshInfos[];
} u_sceneInfo;


struct Triangle {
    vec3 v0;
    vec3 v1;
    vec3 v2;  // Vertices in world space
    vec3 n0;
    vec3 n1;
    vec3 n2;  // Normals in world space
    vec3 t0;
    vec3 t1;
    vec3 t2;  // Tangents in world space
    vec2 uv0;
    vec2 uv1;
    vec2 uv2; // Texture coordinates
};

struct Ray {
    vec3 origin;
    vec3 direction;
    vec3 invDir;
};


#ifdef DEBUG_OUTPUT_BUFFER

    struct DebugData {
        uint primitiveIndex;
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



// Placeholder struct for trace result
struct HitInfo {
    bool hit;
    float t;            // Hit distance
    vec2 barycentricUV; // Barycentric coords at hit point
    uint primitiveIndex; // Index within the mesh's index buffer (e.g., first index of the triangle)
    uint meshIndex;      // Index into u_sceneInfo.MeshInfos array
    Triangle tri;
    vec3 hitPosition;
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
    vertexStartByteOffset[0] = meshInfo.vertexOffsetBytes + (indices[0] * bufferMetadata.vertexStrideBytes);
    vertexStartByteOffset[1] = meshInfo.vertexOffsetBytes + (indices[1] * bufferMetadata.vertexStrideBytes);
    vertexStartByteOffset[2] = meshInfo.vertexOffsetBytes + (indices[2] * bufferMetadata.vertexStrideBytes);

    // Calculate the absolute byte offset for the start of the position attribute within that vertex
    uvec3 positionStartByteOffset = vertexStartByteOffset + bufferMetadata.positionAttributeOffsetBytes;

    uvec3 normalStartByteOffset = vertexStartByteOffset + bufferMetadata.normalAttributeOffsetBytes;

    uvec3 tangentStartByteOffset = vertexStartByteOffset + bufferMetadata.tangentAttributeOffsetBytes;

    uvec3 textureStartByteOffset =vertexStartByteOffset + bufferMetadata.texCoordAttributeOffsetBytes;

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
    float t0z = *((float *)(vboBasePtr + tangentStartByteOffset[0] + 8)); // Offset 20 is 4-byte aligned
    vec3 t0_local = vec3(t0x, t0y, t0z);

    float uv0x = *((float *)(vboBasePtr + textureStartByteOffset[0] + 0));
    float uv0y = *((float *)(vboBasePtr + textureStartByteOffset[0] + 4));
    vec2 uv0  = vec2(uv0x, uv0y);
    
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

    float uv1x = *((float *)(vboBasePtr + textureStartByteOffset[1] + 0));
    float uv1y = *((float *)(vboBasePtr + textureStartByteOffset[1] + 4));
    vec2 uv1  = vec2(uv1x, uv1y);

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

    float uv2x = *((float *)(vboBasePtr + textureStartByteOffset[2] + 0));
    float uv2y = *((float *)(vboBasePtr + textureStartByteOffset[2] + 4));
    vec2 uv2  = vec2(uv2x, uv2y);

    
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
    

    // Calculate edges
    vec3 edgeAB = tri.v1 - tri.v0;
    vec3 edgeAC = tri.v2 - tri.v0;
    
    vec3 normalVec = cross(edgeAB, edgeAC);

    // Backface culling: if the ray hits from behind, ignore intersection
    //if (dot(ray.direction, normalVec) > 0.0) {
    //    return false;
    //}
    
    vec3 ao = ray.origin - tri.v0;
    vec3 dao = cross(ao, ray.direction);

    float determinant = -dot(ray.direction, normalVec);
    float invDet = 1.0/determinant;

    float dst = dot(ao, normalVec) * invDet;
    float u = dot(edgeAC, dao) * invDet;
    float v = -dot(edgeAB, dao) * invDet;
    float w = 1.0 - u - v;

    t = dst;
    barycentricUV = vec2(u, v);

    // Two-sided intersection test
    if (abs(determinant) < 0.000001) { // Ray is parallel to triangle plane or determinant is zero
        return false;
    }

    // Check if hit is in front of ray and within triangle bounds.
    // Using a small epsilon for barycentric coordinates and distance for robustness.
    bool hitValid = (dst >= 0.00001) &&                 // Hit must be in front of the ray (t > epsilon)
                    (u >= -0.00001) && (u <= 1.00001) && // u must be approx in [0,1]
                    (v >= -0.00001) && (v <= 1.00001) && // v must be approx in [0,1]
                    ((u + v) <= 1.00001);                // u + v must be approx <= 1 (ensures w approx >= 0)
    
    return hitValid;
}


// Usage example for texture sampling:
vec2 interpolateUV(vec2 barycentricUV, Triangle tri) {
    float u = barycentricUV.x;
    float v = barycentricUV.y;
    float w = 1.0 - u - v;  // Third barycentric coordinate
    
    // Interpolate texture coordinates
    return tri.uv0 * w + tri.uv1 * u + tri.uv2 * v;
}

// Interpolates world-space normals using barycentric coordinates
vec3 interpolateNormal(vec2 barycentricUV, Triangle tri) {
    float u = barycentricUV.x;
    float v = barycentricUV.y;
    float w = 1.0 - u - v;  // Third barycentric coordinate

    return tri.n0 * w + tri.n1 * u + tri.n2 * v; 
}

vec3 interpolateTangent(vec2 barycentricUV, Triangle tri) {
    float u = barycentricUV.x;
    float v = barycentricUV.y;
    float w = 1.0 - u - v;
    return tri.t0 * w + tri.t1 * u + tri.t2 * v;
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


HitInfo traceBVH(Ray ray, Ray localRay, uint rootIndexOffset, MeshInfo meshInfo) {
    int currentNodeIndex = 0; 

    HitInfo result;
    result.hit = false;
    result.t = INFINITY_FLOAT;
    result.primitiveIndex = 0;
    result.barycentricUV = vec2(0.0);

    // stack
    #define MAX_STACK_SIZE 128
    int nodeStack[MAX_STACK_SIZE];
    uint stackPointer = 0;
    nodeStack[stackPointer++] = int(rootIndexOffset);


    while (stackPointer > 0) {
        currentNodeIndex = nodeStack[--stackPointer];
        BVHNode node = u_lbvh.lbvh[currentNodeIndex];

        if (node.left == INVALID_POINTER && node.right == INVALID_POINTER) {
            Triangle tri = getTriangle(meshInfo, node.primitiveIdx);
            vec2 barycentricUV;
            float t;
            bool hit = intersectTriangle(ray, tri, barycentricUV, t);
            vec3 hitPosition = ray.origin + ray.direction * t;

            if (hit && t < result.t) {
                result.hit = true;
                result.t = t;
                result.primitiveIndex = node.primitiveIdx;
                result.barycentricUV = barycentricUV;
                result.tri = tri;
            } 
        }  else {
            int childIndexA = int(node.left+rootIndexOffset);
            int childIndexB = int(node.right+rootIndexOffset);
            BVHNode childA = u_lbvh.lbvh[childIndexA];
            BVHNode childB = u_lbvh.lbvh[childIndexB];

            float dstA;
            float dstB;
            bool hitA = RayBoundingBoxDst(localRay, childA.aabbMin, childA.aabbMax, dstA);
            bool hitB = RayBoundingBoxDst(localRay, childB.aabbMin, childB.aabbMax, dstB);
            
            bool isNearestA = dstA <= dstB;
            float dstNear = isNearestA ? dstA : dstB;
            float dstFar = isNearestA ? dstB : dstA;

            int childIndexNear = isNearestA ? childIndexA : childIndexB;
            int childIndexFar = isNearestA ? childIndexB : childIndexA;

            if (dstFar < result.t) {
                nodeStack[stackPointer++] = childIndexFar;
            }

            if (dstNear < result.t) {
                nodeStack[stackPointer++] = childIndexNear;
            }
            


        }

        
    }

    return result;
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



// Add this function to approximate indirect lighting
vec3 samplePreviousProbes(vec3 hitPosition, vec3 worldNormal) {
    // Simplified: Find the nearest probe and sample its irradiance
    // In a full implementation, interpolate multiple nearby probes
    vec3 gridCoord = (hitPosition - probeOrigin) / probeSpacing;
    ivec3 nearestProbeIdx = ivec3(floor(gridCoord));
    nearestProbeIdx = clamp(nearestProbeIdx, ivec3(0), ivec3(probeGridDimensions) - 1);

    // Convert 3D probe index to 2D atlas position
    uint linearIdx = nearestProbeIdx.z * probeGridDimensions.x * probeGridDimensions.y +
                    nearestProbeIdx.y * probeGridDimensions.x + nearestProbeIdx.x;
    ivec2 atlasBase = ivec2(
        (linearIdx % u_probesPerRow) * probeResolution.x,
        (linearIdx / u_probesPerRow) * probeResolution.y
    );

    // Direction from hit point to probe
    vec3 probePos = probeOrigin + vec3(nearestProbeIdx) * probeSpacing;
    vec3 dirToProbe = normalize(probePos - hitPosition);
    vec2 octCoord = octEncode(dirToProbe);
    ivec2 texelCoord = atlasBase + ivec2(octCoord * probeResolution);

    // Sample previous irradiance
    vec3 indirectRadiance = imageLoad(prevProbeAtlas, texelCoord).rgb;

    // Basic visibility check using depth (optional refinement)
    vec2 prevDepth = imageLoad(prevProbeDepthAtlas, texelCoord).rg;
    float distToProbe = length(probePos - hitPosition);
    float meanDist = prevDepth.x;
    float variance = prevDepth.y - meanDist * meanDist;
    float chebyshevWeight = variance / (variance + pow(distToProbe - meanDist, 2));
    if (distToProbe > meanDist) indirectRadiance *= max(chebyshevWeight, 0.1);

    // Return irradiance (radiance * cosine term approximated in shading)
    return indirectRadiance * max(0.0, dot(worldNormal, dirToProbe));
}

// Add these constants at the top with other defines
#define NUM_RAYS 256
#define GOLDEN_RATIO 1.618033988749895

// Add this function before main()
vec3 fibonacciSpherePoint(uint i, uint n) {
    float phi = 2.0 * PI * float(i) / GOLDEN_RATIO;
    float cosTheta = 1.0 - 2.0 * float(i) / float(n);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    return vec3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );
}

void main() {
    // texel in the atlas
    ivec2 globalIndex2D = ivec2(gl_GlobalInvocationID.xy);

    // get the probe related to the texel
    vec3 probeIndex = texelToProbeIndex(globalIndex2D, probeResolution, probeGridDimensions, u_probesPerRow);

    // Check if the calculated probeIndex is outside the valid grid dimensions
    if (probeIndex.x >= probeGridDimensions.x ||
        probeIndex.y >= probeGridDimensions.y ||
        probeIndex.z >= probeGridDimensions.z) {
        imageStore(probeAtlas, globalIndex2D, vec4(0.0, 0.0, 0.0, 1.0));
        imageStore(probeDepthAtlas, globalIndex2D, vec4(0.0, 0.0, 0.0, 0.0)); 
        return;
    }

    // Calculate probe position
    vec3 totalGridSize = vec3(probeGridDimensions) * probeSpacing;
    vec3 startOffset = probeOrigin - (totalGridSize / 2.0f) + (probeSpacing / 2.0f);
    vec3 probePosition = startOffset + probeIndex * probeSpacing;

    // Calculate LOCAL texel coordinates within the current probe tile
    ivec2 localTexelCoord = globalIndex2D % ivec2(probeResolution);
    
    // Calculate which ray this texel should process
    // We want to distribute NUM_RAYS across all texels in the probe
    uint totalTexelsInProbe = uint(probeResolution.x * probeResolution.y);
    uint rayIndex = uint(localTexelCoord.y * probeResolution.x + localTexelCoord.x);
    
    // If this texel is beyond our ray count, return early
    if (rayIndex >= NUM_RAYS) {
        imageStore(probeAtlas, globalIndex2D, vec4(0.0, 0.0, 0.0, 1.0));
        imageStore(probeDepthAtlas, globalIndex2D, vec4(0.0, 0.0, 0.0, 0.0));
        return;
    }

    // Generate ray direction using Fibonacci sphere
    vec3 rayDirection = fibonacciSpherePoint(rayIndex, NUM_RAYS);
    vec3 invDir = 1.0 / rayDirection;

    Ray ray = Ray(probePosition, rayDirection, invDir);
    HitInfo closestHitInfo;
    closestHitInfo.hit = false;
    closestHitInfo.t = INFINITY_FLOAT;

    // Trace against all meshes
    for (int i = 0; i < u_meshCount; i++) {
        MeshInfo meshInfo = u_sceneInfo.MeshInfos[i];
        uint rootIndex = meshInfo.RootIndex;

        // Transform Ray to Mesh Local Space for BVH Traversal
        mat4 invTransform = meshInfo.InvTransform;
        vec3 localRayOrigin = (invTransform * vec4(ray.origin, 1.0)).xyz;
        vec3 localRayDirection = (invTransform * vec4(ray.direction, 0.0)).xyz;
        vec3 localInvDir = 1.0 / localRayDirection;
        Ray localRay = Ray(localRayOrigin, localRayDirection, localInvDir);

        HitInfo result = traceBVH(ray, localRay, rootIndex, meshInfo);
        result.meshIndex = i;

        if (result.hit && result.t < closestHitInfo.t && result.t < probeSpacing.x * 2.0) {
            closestHitInfo = result;
        }
    }

    vec3 rayIrradiance;
    float rayDistance;
    float rayDistanceSquared;

    // Process hit or miss
    if (closestHitInfo.hit) {
        vec2 uv = interpolateUV(closestHitInfo.barycentricUV, closestHitInfo.tri);
        vec3 worldNormal = interpolateNormal(closestHitInfo.barycentricUV, closestHitInfo.tri);
        vec3 worldTangent = interpolateTangent(closestHitInfo.barycentricUV, closestHitInfo.tri);

        vec3 worldShadingNormal = calculateShadingNormal(
            closestHitInfo.meshIndex,
            uv,
            worldNormal,
            worldTangent
        );

        vec3 albedo = sampleAlbedo(closestHitInfo.meshIndex, uv);
        float NdotL = max(0.0, dot(worldShadingNormal, -normalize(u_SunLight.direction)));

        vec3 indirectLighting = samplePreviousProbes(closestHitInfo.hitPosition, worldShadingNormal);
        rayIrradiance = (albedo / PI) * (u_SunLight.intensity * NdotL + indirectLighting);
        rayDistance = closestHitInfo.t;
        rayDistanceSquared = closestHitInfo.t * closestHitInfo.t;
    } else {
        vec3 skyColor = vec3(1.0, 0.889, 0.669);
        float luminance = dot(skyColor, vec3(0.299, 0.587, 0.114));
        float desaturationFactor = 0.9;
        rayIrradiance = mix(skyColor, vec3(luminance), desaturationFactor);
        
        float skyDistance = 10000.0;
        rayDistance = skyDistance;
        rayDistanceSquared = skyDistance * skyDistance;
    }

    // Store the ray's contribution directly
    imageStore(probeAtlas, globalIndex2D, vec4(rayIrradiance, 1.0));
    imageStore(probeDepthAtlas, globalIndex2D, vec4(rayDistance, rayDistanceSquared, 0.0, 0.0));
}







