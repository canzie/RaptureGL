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


layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;


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
    uvec2 probeResolution; // Resolution of each probe texture (e.g., 8x8) - IMPORTANT for shared memory
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

struct sharedRayData {
    vec3 rayIrradiance;
    vec3 rayDirection;
    float rayVisibility;
}



// Define PI constant
#define PI 3.14159265359
#define PI2 6.28318530718

uniform vec2 u_AtlasTextureResolution; // atlas pixel resolution in screen size, not ndc
uniform uint u_meshCount;
uniform uint u_probesPerRow; // Number of probes along the X-axis of the atlas texture
uniform float u_hysteresis;

// --- Sun Shadow Uniforms for Largest Cascade ---
layout(std140, binding = 1) uniform SunPropertiesUBO {
    mat4 sunLightSpaceMatrix;
    vec3 sunDirectionWorld;       
    uint sunCascadeCount;
    float sunIntensity;
    uint64_t sunShadowTextureArrayHandle; 

} u_SunProperties;

// Placeholder struct for trace result
struct HitInfo {
    bool hit;
    float t;            // Hit distance
    vec2 barycentricUV; // Barycentric coords at hit point
    uint primitiveIndex; // Index within the mesh's index buffer (e.g., first index of the triangle)
    uint meshIndex;      // Index into u_sceneInfo.MeshInfos array
    Triangle tri;
    vec3 hitPosition; // World space hit position
};


#define UNSIGNED_INT 5125
#define UNSIGNED_SHORT 5123
#define INFINITY_FLOAT (1.0/0.0)
#define SKYBOX_DISTANCE 1000.0
#define MAX_DISTANCE 1000.0

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

/*
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
*/

float RTXGISignNotZero(float v)
{
    return (v >= 0.0) ? 1.0 : -1.0;
}
vec2 RTXGISignNotZero2(vec2 v)
{
    return vec2(RTXGISignNotZero(v.x), RTXGISignNotZero(v.y));
}

vec2 octEncode(vec3 n) {
    float l1norm = abs(n.x) + abs(n.y) + abs(n.z);
    vec2 uv = n.xy * (1.0 / l1norm);
    if (n.z < 0.0)
    {
        uv = (1.0 - abs(uv.yx)) * RTXGISignNotZero2(uv.xy);
    }
    return uv;
}

vec3 octDecode(vec2 coords) {
    //vec2 coords = f * 2.0 - 1.0;
    vec3 direction = vec3(coords.x, coords.y, 1.0 - abs(coords.x) - abs(coords.y));
    if (direction.z < 0.0)
    {
        direction.xy = (1.0 - abs(direction.yx)) * RTXGISignNotZero2(direction.xy);
    }
    return normalize(direction);
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

ivec2 getProbeSpecificAtlasUV(uvec3 probePhysicalId3D, vec2 sampleDirOctEncodedUV, uvec2 singleProbeResolution) {
    // 1. Convert 3D probe index to linear 1D index
    uint linearProbeIdx = probePhysicalId3D.z * (probeGridDimensions.x * probeGridDimensions.y) +
                          probePhysicalId3D.y * probeGridDimensions.x +
                          probePhysicalId3D.x;

    // 2. Convert linear 1D index to 2D atlas grid coordinates (which probe tile)
    uint atlasProbeTileX = linearProbeIdx % u_probesPerRow;
    uint atlasProbeTileY = linearProbeIdx / u_probesPerRow;

    // 3. Top-left texel of this specific probe's data in the atlas
    vec2 probeTileOriginPixels = vec2(atlasProbeTileX * singleProbeResolution.x,
                                      atlasProbeTileY * singleProbeResolution.y);



    // 4. Texel coordinate within this probe based on the octahedral encoded sample direction
    // sampleDirOctEncodedUV is already [0,1], scale by probe resolution
    vec2 targetPixelFloatInProbe = sampleDirOctEncodedUV * vec2(singleProbeResolution);
    
    ivec2 localTexelIndices = ivec2(floor(targetPixelFloatInProbe));
    localTexelIndices = clamp(localTexelIndices, 
                              ivec2(0), 
                              ivec2(singleProbeResolution - 1u));



    // Calculate absolute integer pixel coordinates
    ivec2 absoluteAtlasPixelCoord = ivec2(probeTileOriginPixels + localTexelIndices);
    // Clamp to ensure within bounds (though should be if probeResolution/u_probesPerRow are correct)
    absoluteAtlasPixelCoord = clamp(absoluteAtlasPixelCoord, ivec2(0), ivec2(u_AtlasTextureResolution - 1)); 
    return absoluteAtlasPixelCoord;
}


// Add this function to approximate indirect lighting
vec3 samplePreviousProbes(vec3 hitPosition, vec3 worldNormal) {
    // Simplified: Find the nearest probe and sample its irradiance
    // In a full implementation, interpolate multiple nearby probes
    float normalBias = 0.01f; // Consistent normal bias
    vec3 biasedWorldPos = hitPosition + worldNormal * normalBias;

    vec3 centeredGridPos_float = (biasedWorldPos - probeOrigin) / probeSpacing;
    vec3 nearestProbeIndex_0indexed_float = centeredGridPos_float + (vec3(probeGridDimensions - 1u) / 2.0);
    uvec3 nearestProbe_ID = uvec3(clamp(round(nearestProbeIndex_0indexed_float), vec3(0.0), vec3(probeGridDimensions - 1u)));

    vec3 nearestProbeWorldPos = probeOrigin +
                               (vec3(nearestProbe_ID) - (vec3(probeGridDimensions - 1u) / 2.0f)) * probeSpacing;

    vec3 dirFromFragToProbe = nearestProbeWorldPos - biasedWorldPos;
    float distToProbe = length(dirFromFragToProbe);

    if (distToProbe < 0.0001) { // Avoid normalization of zero vector if at probe center
        dirFromFragToProbe = -worldNormal; // Sample direction opposite to surface normal as a fallback
    } else {
        dirFromFragToProbe = normalize(dirFromFragToProbe);
    }



    vec2 octCoord = octEncode(dirFromFragToProbe);
    octCoord = octCoord * 0.5 + 0.5;

    ivec2 probeAtlasUV = getProbeSpecificAtlasUV(nearestProbe_ID, octCoord, probeResolution);

    // Sample previous irradiance
    vec3 indirectRadiance = imageLoad(prevProbeAtlas, probeAtlasUV).rgb;

    // Basic visibility check using depth (optional refinement)
    vec2 prevDepth = imageLoad(prevProbeDepthAtlas, probeAtlasUV).rg;
    float meanDist = prevDepth.x;
    float variance = prevDepth.y - meanDist * meanDist;
    float chebyshevWeight = variance / (variance + pow(distToProbe - meanDist, 2));
    if (distToProbe > meanDist) indirectRadiance *= max(chebyshevWeight, 0.1);

    // Return irradiance (radiance * cosine term approximated in shading)
    return indirectRadiance * max(0.0, dot(worldNormal, dirFromFragToProbe));
}


#define N_RAYS_PER_PROBE 256 // Number of sample rays to cast per probe

// Function to generate fairly uniform points on a sphere using Fibonacci lattice
vec3 getFibonacciSphereSample(uint sampleIndex, uint numSamples) {
    if (numSamples == 0) return vec3(0,0,1); // Should not happen with N_RAYS_PER_PROBE > 0
    if (numSamples == 1) {
        // For a single sample, a consistent direction like Z-up (or Y-up depending on convention)
        // For DDGI, probes sample all directions, so (0,0,1) is arbitrary but consistent.
        return vec3(0.0, 0.0, 1.0);
    }

    float N_f = float(numSamples);
    float k_f = float(sampleIndex);

    float phi_const = PI * (sqrt(5.0f) - 1.0f); // Golden angle in radians

    // y goes from 1 down to -1. max(N_f - 1.0f, 1.0f) prevents division by zero if N_f is 1 (already handled) or 0.
    float y_coord = 1.0f - (k_f / (N_f - 1.0f)) * 2.0f;
    y_coord = clamp(y_coord, -1.0f, 1.0f); // Ensure y is strictly within [-1, 1] for sqrt
    float radius = sqrt(1.0f - y_coord * y_coord);

    float theta = phi_const * k_f;

    float x_coord = cos(theta) * radius;
    float z_coord = sin(theta) * radius;

    // Standard Fibonacci lattice produces points where y is the up-axis for the distribution pattern.
    // Depending on your world coordinate system (Y-up or Z-up), you might permute.
    // Assuming Y is up for the sphere generation pattern, then (x,y,z) is (x_coord, y_coord, z_coord)
    return normalize(vec3(x_coord, y_coord, z_coord)); 
}

/**
 * Computes a low discrepancy spherically distributed direction on the unit sphere,
 * for the given index in a set of samples. Each direction is unique in
 * the set, but the set of directions is always the same.
 */
vec3 RTXGISphericalFibonacci(uint sampleIndex, uint numSamples)
{
    const float b = (sqrt(5.0) * 0.5 + 0.5) - 1.0;
    float phi = PI2 * fract(sampleIndex * b);
    float cosTheta = 1.0 - (2.0 * sampleIndex + 1.0) * (1.0 / numSamples);
    float sinTheta = sqrt(clamp(1.0 - (cosTheta * cosTheta), 0.0, 1.0));

    return vec3((cos(phi) * sinTheta), (sin(phi) * sinTheta), cosTheta);
}

// Shared memory for storing results of the N sample rays per probe
shared vec3 s_sampleRayDirections[N_RAYS_PER_PROBE];
shared vec3 s_sampleRayIrradiance[N_RAYS_PER_PROBE];
shared float s_sampleRayDepth[N_RAYS_PER_PROBE];

// --- Helper function for Sun Shadow Calculation (Largest Cascade) ---
float calculateSunShadowFactor_LargestCascade(vec3 hitPositionWorld, vec3 hitNormalWorld) {
    if (u_SunProperties.sunCascadeCount == 0 || u_SunProperties.sunShadowTextureArrayHandle == 0) return 1.0; // No shadow map or handle is zero

    // Transform hit position to light clip space for the largest cascade
    vec4 hitPosLightSpace = u_SunProperties.sunLightSpaceMatrix * vec4(hitPositionWorld, 1.0);

    // Perform perspective divide
    vec3 projCoords = hitPosLightSpace.xyz / hitPosLightSpace.w;

    // Transform to [0,1] range for texture lookup
    projCoords = projCoords * 0.5 + 0.5;

    // Check if fragment is outside the light's view frustum [0, 1] range
    if(projCoords.x < 0.0 || projCoords.x > 1.0 ||
       projCoords.y < 0.0 || projCoords.y > 1.0 ||
       projCoords.z < 0.0 || projCoords.z > 1.0) { // Check Z too
        return 1.0; // Outside frustum = Not shadowed
    }

    // Bias calculation (similar to DeferredLightingPass)
    // u_SunProperties.sunDirectionWorld should be the normalized vector FROM the hit point TO the sun for this bias calculation.
    // If u_SunProperties.sunDirectionWorld is currently the direction light TRAVELS, we need to use its negative for this specific bias calculation to match the comment and typical CSM bias logic.
    //vec3 sunDirForBias = -normalize(u_SunProperties.sunDirectionWorld);
    vec3 lightWorldDir = -normalize(u_SunProperties.sunDirectionWorld);
    float cosTheta = clamp(dot(hitNormalWorld, lightWorldDir), 0.0, 1.0);
    float bias = max(0.005 * (1.0 - cosTheta), 0.001);

    float comparisonDepth = projCoords.z - bias;
    
    sampler2DArrayShadow shadowMapArray = sampler2DArrayShadow(u_SunProperties.sunShadowTextureArrayHandle);
    //vec2 texelSize = 1.0 / vec2(textureSize(shadowMapArray, 0));
    
    // Single shadow map lookup (no PCF)
    float shadowFactor = texture(
        shadowMapArray,
        vec4(
            projCoords.xy,
            float(u_SunProperties.sunCascadeCount - 1), // Layer index for the largest cascade
            comparisonDepth
        )
    );

    return clamp(shadowFactor, 0.0, 1.0);
}

void main() {
    // Get the 3D ID of the probe this workgroup is processing
    uvec3 probeID = uvec3(gl_WorkGroupID.xyz);

    // Bounds check for the probe ID (derived from WorkGroupID)
    if (probeID.x >= probeGridDimensions.x ||
        probeID.y >= probeGridDimensions.y ||
        probeID.z >= probeGridDimensions.z) {

        return; // This workgroup is outside the actual probe grid
    }

    // probedim/2 = center of the grid
    // offsetcenter = origin + (probedim/2) * probespacing

    vec3 probePosition = probeOrigin + (vec3(probeID) - (vec3(probeGridDimensions - 1u) / 2.0f)) * probeSpacing;
    uint localInvocationIndex1D = gl_LocalInvocationID.y * gl_WorkGroupSize.x + gl_LocalInvocationID.x;
    uint numLocalInvocationsXY = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    bool hitSky = true;

    // --- Phase 1: Cast N_RAYS_PER_PROBE sample rays and store results in shared memory ---
    for (uint sampleIdx = localInvocationIndex1D; sampleIdx < N_RAYS_PER_PROBE; sampleIdx += numLocalInvocationsXY) {
        vec3 fibonacciDir = RTXGISphericalFibonacci(sampleIdx, N_RAYS_PER_PROBE);
        // Assuming OpenGL Y-up world, and RTXGISphericalFibonacci is Z-up.
        // Swizzle: WorldX = FibX, WorldY = FibZ, WorldZ = -FibY (testing negation)
        vec3 rayDir = vec3(fibonacciDir.x, fibonacciDir.z, fibonacciDir.y);
        //vec3 rayDir = fibonacciDir;

        s_sampleRayDirections[sampleIdx] = rayDir;
        Ray sampleRay = Ray(probePosition, rayDir, 1.0 / rayDir);
        HitInfo closestHitInfo;
        closestHitInfo.hit = false;
        closestHitInfo.t = MAX_DISTANCE;

        for (int meshIdx = 0; meshIdx < u_meshCount; meshIdx++) {
            MeshInfo meshInfo = u_sceneInfo.MeshInfos[meshIdx];
            uint rootIndex = meshInfo.RootIndex;
            mat4 invTransform = meshInfo.InvTransform;
            vec3 localRayOrigin = (invTransform * vec4(sampleRay.origin, 1.0)).xyz;
            vec3 localRayDirection = (invTransform * vec4(sampleRay.direction, 0.0)).xyz;
            vec3 localInvDir = 1.0 / localRayDirection;
            Ray localRay = Ray(localRayOrigin, localRayDirection, localInvDir);
            HitInfo currentMeshHitResult = traceBVH(sampleRay, localRay, rootIndex, meshInfo);
            
            if (currentMeshHitResult.hit) {
                // Calculate anisotropic maximum travel distance t for the ray
                // such that its endpoint lies on the boundary of the probe's cell (probePosition +/- probeSpacing / 2.0)
                vec3 scaledRayDirComponents = abs(sampleRay.direction / probeSpacing); // component-wise division
                float maxScaledRate = max(scaledRayDirComponents.x, max(scaledRayDirComponents.y, scaledRayDirComponents.z));
                // Add a small epsilon to the denominator to prevent division by zero if maxScaledRate is somehow zero,
                // though it shouldn't be for a normalized rayDir and positive probeSpacing.
                float anisotropicMaxT = 0.5 / (maxScaledRate + 0.00001); 

                if (currentMeshHitResult.t < anisotropicMaxT+0.01 && currentMeshHitResult.t < closestHitInfo.t) {
                    closestHitInfo = currentMeshHitResult;
                    closestHitInfo.meshIndex = meshIdx;
                }
                hitSky = false;

            } 
        }

        if (hitSky) {
            // Calculate sun shadow for rays hitting the sky
            // Consider the "hit" to be at the probe's origin, and the normal to be the ray direction
            float sunShadowFactorSky = calculateSunShadowFactor_LargestCascade(probePosition, rayDir);

            vec3 skyColor = texture(u_skyboxCubemap, rayDir).rgb;
            // Optional: Desaturate skyColor if needed, or apply other sky effects
            // float luminance = dot(skyColor, vec3(0.2126, 0.7152, 0.0722));
            // float desaturationFactor = 0.9;
            // skyColor = mix(skyColor, vec3(luminance), desaturationFactor);

            // Apply shadow to sky color
            skyColor *= sunShadowFactorSky;
            
            s_sampleRayIrradiance[sampleIdx] = vec3(1.0, 0.0, 1.0) * sunShadowFactorSky; // to test skybox influence
            s_sampleRayDepth[sampleIdx] = SKYBOX_DISTANCE;

        } else if (closestHitInfo.hit) {
            vec2 uv = interpolateUV(closestHitInfo.barycentricUV, closestHitInfo.tri);
            vec3 worldNormal_geom = interpolateNormal(closestHitInfo.barycentricUV, closestHitInfo.tri);
            vec3 worldTangent_geom = interpolateTangent(closestHitInfo.barycentricUV, closestHitInfo.tri);
            vec3 worldShadingNormal = calculateShadingNormal(closestHitInfo.meshIndex, uv, worldNormal_geom, worldTangent_geom);
            vec3 albedo = sampleAlbedo(closestHitInfo.meshIndex, uv);


            float sunShadowFactor = calculateSunShadowFactor_LargestCascade(closestHitInfo.hitPosition, worldShadingNormal);
            // u_SunLight.direction is the direction the light travels (e.g., from sun towards scene)
            // NdotL needs vector from surface TO light, so -normalize(u_SunLight.direction)
            float NdotL_sun = max(0.0, dot(worldShadingNormal, -normalize(u_SunProperties.sunDirectionWorld)));
            vec3 directSunIrradianceComponent = (albedo / PI) * u_SunProperties.sunIntensity * NdotL_sun * sunShadowFactor; // Modulate by shadow
            
            // Indirect lighting component from previous frame's probes
            // samplePreviousProbes returns incident irradiance at the surfel from the direction of a previous probe.
            vec3 incidentIndirectIrradianceAtSurfel = samplePreviousProbes(closestHitInfo.hitPosition, worldShadingNormal);
            // The surfel reflects this incident indirect irradiance
            vec3 indirectReflectionAtSurfel = (albedo / PI) * incidentIndirectIrradianceAtSurfel;

            s_sampleRayIrradiance[sampleIdx] = directSunIrradianceComponent + indirectReflectionAtSurfel;
            s_sampleRayDepth[sampleIdx] = closestHitInfo.t;
        } else { // hit, but another probe was closer
            s_sampleRayIrradiance[sampleIdx] = vec3(0.0); // No indirect light contribution on miss
            s_sampleRayDepth[sampleIdx] = MAX_DISTANCE; // Use INFINITY_FLOAT for misses
        }

    }

    barrier(); // Synchronize workgroup: ensure all shared memory writes from Phase 1 are complete

    // --- Phase 2: Filter shared samples to compute and store output texel values ---
    uint totalOutputTexels = probeResolution.x * probeResolution.y;
    
    // Calculate the base atlas coordinate for this probe (used by all invocations for this probe)
    uint linearProbeIdx_global = probeID.z * (probeGridDimensions.x * probeGridDimensions.y) +
                                 probeID.y * probeGridDimensions.x +
                                 probeID.x;
    ivec2 probeTileBaseAtlasCoord;
    probeTileBaseAtlasCoord.x = int((linearProbeIdx_global % u_probesPerRow) * probeResolution.x);
    probeTileBaseAtlasCoord.y = int((linearProbeIdx_global / u_probesPerRow) * probeResolution.y);


    // Filtering parameters
    float cosConeLobe = 0.85; // around half the hemisphere
                               // Higher values mean narrower cones. Adjusted from 0.8 for a bit wider cone initially.
    float weightPower = 2.0;   // Power for dot product weighting, higher values give sharper falloff.

    for (uint outputTexel1DIdx = localInvocationIndex1D; outputTexel1DIdx < totalOutputTexels; outputTexel1DIdx += numLocalInvocationsXY) {
        uint local_tx = outputTexel1DIdx % probeResolution.x;
        uint local_ty = outputTexel1DIdx / probeResolution.x;

        vec2 uv_01 = (vec2(local_tx, local_ty) + 0.5) / vec2(probeResolution); // UVs in [0,1] range
        vec2 oct_coords_neg1_1 = uv_01 * 2.0 - 1.0; // Remap to [-1,1] for new octDecode
        vec3 texelPrimaryRayDir = octDecode(oct_coords_neg1_1); // New octDecode expects [-1,1]

        vec3 accumulatedIrradiance = vec3(0.0);
        float totalWeight = 0.0;
        float minDepthInCone = INFINITY_FLOAT;
        uint contributingSamplesCount = 0;

        for (uint i = 0; i < N_RAYS_PER_PROBE; i++) {
            float dot_product = dot(s_sampleRayDirections[i], texelPrimaryRayDir);
            if (dot_product > cosConeLobe) {
                float weight = pow(max(0.0, dot_product), weightPower); // max(0,...) in case dot_product is slightly negative but > cosConeLobe (if cosConeLobe is negative, which it isn't here)
               
                accumulatedIrradiance += s_sampleRayIrradiance[i] * weight;
                totalWeight += weight;

                if (s_sampleRayDepth[i] < minDepthInCone) {
                    minDepthInCone = s_sampleRayDepth[i];
                }
                // Only count samples that are actual hits for depth purposes, or just any sample in cone?
                // Let's say any sample in cone can contribute to irradiance, but depth uses actual hits.
                if(!isinf(s_sampleRayDepth[i])) { // Check if the sample ray actually hit something
                   contributingSamplesCount++; // This counts actual geometric hits within the cone
                }
            }
        }

        ivec2 atlasTexelCoord = probeTileBaseAtlasCoord + ivec2(local_tx, local_ty);
        float alpha_blend = 1.0 - u_hysteresis; // alpha_blend is the weight for NEW data

        // Hysteresis for Irradiance
        vec3 oldIrradiance = imageLoad(prevProbeAtlas, atlasTexelCoord).rgb;
        vec3 finalBlendedIrradiance;
        if (totalWeight > 0.0001) {
            vec3 currentFrameCalculatedIrradiance = accumulatedIrradiance / totalWeight;
            finalBlendedIrradiance = mix(oldIrradiance, currentFrameCalculatedIrradiance, alpha_blend);
        } else {
            // If current frame provides no significant new irradiance, keep the old value
            // to prevent valid lit areas from fading to black.
            finalBlendedIrradiance = oldIrradiance;
        }


        // Determine finalDepthForTexel based on current frame's data before creating moments
        float finalDepthForTexel;
        if (totalWeight > 0.0001 && minDepthInCone < INFINITY_FLOAT && contributingSamplesCount > 0) {
            finalDepthForTexel = minDepthInCone;
        } else {
            // Conditions for a valid depth hit not met (low weight, or all hits were sky/misses)
            finalDepthForTexel = MAX_DISTANCE;
        }

        // Hysteresis for Depth Moments
        vec2 currentDepthMoments;
        if (finalDepthForTexel < MAX_DISTANCE) {
            currentDepthMoments = vec2(finalDepthForTexel, finalDepthForTexel * finalDepthForTexel);
        } else {
            // Current frame is a miss or no info
            currentDepthMoments = vec2(MAX_DISTANCE, MAX_DISTANCE * MAX_DISTANCE); // Use MAX_DISTANCE and its square
        }

        vec2 oldDepthData = imageLoad(prevProbeDepthAtlas, atlasTexelCoord).rg;
        vec2 finalBlendedDepthMoments;

        // DDGI Paper: "If the new value is infinite (a miss), it overwrites the old depth estimate...
        // Otherwise, new finite values are blended into the existing estimate using hysteresis."
        // We use MAX_DISTANCE to represent a miss/sky.
        // Check if current frame is a miss (using MAX_DISTANCE as the indicator)
        if (currentDepthMoments.x >= MAX_DISTANCE - 0.0001) { // Use epsilon comparison for floats
            finalBlendedDepthMoments = currentDepthMoments; // Overwrite with the miss state
        } else {
            // Current frame's depth is a valid, finite hit.
            // Check if old data was a miss state
            if (oldDepthData.x >= MAX_DISTANCE - 0.0001 || isnan(oldDepthData.x)) { // If old data was bad/miss
                finalBlendedDepthMoments = currentDepthMoments; // Use new valid data directly
            } else { // Both current and old are valid, finite hits
                finalBlendedDepthMoments = mix(oldDepthData, currentDepthMoments, alpha_blend); // Blend them
            }
        }

        // Store final results to atlas
        imageStore(probeAtlas, atlasTexelCoord, vec4(finalBlendedIrradiance, 1.0));
        imageStore(probeDepthAtlas, atlasTexelCoord, vec4(finalBlendedDepthMoments, 0.0, 0.0));
    }
    // No explicit return needed, all paths complete.
}







