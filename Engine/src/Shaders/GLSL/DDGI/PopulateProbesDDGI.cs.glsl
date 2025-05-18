#version 460 core

#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_NV_shader_buffer_load : require
#extension GL_NV_gpu_shader5 : require
#extension GL_ARB_shader_clock : enable


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
    vec3 aabbMin; 
    int left;
    vec3 aabbMax;
    int right;
    uint primitiveIdx;

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

// New struct for traceBVH to return its internal timings and counts
struct BVHInternalTimings {
    uint64_t total_gtverts_time_in_bvh;
    uint count_gtverts_in_bvh;
    uint64_t total_itri_time_in_bvh;
    uint count_itri_in_bvh;
    uint64_t total_rbbox_time_in_bvh;
    uint count_rbbox_in_bvh;
};

struct Profile { // Updated to reflect new and renamed metrics
    uint64_t TOTAL_INVOCATION_TIME;
    uint64_t TRACE_TLAS_TIME;
    uint64_t TOTAL_BVH_TIME_PER_TLAS_CALL; // New metric
    uint64_t BVH_TRACE_TIME; // Avg time for one full traceBVH call

    // Average time per individual call of these functions (workgroup-wide)
    uint64_t AVG_TIME_PER_GTVERTS_CALL;
    uint64_t AVG_TIME_PER_ITRI_CALL;
    uint64_t AVG_TIME_PER_RBBOX_CALL;

    // New: Average sum of time spent in these functions *during one average traceBVH call*
    uint64_t AVG_SUM_GTVERTS_TIME_IN_BVHCALL;
    uint64_t AVG_SUM_ITRI_TIME_IN_BVHCALL;
    uint64_t AVG_SUM_RBBOX_TIME_IN_BVHCALL;

    uint64_t GET_TRIANGLE_EXTRAS_TIME; 
    uint64_t DIRECT_DIFFUSE_LIGHTING_TIME;
    uint64_t GET_VOLUME_IRRADIANCE_TIME;
};


layout(std430, binding = 4) writeonly buffer Profling {
    Profile mean_timings[];
} u_profile;





#define INFINITY_FLOAT (1.0/0.0)
#define SKYBOX_DISTANCE 1000.0
#define MAX_DISTANCE 1000.0
// Define PI constant
#define PI 3.14159265359
#define PI2 6.28318530718


#define INVALID_POINTER 0x0
#define INVALID_POINTER_INT -1


// Shared variables for profiling
shared uint64_t total_BVH_TRACE_time; // For BVH_TRACE_TIME
shared uint count_BVH_TRACE;          // For BVH_TRACE_TIME & denominator for AVG_SUM_..._IN_BVHCALL
shared uint64_t shared_sum_total_bvh_time_for_all_tlas_runs; // New shared variable

// For AVG_TIME_PER_GTVERTS_CALL (these are the *original* accumulators, just renamed for clarity if needed)
shared uint64_t total_GET_TRIANGLE_VERTS_time_global; // Aggregates all individual getTriangleVertices calls
shared uint count_GET_TRIANGLE_VERTS_global;      // Total count of getTriangleVertices calls

// For AVG_TIME_PER_ITRI_CALL
shared uint64_t total_INTERSECT_TRIANGLE_time_global; // Aggregates all individual intersectTriangle calls
shared uint count_INTERSECT_TRIANGLE_global;      // Total count of intersectTriangle calls

// For AVG_TIME_PER_RBBOX_CALL
shared uint64_t total_INTERSECT_BOUNDING_BOX_time_global; // Aggregates all individual RayBoundingBoxDst calls
shared uint count_INTERSECT_BOUNDING_BOX_global;      // Total count of RayBoundingBoxDst calls

// New shared accumulators for the sums of times *within* BVH calls, aggregated across the workgroup
shared uint64_t shared_sum_total_gtverts_time_across_all_bvh_runs;
shared uint64_t shared_sum_total_itri_time_across_all_bvh_runs;
shared uint64_t shared_sum_total_rbbox_time_across_all_bvh_runs;

// Existing timers
shared uint64_t total_INVOCATION_TIME_accumulator;
shared uint count_INVOCATIONS;
shared uint64_t total_GET_TRIANGLE_EXTRAS_time;
shared uint count_GET_TRIANGLE_EXTRAS;
shared uint64_t total_DIRECT_DIFFUSE_LIGHTING_time;
shared uint count_DIRECT_DIFFUSE_LIGHTING;
shared uint64_t total_GET_VOLUME_IRRADIANCE_time;
shared uint count_GET_VOLUME_IRRADIANCE;
shared uint64_t total_TRACE_TLAS_time;
shared uint count_TRACE_TLAS;


// Returns true if there's an intersection, and outputs the barycentric coordinates and distance
bool intersectTriangle(Ray ray, TriangleVertices triVertices, out vec2 barycentricUV, out float t, out bool isFrontFacing) {
    // uint64_t start_time = clockARB(); // Timing to be done at call site

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
    
    // uint64_t end_time = clockARB(); // Timing to be done at call site
    // REMOVED: atomicAdd(total_INTERSECT_TRIANGLE_time_global, end_time - start_time);
    // REMOVED: atomicAdd(count_INTERSECT_TRIANGLE_global, 1);
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
    // uint64_t start_time = clockARB(); // Timing to be done at call site

    vec3 tMin = (aabbMin - ray.origin) * ray.invDir;
	vec3 tMax = (aabbMax - ray.origin) * ray.invDir;

	vec3 t1 = min(tMin, tMax);
	vec3 t2 = max(tMin, tMax);

	float tNear = max(max(t1.x, t1.y), t1.z);
	float tFar = min(min(t2.x, t2.y), t2.z);

	bool hit = tFar >= tNear && tFar > 0;
	tmin_out = hit ? tNear > 0 ? tNear : 0 : u_volume.probeMaxRayDistance;

    // uint64_t end_time = clockARB(); // Timing to be done at call site
    // REMOVED: atomicAdd(total_INTERSECT_BOUNDING_BOX_time_global, end_time - start_time);
    // REMOVED: atomicAdd(count_INTERSECT_BOUNDING_BOX_global, 1);
    return hit;

    
}




HitInfo traceBVH(Ray localRay, float closestHit, uint rootIndexOffset, MeshInfo meshInfo, out BVHInternalTimings internal_timings) {
    // Initialize the output struct for this run of traceBVH
    internal_timings.total_gtverts_time_in_bvh = 0;
    internal_timings.count_gtverts_in_bvh = 0;
    internal_timings.total_itri_time_in_bvh = 0;
    internal_timings.count_itri_in_bvh = 0;
    internal_timings.total_rbbox_time_in_bvh = 0;
    internal_timings.count_rbbox_in_bvh = 0;

    int currentNodeIndex = 0; 

    HitInfo result;
    result.hit = false;
    result.t = closestHit;
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
            uint64_t start_get_tri_v_time = clockARB();
            TriangleVertices triVerts = getTriangleVertices(meshInfo, node.primitiveIdx, u_bufferMetadata.AllBufferMetadata[meshInfo.bufferMetadataIDX]);
            uint64_t end_get_tri_v_time = clockARB();
            uint64_t duration_gtverts = end_get_tri_v_time - start_get_tri_v_time;
            internal_timings.total_gtverts_time_in_bvh += duration_gtverts;
            internal_timings.count_gtverts_in_bvh++;
            atomicAdd(total_GET_TRIANGLE_VERTS_time_global, duration_gtverts);
            atomicAdd(count_GET_TRIANGLE_VERTS_global, 1);
            
            vec2 barycentricUV;
            float t;
            bool isFrontFacing;
            uint64_t start_itri_time = clockARB();
            bool hit = intersectTriangle(localRay, triVerts, barycentricUV, t, isFrontFacing);
            uint64_t end_itri_time = clockARB();
            uint64_t duration_itri = end_itri_time - start_itri_time;
            internal_timings.total_itri_time_in_bvh += duration_itri;
            internal_timings.count_itri_in_bvh++;
            atomicAdd(total_INTERSECT_TRIANGLE_time_global, duration_itri);
            atomicAdd(count_INTERSECT_TRIANGLE_global, 1);

            if (hit && t < result.t) {
                result.hit = true;
                result.t = t;
                result.primitiveIndex = node.primitiveIdx;
                result.barycentricUV = barycentricUV;
                result.tri = triVerts;
                result.isFrontFacing = isFrontFacing;
            }
        }  else {
            int childIndexA = int(node.left+rootIndexOffset);
            int childIndexB = int(node.right+rootIndexOffset);
            BVHNode childA = u_lbvh.lbvh[childIndexA];
            BVHNode childB = u_lbvh.lbvh[childIndexB];

            float dstA;
            float dstB;
            uint64_t start_rbbox_A_time = clockARB();
            bool hitA = RayBoundingBoxDst(localRay, childA.aabbMin, childA.aabbMax, dstA);
            uint64_t end_rbbox_A_time = clockARB();
            uint64_t duration_rbbox_A = end_rbbox_A_time - start_rbbox_A_time;
            internal_timings.total_rbbox_time_in_bvh += duration_rbbox_A;
            internal_timings.count_rbbox_in_bvh++;
            atomicAdd(total_INTERSECT_BOUNDING_BOX_time_global, duration_rbbox_A);
            atomicAdd(count_INTERSECT_BOUNDING_BOX_global, 1);

            uint64_t start_rbbox_B_time = clockARB();
            bool hitB = RayBoundingBoxDst(localRay, childB.aabbMin, childB.aabbMax, dstB);
            uint64_t end_rbbox_B_time = clockARB();
            uint64_t duration_rbbox_B = end_rbbox_B_time - start_rbbox_B_time;
            internal_timings.total_rbbox_time_in_bvh += duration_rbbox_B;
            internal_timings.count_rbbox_in_bvh++;
            atomicAdd(total_INTERSECT_BOUNDING_BOX_time_global, duration_rbbox_B);
            atomicAdd(count_INTERSECT_BOUNDING_BOX_global, 1);
            
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




// PEFORMANCE NOTE: removing the tracebvh makes fps go from 60->130
HitInfo traceTLAS(Ray ray, uint rootIndex, out BVHInternalTimings accumulated_bvh_internal_timings_for_ray, out uint64_t total_bvh_duration_within_this_tlas) {
    total_bvh_duration_within_this_tlas = 0; // Initialize new out parameter
    // Initialize the output struct for accumulated timings for this ray
    accumulated_bvh_internal_timings_for_ray.total_gtverts_time_in_bvh = 0;
    accumulated_bvh_internal_timings_for_ray.count_gtverts_in_bvh = 0; // Counts are not summed here, only times
    accumulated_bvh_internal_timings_for_ray.total_itri_time_in_bvh = 0;
    accumulated_bvh_internal_timings_for_ray.count_itri_in_bvh = 0;     // Counts are not summed here
    accumulated_bvh_internal_timings_for_ray.total_rbbox_time_in_bvh = 0;
    accumulated_bvh_internal_timings_for_ray.count_rbbox_in_bvh = 0;    // Counts are not summed here

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

            uint64_t start_bvh_time = clockARB();
            BVHInternalTimings current_bvh_run_timings; // Changed from 'internal_timings' to avoid conflict
            HitInfo BVHResult = traceBVH(localRay, result.t, meshInfo.RootIndex, meshInfo, current_bvh_run_timings);
            uint64_t end_bvh_time = clockARB();
            uint64_t bvh_duration = end_bvh_time - start_bvh_time;
            atomicAdd(total_BVH_TRACE_time, bvh_duration);
            atomicAdd(count_BVH_TRACE, 1);

            total_bvh_duration_within_this_tlas += bvh_duration; // Accumulate BVH duration for this TLAS call

            // Accumulate the sums of times from this BVH run into the ray-specific accumulator
            accumulated_bvh_internal_timings_for_ray.total_gtverts_time_in_bvh += current_bvh_run_timings.total_gtverts_time_in_bvh;
            accumulated_bvh_internal_timings_for_ray.total_itri_time_in_bvh += current_bvh_run_timings.total_itri_time_in_bvh;
            accumulated_bvh_internal_timings_for_ray.total_rbbox_time_in_bvh += current_bvh_run_timings.total_rbbox_time_in_bvh;
            // The counts within accumulated_bvh_internal_timings_for_ray are not used / summed here as this struct passes sums of times.

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
    uint64_t invocation_start_time = clockARB(); // Start timing for the entire invocation

    // Initialize shared profiling variables
    if (gl_LocalInvocationIndex == 0) {
        total_BVH_TRACE_time = 0;
        count_BVH_TRACE = 0;

        total_GET_TRIANGLE_VERTS_time_global = 0;
        count_GET_TRIANGLE_VERTS_global = 0;
        total_INTERSECT_TRIANGLE_time_global = 0;
        count_INTERSECT_TRIANGLE_global = 0;
        total_INTERSECT_BOUNDING_BOX_time_global = 0;
        count_INTERSECT_BOUNDING_BOX_global = 0;

        shared_sum_total_gtverts_time_across_all_bvh_runs = 0;
        shared_sum_total_itri_time_across_all_bvh_runs = 0;
        shared_sum_total_rbbox_time_across_all_bvh_runs = 0;
        shared_sum_total_bvh_time_for_all_tlas_runs = 0; // Initialize new shared variable

        total_INVOCATION_TIME_accumulator = 0;
        count_INVOCATIONS = 0;
        total_GET_TRIANGLE_EXTRAS_time = 0;
        count_GET_TRIANGLE_EXTRAS = 0;
        total_DIRECT_DIFFUSE_LIGHTING_time = 0;
        count_DIRECT_DIFFUSE_LIGHTING = 0;
        total_GET_VOLUME_IRRADIANCE_time = 0;
        count_GET_VOLUME_IRRADIANCE = 0;
        total_TRACE_TLAS_time = 0;
        count_TRACE_TLAS = 0;
    }
    barrier(); // Synchronize after initialization

    ivec3 probeCoords = ivec3(gl_WorkGroupID.x, gl_WorkGroupID.z, gl_WorkGroupID.y);
    int probeIndex = DDGIGetProbeIndex(probeCoords, u_volume);

    int rayIndex = int(gl_LocalInvocationID.y * gl_WorkGroupSize.x + gl_LocalInvocationID.x);

    vec3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, u_volume);

    vec3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, u_volume);
   
    uvec3 outputCoords = DDGIGetRayDataTexelCoords(rayIndex, probeIndex, u_volume);

    Ray ray = Ray(probeWorldPosition, probeRayDirection, 1.0 / probeRayDirection);
    
    BVHInternalTimings accumulated_bvh_internal_timings_for_this_ray; // Renamed for clarity
    uint64_t total_bvh_time_for_this_tlas_call; // Variable for the new out parameter

    uint64_t trace_tlas_start_time = clockARB();
    HitInfo closestHitInfo = traceTLAS(ray, 0, accumulated_bvh_internal_timings_for_this_ray, total_bvh_time_for_this_tlas_call); // Pass new out param
    uint64_t trace_tlas_end_time = clockARB();
    atomicAdd(total_TRACE_TLAS_time, trace_tlas_end_time - trace_tlas_start_time);
    atomicAdd(count_TRACE_TLAS, 1);

    atomicAdd(shared_sum_total_bvh_time_for_all_tlas_runs, total_bvh_time_for_this_tlas_call); // Atomic add to shared accumulator

    // Add the sums of times from all BVH runs for this ray to the workgroup-shared accumulators
    atomicAdd(shared_sum_total_gtverts_time_across_all_bvh_runs, accumulated_bvh_internal_timings_for_this_ray.total_gtverts_time_in_bvh);
    atomicAdd(shared_sum_total_itri_time_across_all_bvh_runs, accumulated_bvh_internal_timings_for_this_ray.total_itri_time_in_bvh);
    atomicAdd(shared_sum_total_rbbox_time_across_all_bvh_runs, accumulated_bvh_internal_timings_for_this_ray.total_rbbox_time_in_bvh);

    if (!closestHitInfo.hit) {
        // sample the skybox

        vec3 sunColor = texture(u_skyboxCubemap, ray.direction).rgb;
        sunColor *= u_SunProperties.sunIntensity * u_SunProperties.sunColor;
        
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


    uint64_t get_tri_extras_start_time = clockARB();
    Triangle tri = getTriangleExtras(meshInfo, closestHitInfo.primitiveIndex, closestHitInfo.tri, u_bufferMetadata.AllBufferMetadata[meshInfo.bufferMetadataIDX]);
    uint64_t get_tri_extras_end_time = clockARB();
    atomicAdd(total_GET_TRIANGLE_EXTRAS_time, get_tri_extras_end_time - get_tri_extras_start_time);
    atomicAdd(count_GET_TRIANGLE_EXTRAS, 1);

    vec2 uv = interpolateUV(closestHitInfo.barycentricUV, tri);
    vec3 worldNormal_geom = interpolateNormal(closestHitInfo.barycentricUV, tri);
    vec3 worldTangent_geom = interpolateTangent(closestHitInfo.barycentricUV, tri);
    vec3 worldShadingNormal = calculateShadingNormal(meshInfo, uv, worldNormal_geom, worldTangent_geom);
    vec3 albedo = sampleAlbedo(meshInfo, uv);
    
    uint64_t direct_diffuse_start_time = clockARB();
    vec3 diffuse = DirectDiffuseLighting(albedo, worldShadingNormal, closestHitInfo.hitPosition, u_SunProperties);
    uint64_t direct_diffuse_end_time = clockARB();
    atomicAdd(total_DIRECT_DIFFUSE_LIGHTING_time, direct_diffuse_end_time - direct_diffuse_start_time);
    atomicAdd(count_DIRECT_DIFFUSE_LIGHTING, 1);

    // Indirect Lighting (recursive)
    vec3 irradiance = vec3(0.0);
    // Use the ray's own direction for surface bias, not the main camera direction
    vec3 surfaceBias = DDGIGetSurfaceBias(worldShadingNormal, ray.direction, u_volume);



        uint64_t get_volume_irradiance_start_time = clockARB();
        // Get irradiance from the DDGIVolume
        irradiance = DDGIGetVolumeIrradiance(
            closestHitInfo.hitPosition,
            worldShadingNormal,
            surfaceBias,
            prevProbeAtlas,
            prevProbeDepthAtlas,
            u_volume);
        uint64_t get_volume_irradiance_end_time = clockARB();
        atomicAdd(total_GET_VOLUME_IRRADIANCE_time, get_volume_irradiance_end_time - get_volume_irradiance_start_time);
        atomicAdd(count_GET_VOLUME_IRRADIANCE, 1);


    // Perfectly diffuse reflectors don't exist in the real world.
    // Limit the BRDF albedo to a maximum value to account for the energy loss at each bounce.
    float maxAlbedo = 0.9;

    // Store the final ray radiance and hit distance
    vec3 radiance = diffuse + ((min(albedo, vec3(maxAlbedo)) / PI) * irradiance);
    DDGIStoreProbeRayFrontfaceHit(ivec3(outputCoords), clamp(radiance, vec3(0.0), vec3(1.0)), closestHitInfo.t);

    uint64_t invocation_end_time = clockARB(); // End timing for the entire invocation
    atomicAdd(total_INVOCATION_TIME_accumulator, invocation_end_time - invocation_start_time);
    atomicAdd(count_INVOCATIONS, 1);

    // Synchronize before calculating and writing averages
    barrier();

    if (gl_LocalInvocationIndex == 0) {
        // probeIndex is unique per workgroup and suitable for indexing mean_timings

        if (count_INVOCATIONS > 0) {
            u_profile.mean_timings[probeIndex].TOTAL_INVOCATION_TIME = total_INVOCATION_TIME_accumulator / count_INVOCATIONS;
        } else {
            u_profile.mean_timings[probeIndex].TOTAL_INVOCATION_TIME = 0;
        }

        if (count_TRACE_TLAS > 0) {
            u_profile.mean_timings[probeIndex].TRACE_TLAS_TIME = total_TRACE_TLAS_time / count_TRACE_TLAS;
            u_profile.mean_timings[probeIndex].TOTAL_BVH_TIME_PER_TLAS_CALL = shared_sum_total_bvh_time_for_all_tlas_runs / count_TRACE_TLAS;
        } else {
            u_profile.mean_timings[probeIndex].TRACE_TLAS_TIME = 0;
            u_profile.mean_timings[probeIndex].TOTAL_BVH_TIME_PER_TLAS_CALL = 0;
        }

        if (count_BVH_TRACE > 0) {
            u_profile.mean_timings[probeIndex].BVH_TRACE_TIME = total_BVH_TRACE_time / count_BVH_TRACE;
        } else {
            u_profile.mean_timings[probeIndex].BVH_TRACE_TIME = 0;
        }

        // AVG_TIME_PER_..._CALL metrics
        if (count_GET_TRIANGLE_VERTS_global > 0) { // Using _global count
            u_profile.mean_timings[probeIndex].AVG_TIME_PER_GTVERTS_CALL = total_GET_TRIANGLE_VERTS_time_global / count_GET_TRIANGLE_VERTS_global;
        } else {
            u_profile.mean_timings[probeIndex].AVG_TIME_PER_GTVERTS_CALL = 0;
        }
        if (count_INTERSECT_TRIANGLE_global > 0) { // Using _global count
            u_profile.mean_timings[probeIndex].AVG_TIME_PER_ITRI_CALL = total_INTERSECT_TRIANGLE_time_global / count_INTERSECT_TRIANGLE_global;
        } else {
            u_profile.mean_timings[probeIndex].AVG_TIME_PER_ITRI_CALL = 0;
        }
        if (count_INTERSECT_BOUNDING_BOX_global > 0) { // Using _global count
            u_profile.mean_timings[probeIndex].AVG_TIME_PER_RBBOX_CALL = total_INTERSECT_BOUNDING_BOX_time_global / count_INTERSECT_BOUNDING_BOX_global;
        } else {
            u_profile.mean_timings[probeIndex].AVG_TIME_PER_RBBOX_CALL = 0;
        }

        // AVG_SUM_TIME_..._IN_BVHCALL metrics
        // Denominator is count_BVH_TRACE (total BVH calls in workgroup)
        if (count_BVH_TRACE > 0) {
            u_profile.mean_timings[probeIndex].AVG_SUM_GTVERTS_TIME_IN_BVHCALL = shared_sum_total_gtverts_time_across_all_bvh_runs / count_BVH_TRACE;
            u_profile.mean_timings[probeIndex].AVG_SUM_ITRI_TIME_IN_BVHCALL = shared_sum_total_itri_time_across_all_bvh_runs / count_BVH_TRACE;
            u_profile.mean_timings[probeIndex].AVG_SUM_RBBOX_TIME_IN_BVHCALL = shared_sum_total_rbbox_time_across_all_bvh_runs / count_BVH_TRACE;
        } else {
            u_profile.mean_timings[probeIndex].AVG_SUM_GTVERTS_TIME_IN_BVHCALL = 0;
            u_profile.mean_timings[probeIndex].AVG_SUM_ITRI_TIME_IN_BVHCALL = 0;
            u_profile.mean_timings[probeIndex].AVG_SUM_RBBOX_TIME_IN_BVHCALL = 0;
        }

        // Existing unrelated timers
        if (count_GET_TRIANGLE_EXTRAS > 0) {
            u_profile.mean_timings[probeIndex].GET_TRIANGLE_EXTRAS_TIME = total_GET_TRIANGLE_EXTRAS_time / count_GET_TRIANGLE_EXTRAS;
        } else {
            u_profile.mean_timings[probeIndex].GET_TRIANGLE_EXTRAS_TIME = 0;
        }

        if (count_GET_VOLUME_IRRADIANCE > 0) {
            u_profile.mean_timings[probeIndex].GET_VOLUME_IRRADIANCE_TIME = total_GET_VOLUME_IRRADIANCE_time / count_GET_VOLUME_IRRADIANCE;
        } else {
            u_profile.mean_timings[probeIndex].GET_VOLUME_IRRADIANCE_TIME = 0;
        }
    }
}