#version 460 core

#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_NV_shader_buffer_load : require
#extension GL_NV_gpu_shader5 : require


layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
// need to define this so the store functions are enabled, since passing textures trough arguments is kind of scuffed and i cba
#define RAY_DATA_TEXTURE

layout (binding = 0, rgba32f) uniform restrict writeonly image2DArray RayData;

layout (binding = 1) uniform sampler2DArray prevProbeAtlas;
layout (binding = 2) uniform sampler2DArray prevProbeDepthAtlas;

// Skybox Cubemap
layout (binding = 3) uniform samplerCube u_skyboxCubemap;

precision highp float;



#include "MeshCommon.glsl"
#include "IrradianceCommon.glsl"

struct BVHNode {
    int left;
    int right;
    uint primitiveIdx;
    vec3 aabbMin; 
    vec3 aabbMax;

};


struct Ray {
    vec3 origin;
    vec3 direction;
    vec3 invDir;
};


// Placeholder struct for trace result
struct HitInfo {
    bool hit;
    float t;            // Hit distance
    vec2 barycentricUV; // Barycentric coords at hit point
    uint primitiveIndex; // Index within the mesh's index buffer (e.g., first index of the triangle)
    uint meshIndex;      // Index into u_sceneInfo.MeshInfos array
    TriangleVertices tri;
    vec3 hitPosition; // World space hit position
    bool isFrontFacing; // yes or backface
};



// Input Uniforms / Buffers
// Contains global information about the probe grid
layout(std140, binding = 0) uniform ProbeInfo {
    ProbeVolume u_volume;
};



// --- Sun Shadow Uniforms for Largest Cascade ---
layout(std140, binding = 1) uniform SunPropertiesUBO {
    SunProperties u_SunProperties;
};


// Contains the actual BVH node data
layout(std430, binding = 0) readonly buffer LBVHNodes {
    BVHNode lbvh[];
} u_lbvh;


layout(std430, binding = 1) readonly buffer BufferMetadataStorage {
    BufferMetadata AllBufferMetadata[];
} u_bufferMetadata;

layout(std430, binding = 2) readonly buffer SceneInfo {
    MeshInfo MeshInfos[];
} u_sceneInfo;

// Contains the actual BVH node data
layout(std430, binding = 3) readonly buffer TLASNodes {
    BVHNode lbvh[];
} u_tlas;


//uniform vec2 u_AtlasTextureResolution; // atlas pixel resolution in screen size, not ndc
uniform uint u_meshCount = 0;



#define INFINITY_FLOAT (1.0/0.0)
#define SKYBOX_DISTANCE 1000.0
#define MAX_DISTANCE 1000.0
// Define PI constant
#define PI 3.14159265359
#define PI2 6.28318530718


#define INVALID_POINTER 0x0
#define INVALID_POINTER_INT -1


// Returns true if there's an intersection, and outputs the barycentric coordinates and distance
bool intersectTriangle(Ray ray, TriangleVertices triVertices, out vec2 barycentricUV, out float t, out bool isFrontFacing) {

    // Calculate edges
    vec3 edgeAB = triVertices.v1 - triVertices.v0;
    vec3 edgeAC = triVertices.v2 - triVertices.v0;
    
    vec3 normalVec = cross(edgeAB, edgeAC);

    // is used for blending, we then invert the distance and reduce it by 80%
    isFrontFacing = dot(ray.direction, normalVec) < 0.0;
    
    
    vec3 ao = ray.origin - triVertices.v0;
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

// Calculates the final world-space shading normal using interpolated TBN and normal map
vec3 calculateShadingNormal(
    MeshInfo meshInfo,
    vec2 uv,
    vec3 N_geom,   // Interpolated geometric normal (world space)
    vec3 T_geom    // Interpolated tangent (world space)
) {
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

	vec3 t1 = min(tMin, tMax);
	vec3 t2 = max(tMin, tMax);

	float tNear = max(max(t1.x, t1.y), t1.z);
	float tFar = min(min(t2.x, t2.y), t2.z);

	bool hit = tFar >= tNear && tFar > 0;
	tmin_out = hit ? tNear > 0 ? tNear : 0 : u_volume.probeMaxRayDistance;

    return hit;

    
}


HitInfo traceBVH(Ray ray, Ray localRay, uint rootIndexOffset, MeshInfo meshInfo) {
    int currentNodeIndex = 0; 

    HitInfo result;
    result.hit = false;
    result.t = u_volume.probeMaxRayDistance;
    result.primitiveIndex = 0;
    result.barycentricUV = vec2(0.0);

    // stack
    #define MAX_STACK_SIZE 32
    int nodeStack[MAX_STACK_SIZE];
    uint stackPointer = 0;
    nodeStack[stackPointer++] = int(rootIndexOffset);


    while (stackPointer > 0) {
        currentNodeIndex = nodeStack[--stackPointer];
        BVHNode node = u_lbvh.lbvh[currentNodeIndex];

        if (node.left == INVALID_POINTER && node.right == INVALID_POINTER) {
            TriangleVertices triVerts = getTriangleVertices(meshInfo, node.primitiveIdx, u_bufferMetadata.AllBufferMetadata[meshInfo.bufferMetadataIDX]);
            vec2 barycentricUV;
            float t;
            bool isFrontFacing;
            bool hit = intersectTriangle(localRay, triVerts, barycentricUV, t, isFrontFacing);

            if (hit && t < result.t) {
                result.hit = true;
                result.t = t;
                result.primitiveIndex = node.primitiveIdx;
                result.barycentricUV = barycentricUV;
                result.tri = triVerts;
                result.isFrontFacing = isFrontFacing;
                //result.hitPosition = localRay.origin + localRay.direction * t;
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

HitInfo traceTLAS(Ray ray, uint rootIndex) {
    int currentNodeIndex = 0; 

    HitInfo result;
    result.hit = false;
    result.t = u_volume.probeMaxRayDistance;

    // stack
    #define MAX_TLAS_STACK_SIZE 16
    int nodeStack[MAX_TLAS_STACK_SIZE];
    uint stackPointer = 0;
    nodeStack[stackPointer++] = int(rootIndex);


    while (stackPointer > 0) {
        currentNodeIndex = nodeStack[--stackPointer];
        BVHNode node = u_tlas.lbvh[currentNodeIndex];

        if (node.left == -1 && node.right == -1) {
            MeshInfo meshInfo = u_sceneInfo.MeshInfos[node.primitiveIdx];

            mat4 invTransform = meshInfo.InvTransform;
            vec3 localRayOrigin = (invTransform * vec4(ray.origin, 1.0)).xyz;
            vec3 localRayDirection = (invTransform * vec4(ray.direction, 0.0)).xyz;
            vec3 localInvDir = 1.0 / localRayDirection;
            Ray localRay = Ray(localRayOrigin, localRayDirection, localInvDir);

            HitInfo BVHResult = traceBVH(ray, localRay, meshInfo.RootIndex, meshInfo);

            if (BVHResult.hit) {
                vec3 localHitPos = localRay.origin + localRay.direction * BVHResult.t;
                vec3 worldHitPos_candidate = (meshInfo.Transform * vec4(localHitPos, 1.0)).xyz;
                BVHResult.t = distance(ray.origin, worldHitPos_candidate); // Calculate world_t
                BVHResult.hitPosition = worldHitPos_candidate;
                

                if (BVHResult.t < result.t) {
                    result = BVHResult;
                    result.meshIndex = node.primitiveIdx;
                }
            }

        }  else {
            int childIndexA = int(node.left);
            int childIndexB = int(node.right);
            BVHNode childA = u_tlas.lbvh[childIndexA];
            BVHNode childB = u_tlas.lbvh[childIndexB];

            float dstA;
            float dstB;
            bool hitA = RayBoundingBoxDst(ray, childA.aabbMin, childA.aabbMax, dstA);
            bool hitB = RayBoundingBoxDst(ray, childB.aabbMin, childB.aabbMax, dstB);
            
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


vec3 sampleAlbedo(MeshInfo meshInfo, vec2 uv) {
    uint64_t albedoTextureHandle = meshInfo.AlbedoTextureHandle;
    if (albedoTextureHandle == 0) {
        return vec3(1.0, 0.0, 1.0); // Return constant grey for now
    }

    sampler2D albedoSampler = sampler2D(albedoTextureHandle);
    return texture(albedoSampler, uv).rgb;
}

vec3 sampleNormal(MeshInfo meshInfo, vec2 uv) {
    uint64_t normalTextureHandle = meshInfo.NormalTextureHandle;
    if (normalTextureHandle == 0) {
        return vec3(0.0, 0.0, 0.0); // Return constant grey for now
    }

    sampler2D normalSampler = sampler2D(normalTextureHandle);
    return texture(normalSampler, uv).rgb;
}



HitInfo TraceRay(Ray worldRay) {

    HitInfo closestHitInfo;
    closestHitInfo.hit = false;
    closestHitInfo.t = u_volume.probeMaxRayDistance;
    
    for (int meshIdx = 0; meshIdx < u_meshCount; meshIdx++) {
        MeshInfo meshInfo = u_sceneInfo.MeshInfos[meshIdx];
        uint rootIndex = meshInfo.RootIndex;
        BVHNode rootNode = u_lbvh.lbvh[rootIndex]; // Get the root node of the mesh's BVH
        
        mat4 invTransform = meshInfo.InvTransform;
        vec3 localRayOrigin = (invTransform * vec4(worldRay.origin, 1.0)).xyz;
        vec3 localRayDirection = (invTransform * vec4(worldRay.direction, 0.0)).xyz;
        vec3 localInvDir = 1.0 / localRayDirection;
        Ray localRay = Ray(localRayOrigin, localRayDirection, localInvDir);

        float rootAABBDst;
        // Test ray against the mesh's root BVH node AABB
        if (RayBoundingBoxDst(localRay, rootNode.aabbMin, rootNode.aabbMax, rootAABBDst)) {
            // Only proceed if this mesh's AABB is closer than the current closest hit
            if (rootAABBDst < closestHitInfo.t) {
                HitInfo currentMeshHitResult = traceBVH(worldRay, localRay, rootIndex, meshInfo);
            
                if (currentMeshHitResult.hit && currentMeshHitResult.t < closestHitInfo.t) {
                    closestHitInfo = currentMeshHitResult;
                    closestHitInfo.meshIndex = meshIdx;
                } 
            }
        }
    }

    return closestHitInfo;

}




/**
 * Computes a weight value in the range [0, 1] for a world position and volume pair.
 * All positions inside the given volume recieve a weight of 1.
 * Positions outside the volume receive a weight in [0, 1] that
 * decreases as the position moves away from the volume.
 */ 
float DDGIGetVolumeBlendWeight(vec3 worldPosition, ProbeVolume volume)
{
    // Get the volume's origin and extent
    vec3 extent = (volume.spacing * (volume.gridDimensions - 1)) * 0.5;

    // Get the delta between the (rotated volume) and the world-space position
    vec3 position = (worldPosition - volume.origin);
    //position = abs(RTXGIQuaternionRotate(position, RTXGIQuaternionConjugate(volume.rotation)));

    vec3 delta = position - extent;
    if(all(lessThan(delta, vec3(0.0)))) return 1.0;

    // Adjust the blend weight for each axis
    float volumeBlendWeight = 1.0;
    volumeBlendWeight *= (1.0 - clamp(delta.x / volume.spacing.x, 0.0, 1.0));
    volumeBlendWeight *= (1.0 - clamp(delta.y / volume.spacing.y, 0.0, 1.0));
    volumeBlendWeight *= (1.0 - clamp(delta.z / volume.spacing.z, 0.0, 1.0));

    return volumeBlendWeight;
}



void main() {

    ivec3 probeCoords = ivec3(gl_WorkGroupID.x, gl_WorkGroupID.z, gl_WorkGroupID.y);
    int probeIndex = DDGIGetProbeIndex(probeCoords, u_volume);

    int rayIndex = int(gl_LocalInvocationID.y * gl_WorkGroupSize.x + gl_LocalInvocationID.x);

    vec3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, u_volume);

    vec3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, u_volume);
   
    uvec3 outputCoords = DDGIGetRayDataTexelCoords(rayIndex, probeIndex, u_volume);


    Ray ray = Ray(probeWorldPosition, probeRayDirection, 1.0 / probeRayDirection);
    HitInfo closestHitInfo = traceTLAS(ray, 0);
    //HitInfo closestHitInfo = TraceRay(ray);

    if (!closestHitInfo.hit) {
        // sample the skybox

        vec3 sunColor = texture(u_skyboxCubemap, ray.direction).rgb;
        
        DDGIStoreProbeRayMiss(ivec3(outputCoords), sunColor * u_SunProperties.sunIntensity);
        return;
    }

    MeshInfo meshInfo = u_sceneInfo.MeshInfos[closestHitInfo.meshIndex];

    if (!closestHitInfo.isFrontFacing)
    {
        // Store the ray backface hit
        DDGIStoreProbeRayBackfaceHit(ivec3(outputCoords), closestHitInfo.t);
        return;
    }

    Triangle tri = getTriangleExtras(meshInfo, closestHitInfo.primitiveIndex, closestHitInfo.tri, u_bufferMetadata.AllBufferMetadata[meshInfo.bufferMetadataIDX]);

    vec2 uv = interpolateUV(closestHitInfo.barycentricUV, tri);
    vec3 worldNormal_geom = interpolateNormal(closestHitInfo.barycentricUV, tri);
    vec3 worldTangent_geom = interpolateTangent(closestHitInfo.barycentricUV, tri);
    vec3 worldShadingNormal = calculateShadingNormal(meshInfo, uv, worldNormal_geom, worldTangent_geom);
    vec3 albedo = sampleAlbedo(meshInfo, uv);
    
    vec3 diffuse = DirectDiffuseLighting(albedo, worldShadingNormal, closestHitInfo.hitPosition, u_SunProperties);

    // Indirect Lighting (recursive)
    vec3 irradiance = vec3(0.0);
    // Use the ray's own direction for surface bias, not the main camera direction
    vec3 surfaceBias = DDGIGetSurfaceBias(worldShadingNormal, ray.direction, u_volume);

    //float volumeBlendWeight = DDGIGetVolumeBlendWeight(closestHitInfo.hitPosition, u_volume);


        // Get irradiance from the DDGIVolume
        irradiance = DDGIGetVolumeIrradiance(
            closestHitInfo.hitPosition,
            worldShadingNormal,
            surfaceBias,
            prevProbeAtlas,
            prevProbeDepthAtlas,
            u_volume);


    // Perfectly diffuse reflectors don't exist in the real world.
    // Limit the BRDF albedo to a maximum value to account for the energy loss at each bounce.
    float maxAlbedo = 0.9;

    // Store the final ray radiance and hit distance
    vec3 radiance = diffuse + ((min(albedo, vec3(maxAlbedo)) / PI) * irradiance);
    DDGIStoreProbeRayFrontfaceHit(ivec3(outputCoords), clamp(radiance, vec3(0.0), vec3(1.0)), closestHitInfo.t);
}