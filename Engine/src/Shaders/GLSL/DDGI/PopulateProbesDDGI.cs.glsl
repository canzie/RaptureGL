#version 460 core

#extension GL_EXT_buffer_reference : require
#extension GL_ARB_bindless_texture : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;


// Output Images
layout (binding = 0, r11g11b10f) uniform restrict writeonly image2D probeAtlas;
layout (binding = 1, rg16f) uniform restrict writeonly image2D probeDepthAtlas; // Not used in this step


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

struct BufferMetadata {
    uint vertexOffsetBytes; // Starting *byte* offset in the global vertex buffer
    uint texCoordOffsetBytes;
    uint indexOffsetBytes;  // Starting *byte* offset in the global index buffer
    uint triangleCount;     // Number of triangles in this mesh

    uint positionAttributeOffsetBytes; // Offset of position *within* the stride
    uint texCoordAttributeOffsetBytes;
    uint vertexStrideBytes;            // Stride of the vertex buffer in bytes
    uint indexType;                    // GL_UNSIGNED_INT (5125) or GL_UNSIGNED_SHORT (5123)

    uint64_t VBOHandle;
    uint64_t IBOHandle;
};

// Input Uniforms / Buffers
// Contains global information about the probe grid
layout(std140, binding = 0) readonly uniform ProbeInfo {
    vec3 probeGridDimensions; // Number of probes in each dimension (X, Y, Z)
    vec2 probeResolution; // Resolution of each probe texture (e.g., 8x8)
    vec3 probeSpacing;
    vec3 probeOrigin; // will probably be camera position
} probeInfo;

// Contains the actual BVH node data
layout(std430, binding = 1) readonly buffer LBVHNodes {
    LBVHNode lbvh[];
} u_lbvh;


struct MeshInfo {
    uint RootIndex;
    uint64_t AlbedoTextureHandle;
    uint64_t NormalTextureHandle;
    uint64_t MetallicRoughnessTextureHandle;
    uint bufferMetadataIDX; // index for BufferMetadata array

    mat4 Transform;
};

layout(std430, binding = 2) readonly buffer BufferMetadataStorage {
    BufferMetadata AllBufferMetadata[];
} u_bufferMetadata;

layout(std430, binding = 3) readonly buffer SceneInfo {
    MeshInfo MeshInfos[];
} u_sceneInfo;


uniform vec2 u_AtlasTextureResolution; // atlas pixel resolution in screen size, not ndc

struct Ray {
    vec3 origin;
    vec3 direction;
    vec3 invDir;
};

struct Triangle {
    vec3 v0, v1, v2;  // Vertices in world space
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

// Returns true if there's an intersection, and outputs the barycentric coordinates and distance
bool intersectTriangle(Ray ray, Triangle tri, out vec2 barycentricUV, out float t) {
    const float EPSILON = 0.000001;
    
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
    return tri.uv0 * w + tri.uv1 * u + tri.uv2 * v;
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

// Intersects ray with axis-aligned bounding box using the slab method
// Returns true if intersection occurs, false otherwise.
// tmin_out stores the distance to the *entry* point of the AABB along the ray.
bool intersectAABB(Ray ray, vec3 aabbMin, vec3 aabbMax, out float tmin_out) {
    vec3 invDir = ray.invDir;
    vec3 t0s = (aabbMin - ray.origin) * invDir;
    vec3 t1s = (aabbMax - ray.origin) * invDir;

    vec3 tsmaller = min(t0s, t1s);
    vec3 tbigger  = max(t0s, t1s);

    float tmin = max(tsmaller.x, max(tsmaller.y, tsmaller.z));
    float tmax = min(tbigger.x, min(tbigger.y, tbigger.z));

    // Ensure the intersection interval is valid and occurs in front of the ray origin
    if (tmin < tmax && tmax > 0.0) {
        tmin_out = max(0.0, tmin); // Return the entry distance (clamp to 0 if origin is inside)
        return true;
    }
    return false;
}

float RayBoundingBoxDst(Ray ray, vec3 aabbMin, vec3 aabbMax) {
    vec3 tMin = (aabbMin - ray.origin) * ray.invDir;
	vec3 tMax = (aabbMax - ray.origin) * ray.invDir;

	vec3 t1 = min(tMin, tMax);
	vec3 t2 = max(tMin, tMax);
    
	float tNear = max(max(t1.x, t1.y), t1.z);
	float tFar = min(min(t2.x, t2.y), t2.z);

	bool hit = tFar >= tNear && tFar > 0;
	float dst = hit ? tNear > 0 ? tNear : 0 : 1.#INF;
	return dst;

    
}

// Iteratively traverses the BVH for a given ray.
// Returns the index of the first leaf node encountered whose AABB is intersected by the ray,
// prioritizing traversal towards closer AABBs. Returns -1 if no leaf node AABB is hit.
LBVHNode traceBVH(Ray ray, uint index) {
    int currentNodeIndex = 0; // Start at the root
    const int MAX_ITERATIONS = 20; // Safeguard against infinite loops

    // stack
    int nodeStack[32];
    uint stackPointer = 0;
    nodeStack[stackPointer++] = index;

    LBVHNode result;


    while (stackPointer > 0) {
        int currentNodeIndex = nodeStack[--stackPointer];
        LBVHNode node = u_lbvh.lbvh[currentNodeIndex];

        float tHitCurrent;
        if (intersectAABB(ray, vec3(node.aabbMinX, node.aabbMinY, node.aabbMinZ), vec3(node.aabbMaxX, node.aabbMaxY, node.aabbMaxZ), tHitCurrent)) {
            if (node.primitiveIdx > 0) {
                result = node;
                break;

            } else {
                int childIndexA = node.left;
                int childIndexB = node.right;
                LBVHNode childA = u_lbvh.lbvh[childIndexA];
                LBVHNode childB = u_lbvh.lbvh[childIndexB];

                float dstA = RayBoundingBoxDst(ray, vec3(childA.aabbMinX, childA.aabbMinY, childA.aabbMinZ), vec3(childA.aabbMaxX, childA.aabbMaxY, childA.aabbMaxZ));
                float dstB = RayBoundingBoxDst(ray, vec3(childB.aabbMinX, childB.aabbMinY, childB.aabbMinZ), vec3(childB.aabbMaxX, childB.aabbMaxY, childB.aabbMaxZ));

                bool isNestestA = dstA < dstB;
                float dstNearest = isNestestA ? dstA : dstB;
                float dstFarthest = isNestestA ? dstB : dstA;
                int childIndexNear = isNestestA ? childIndexA : childIndexB;
                int childIndexFar = isNestestA ? childIndexB : childIndexA;

                if (dstFarthest < tHitCurrent) {
                    nodeStack[stackPointer++] = childIndexFar;
                }

                if (dstNearest > tHitCurrent) {
                    nodeStack[stackPointer++] = childIndexNear;
                }
            }
        }
    }

    return result;
}

// Placeholder: Gets triangle data (vertices, uvs) in world space
// This needs implementation using Buffer Device Address (buffer_reference)
// or alternative vertex fetching methods.
Triangle getTriangle(uint meshIndex, uint primitiveIndex) {
    Triangle tri;
    // TODO: Implement vertex data fetching.
    // 1. Get BufferMetadata: meta = u_bufferMetadata.AllBufferMetadata[u_sceneInfo.MeshInfos[meshIndex].bufferMetadataIDX];
    // 2. Get buffer handles: vboPtr = buffer_reference<float>(meta.VBOHandle); iboPtr = buffer_reference<uint_or_ushort>(meta.IBOHandle);
    // 3. Determine index size (meta.indexType == 5125 -> uint, else ushort).
    // 4. Read indices i0, i1, i2 from IBO based on primitiveIndex and meta.indexOffsetBytes.
    // 5. Read vertex positions v0_local, v1_local, v2_local from VBO based on indices, meta.vertexOffsetBytes, meta.vertexStrideBytes, meta.positionAttributeOffsetBytes.
    // 6. Read texcoords uv0, uv1, uv2 from VBO based on indices, meta.vertexOffsetBytes, meta.vertexStrideBytes, meta.texCoordAttributeOffsetBytes.
    // 7. Transform vertices to world space: tri.v0 = (u_sceneInfo.MeshInfos[meshIndex].Transform * vec4(v0_local, 1.0)).xyz; ...
    // 8. Assign texcoords: tri.uv0 = uv0; ...
    return tri;
}

// Placeholder: Samples albedo texture using bindless handle
// Requires GL_ARB_bindless_texture
vec3 sampleAlbedo(uint meshIndex, vec2 uv) {
    // TODO: Implement bindless texture sampling.
    // uint64_t handle = u_sceneInfo.MeshInfos[meshIndex].AlbedoTextureHandle;
    // if (handle == 0) return vec3(0.8); // Handle invalid texture
    // sampler2D albedoSampler = sampler2D(handle);
    // return texture(albedoSampler, uv).rgb;
    return vec3(0.8); // Return constant grey for now
}

// assumes 1 probe map is in a square format
// example using 4x4
//  1    2    3    4    : probes
// #### #### #### ####
// #### #### #### ####
// #### #### #### ####
// #### #### #### ####
//
//
vec3 texelToProbeIndex(vec2 texelCoord, vec2 probeResolution, vec3 probeGridDimensions) {
    // Calculate which probe grid cell the texel belongs to (in the 2D atlas layout)
    // Use float division and floor to get the integer index of the probe tile
    float probeGridCoordX_f = floor(texelCoord.x / probeResolution.x);
    float probeGridCoordY_f = floor(texelCoord.y / probeResolution.y);

    // Assume the atlas grid width (in number of probes) is probeGridDimensions.x
    float atlasGridWidthInProbes = probeGridDimensions.x;

    // Calculate the linear index of the probe based on its 2D position in the atlas grid
    // This assumes a row-major layout of probes in the atlas.
    float linearAtlasIndex = probeGridCoordY_f * atlasGridWidthInProbes + probeGridCoordX_f;

    // De-linearize the index back into 3D probe grid coordinates (X, Y, Z)
    // This assumes the 3D grid was linearized as: Z * (dimX * dimY) + Y * dimX + X
    float probeIndexX = mod(linearAtlasIndex, probeGridDimensions.x);

    // Integer division equivalent in GLSL for positive numbers: floor(a / b)
    float tempIndex = floor(linearAtlasIndex / probeGridDimensions.x);
    float probeIndexY = mod(tempIndex, probeGridDimensions.y);
    float probeIndexZ = floor(tempIndex / probeGridDimensions.y);

    return vec3(probeIndexX, probeIndexY, probeIndexZ);
}

vec2 normalizeTexelCoord(vec2 texelCoord, vec2 probeResolution) {
    return vec2(texelCoord.x / probeResolution.x, texelCoord.y / probeResolution.y);
}

void main() {
    // texel in the atlas
    ivec2 globalIndex2D = gl_GlobalInvocationID.xy;


    // get the probe related to the texel 
    vec3 probeIndex = texelToProbeIndex(globalIndex2D, probeInfo.probeResolution);

    //get probe position
    // NOTE: might need to quantize the probe index to the nearest integer
    vec3 probePosition = probeInfo.probeOrigin + probeIndex * probeInfo.probeSpacing;

    vec2 normAtlasTexcoord = normalizeTexelCoord(globalIndex2D, probeInfo.probeResolution);
    vec3 rayDirection = octDecode(normAtlasTexcoord);
    vec3 invDir = 1.0 / rayDirection;

    Ray ray = Ray(probePosition, rayDirection, invDir);
    
    for (int i = 0; i < u_sceneInfo.MeshInfos.length(); i++) {
        MeshInfo meshInfo = u_sceneInfo.MeshInfos[i];

        uint rootIndex = meshInfo.RootIndex;
    }

    // trace ray
    traceBVH(ray);

    // write to probe atlas
    imageStore(probeAtlas, globalIndex2D, vec4(hitInfo.hit, 0.0, 0.0, 0.0));
    
        
        
    
}







