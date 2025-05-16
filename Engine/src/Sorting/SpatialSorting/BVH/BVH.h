#pragma once

#include "BVHCommon.h"
#include <vector>
#include <glm/glm.hpp> // For glm::vec3 and glm::mat4
#include <utility> // For std::pair

// Forward declaration if BVHNode is not fully defined via BVHCommon.h for PrimitiveData, though it should be.
// struct BVHNode; 

namespace Rapture {

    // BVH class for cpu, for GPU implementations see LBVH.h (or HLBVH.h)
    class BVH {
    public:
        // Builds a Top-Level Acceleration Structure (TLAS) from a vector of BLASes (BVHCPU).
        // The generated TLAS is stored in 'outTlas'.
        // 'outOriginalTransforms' will be populated with the transforms of the input 'bvhs'.
        static void buildTLAS(
            const std::vector<BVHCPU>& bvhs,
            BVHCPU& outTlas,
            std::vector<glm::mat4>& outOriginalTransforms
        );

        // Updates an existing TLAS based on potentially changed transforms in 'currentBvhs'.
        // 'tlasToUpdate' is the TLAS to be updated.
        // 'originalAndUpdatedTransforms' is used to check for changes and is updated if the TLAS is modified.
        // Returns true if the TLAS was updated, false otherwise.
        static bool updateTLAS(
            BVHCPU& tlasToUpdate,
            const std::vector<BVHCPU>& currentBvhs,
            std::vector<glm::mat4>& originalAndUpdatedTransforms
        );

    private:
        // Internal structure used during TLAS construction
        struct TlasBuildPrimitive {
            glm::vec3 worldMinBounds;
            glm::vec3 worldMaxBounds;
            glm::vec3 centroid;
            uint32_t originalBvhCpuIndex; // Index in the initial 'bvhs' vector
        };

        // Recursive SAH-based builder function
        static uint32_t recursiveBuildSAH(
            std::vector<TlasBuildPrimitive>& buildPrimitives,
            int start,
            int end,
            std::vector<BVHNode>& tlasNodes // Nodes are added to this vector
        );

        // Helper to refit AABBs upwards from leaves after an update
        static void refitAABBsRecursive(uint32_t nodeIndex, std::vector<BVHNode>& tlasNodes);
        
        // Helper to transform a local AABB to world space
        static std::pair<glm::vec3, glm::vec3> transformAABB(
            const glm::vec3& localMin,
            const glm::vec3& localMax,
            const glm::mat4& transformMatrix
        );

        // Helper to calculate the world-space AABB of a BVHCPU (BLAS)
        static std::pair<glm::vec3, glm::vec3> calculateWorldAABBForBlas(
            const BVHCPU& blas
        );

        // Helper to calculate surface area of an AABB
        static float calculateSurfaceArea(const glm::vec3& minBounds, const glm::vec3& maxBounds);

        // SAH specific constants
        static constexpr int MAX_PRIMITIVES_PER_LEAF = 1; // Each leaf refers to one BLAS
        static constexpr int SAH_NUM_BINS = 16;          // Number of bins for binned SAH
        static constexpr float SAH_TRAVERSAL_COST = 0.5f; // Cost of traversing an internal node (adjust as needed)
        static constexpr float SAH_INTERSECTION_COST = 1.0f; // Cost of intersecting a primitive (BLAS)
    };

}

