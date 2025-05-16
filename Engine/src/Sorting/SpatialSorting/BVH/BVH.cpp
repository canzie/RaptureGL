#include "BVH.h"
#include "Logger/Log.h" // Assuming this path is correct for your logging library
#include <algorithm>
#include <limits>
#include <numeric> // For std::iota if needed, or other utilities
#include <vector>

// Define a sentinel for invalid node indices, consistent with BVHNode::leftChildIndex/rightChildIndex type
constexpr int BVH_INVALID_NODE_INDEX = -1; 

namespace Rapture {

// SAH specific constants (already in .h but good for reference here)
// constexpr int BVH::MAX_PRIMITIVES_PER_LEAF; // = 1 for TLAS
// constexpr int BVH::SAH_NUM_BINS;            // = 16
// constexpr float BVH::SAH_TRAVERSAL_COST;    // = 0.5f
// constexpr float BVH::SAH_INTERSECTION_COST; // = 1.0f


// Helper to transform a local AABB to world space
std::pair<glm::vec3, glm::vec3> BVH::transformAABB(
    const glm::vec3& localMin,
    const glm::vec3& localMax,
    const glm::mat4& transformMatrix) {
    
    glm::vec3 corners[8] = {
        glm::vec3(localMin.x, localMin.y, localMin.z),
        glm::vec3(localMax.x, localMin.y, localMin.z),
        glm::vec3(localMin.x, localMax.y, localMin.z),
        glm::vec3(localMin.x, localMin.y, localMax.z),
        glm::vec3(localMax.x, localMax.y, localMin.z),
        glm::vec3(localMin.x, localMax.y, localMax.z),
        glm::vec3(localMax.x, localMin.y, localMax.z),
        glm::vec3(localMax.x, localMax.y, localMax.z)
    };

    glm::vec3 worldMin(std::numeric_limits<float>::max());
    glm::vec3 worldMax(std::numeric_limits<float>::lowest());

    for (int i = 0; i < 8; ++i) {
        glm::vec4 transformedCorner = transformMatrix * glm::vec4(corners[i], 1.0f);
        worldMin = glm::min(worldMin, glm::vec3(transformedCorner));
        worldMax = glm::max(worldMax, glm::vec3(transformedCorner));
    }
    return {worldMin, worldMax};
}

// Helper to calculate the world-space AABB of a BVHCPU (BLAS)
std::pair<glm::vec3, glm::vec3> BVH::calculateWorldAABBForBlas(const BVHCPU& blas) {
    if (blas.nodes.empty() || blas.rootIndex >= blas.nodes.size()) {
        GE_CORE_WARN("Attempting to calculate AABB for an empty or invalid BLAS. Returning zero AABB.");
        return {glm::vec3(0.0f), glm::vec3(0.0f)};
    }
    const BVHNode& blasRootNode = blas.nodes[blas.rootIndex];
    return transformAABB(blasRootNode.minBounds, blasRootNode.maxBounds, blas.transform);
}

// Helper to calculate surface area of an AABB
float BVH::calculateSurfaceArea(const glm::vec3& minBounds, const glm::vec3& maxBounds) {
    glm::vec3 extent = maxBounds - minBounds;
    if (extent.x < 0.0f || extent.y < 0.0f || extent.z < 0.0f) return 0.0f; // Invalid AABB
    return 2.0f * (extent.x * extent.y + extent.x * extent.z + extent.y * extent.z);
}


uint32_t BVH::recursiveBuildSAH(
    std::vector<TlasBuildPrimitive>& buildPrimitives,
    int start,
    int end, // Exclusive end index
    std::vector<BVHNode>& tlasNodes) {

    int numPrimitives = end - start;
    if (numPrimitives <= 0) {
        GE_CORE_ERROR("recursiveBuildSAH called with zero or negative primitives.");
        BVHNode dummyNode;
        dummyNode.leftChildIndex = BVH_INVALID_NODE_INDEX;
        dummyNode.rightChildIndex = BVH_INVALID_NODE_INDEX;
        dummyNode.primitiveIdx = static_cast<uint32_t>(-1); // Indicate error/dummy
        dummyNode.minBounds = glm::vec3(std::numeric_limits<float>::max());
        dummyNode.maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
        tlasNodes.push_back(dummyNode);
        return static_cast<uint32_t>(tlasNodes.size() - 1);
    }
    
    uint32_t currentNodeIndex = static_cast<uint32_t>(tlasNodes.size());
    tlasNodes.emplace_back(); // Allocate space for the current node

    // Calculate bounds for all primitives in the current range
    glm::vec3 overallMin = buildPrimitives[start].worldMinBounds;
    glm::vec3 overallMax = buildPrimitives[start].worldMaxBounds;
    for (int i = start + 1; i < end; ++i) {
        overallMin = glm::min(overallMin, buildPrimitives[i].worldMinBounds);
        overallMax = glm::max(overallMax, buildPrimitives[i].worldMaxBounds);
    }
    
    tlasNodes[currentNodeIndex].minBounds = overallMin;
    tlasNodes[currentNodeIndex].maxBounds = overallMax;
    tlasNodes[currentNodeIndex].primitiveIdx = 0; // Internal nodes don't point to a specific BLAS index this way

    if (numPrimitives <= MAX_PRIMITIVES_PER_LEAF) { // Create a leaf node
        tlasNodes[currentNodeIndex].leftChildIndex = BVH_INVALID_NODE_INDEX;
        tlasNodes[currentNodeIndex].rightChildIndex = BVH_INVALID_NODE_INDEX;
        // For TLAS, leaf stores index of the single BLAS it represents
        tlasNodes[currentNodeIndex].primitiveIdx = buildPrimitives[start].originalBvhCpuIndex; 
        return currentNodeIndex;
    }

    // --- Binned SAH for splitting ---
    glm::vec3 centroidBoundsMin(std::numeric_limits<float>::max());
    glm::vec3 centroidBoundsMax(std::numeric_limits<float>::lowest());
    for (int i = start; i < end; ++i) {
        centroidBoundsMin = glm::min(centroidBoundsMin, buildPrimitives[i].centroid);
        centroidBoundsMax = glm::max(centroidBoundsMax, buildPrimitives[i].centroid);
    }

    int bestSplitAxis = -1;
    float bestSplitCost = std::numeric_limits<float>::max();
    int bestSplitPrimitiveMid = start + numPrimitives / 2; // Fallback partition
    float bestSplitCoordinate = 0.0f; // The coordinate value for the best split plane

    float parentSurfaceArea = calculateSurfaceArea(overallMin, overallMax);
    float costNoSplit = SAH_INTERSECTION_COST * numPrimitives; // Cost of making this a leaf (if > MAX_PRIMITIVES_PER_LEAF)

    for (int axis = 0; axis < 3; ++axis) {
        float axisExtent = centroidBoundsMax[axis] - centroidBoundsMin[axis];
        if (axisExtent < 1e-6f) continue; 

        struct Bin {
            glm::vec3 boundsMin = glm::vec3(std::numeric_limits<float>::max());
            glm::vec3 boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
            int primitiveCount = 0;
        };
        std::vector<Bin> bins(SAH_NUM_BINS);

        for (int i = start; i < end; ++i) {
            float c = buildPrimitives[i].centroid[axis];
            int binIdx = static_cast<int>(SAH_NUM_BINS * ((c - centroidBoundsMin[axis]) / axisExtent));
            binIdx = std::min(SAH_NUM_BINS - 1, std::max(0, binIdx));
            
            bins[binIdx].primitiveCount++;
            bins[binIdx].boundsMin = glm::min(bins[binIdx].boundsMin, buildPrimitives[i].worldMinBounds);
            bins[binIdx].boundsMax = glm::max(bins[binIdx].boundsMax, buildPrimitives[i].worldMaxBounds);
        }

        for (int splitPlaneBinIdx = 0; splitPlaneBinIdx < SAH_NUM_BINS - 1; ++splitPlaneBinIdx) {
            glm::vec3 leftAABBMin(std::numeric_limits<float>::max());
            glm::vec3 leftAABBMax(std::numeric_limits<float>::lowest());
            int leftCount = 0;
            for (int j = 0; j <= splitPlaneBinIdx; ++j) {
                if (bins[j].primitiveCount > 0) {
                    leftAABBMin = glm::min(leftAABBMin, bins[j].boundsMin);
                    leftAABBMax = glm::max(leftAABBMax, bins[j].boundsMax);
                    leftCount += bins[j].primitiveCount;
                }
            }

            glm::vec3 rightAABBMin(std::numeric_limits<float>::max());
            glm::vec3 rightAABBMax(std::numeric_limits<float>::lowest());
            int rightCount = 0;
            for (int j = splitPlaneBinIdx + 1; j < SAH_NUM_BINS; ++j) {
                 if (bins[j].primitiveCount > 0) {
                    rightAABBMin = glm::min(rightAABBMin, bins[j].boundsMin);
                    rightAABBMax = glm::max(rightAABBMax, bins[j].boundsMax);
                    rightCount += bins[j].primitiveCount;
                }
            }
            
            if (leftCount == 0 || rightCount == 0) continue;

            float leftArea = calculateSurfaceArea(leftAABBMin, leftAABBMax);
            float rightArea = calculateSurfaceArea(rightAABBMin, rightAABBMax);
            
            float cost = SAH_TRAVERSAL_COST;
            if (parentSurfaceArea > 1e-6f) { 
                 cost += (SAH_INTERSECTION_COST * (leftCount * leftArea + rightCount * rightArea)) / parentSurfaceArea;
            } else { 
                 cost += SAH_INTERSECTION_COST * (leftCount + rightCount); 
            }

            if (cost < bestSplitCost) {
                bestSplitCost = cost;
                bestSplitAxis = axis;
                bestSplitCoordinate = centroidBoundsMin[axis] + (splitPlaneBinIdx + 1) * axisExtent / SAH_NUM_BINS;
            }
        }
    }
    
    if (bestSplitAxis != -1 && bestSplitCost < costNoSplit) { 
        auto it = std::partition(buildPrimitives.begin() + start, buildPrimitives.begin() + end,
            [&](const TlasBuildPrimitive& p) {
            return p.centroid[bestSplitAxis] < bestSplitCoordinate;
        });
        bestSplitPrimitiveMid = static_cast<int>(std::distance(buildPrimitives.begin(), it));

        if (bestSplitPrimitiveMid == start || bestSplitPrimitiveMid == end) { 
            bestSplitAxis = (bestSplitAxis != -1) ? bestSplitAxis : 0; // Ensure bestSplitAxis is valid
            if (centroidBoundsMax[bestSplitAxis] - centroidBoundsMin[bestSplitAxis] < 1e-6f) { // If chosen axis is degenerate, pick largest extent
                bestSplitAxis = 0;
                if (centroidBoundsMax.y - centroidBoundsMin.y > centroidBoundsMax.x - centroidBoundsMin.x) bestSplitAxis = 1;
                if (centroidBoundsMax.z - centroidBoundsMin.z > glm::max(centroidBoundsMax.x - centroidBoundsMin.x, centroidBoundsMax.y - centroidBoundsMin.y)) bestSplitAxis = 2;
            }

            bestSplitPrimitiveMid = start + numPrimitives / 2;
            std::nth_element(buildPrimitives.begin() + start, 
                             buildPrimitives.begin() + bestSplitPrimitiveMid, 
                             buildPrimitives.begin() + end,
                             [&](const TlasBuildPrimitive& a, const TlasBuildPrimitive& b) {
                                 return a.centroid[bestSplitAxis] < b.centroid[bestSplitAxis];
                             });
        }
    } else { 
        bestSplitAxis = 0;
        if (centroidBoundsMax.y - centroidBoundsMin.y > centroidBoundsMax.x - centroidBoundsMin.x) bestSplitAxis = 1;
        if (centroidBoundsMax.z - centroidBoundsMin.z > glm::max(centroidBoundsMax.x - centroidBoundsMin.x, centroidBoundsMax.y - centroidBoundsMin.y)) bestSplitAxis = 2;
        
        bestSplitPrimitiveMid = start + numPrimitives / 2;
        std::nth_element(buildPrimitives.begin() + start, 
                         buildPrimitives.begin() + bestSplitPrimitiveMid, 
                         buildPrimitives.begin() + end,
                         [&](const TlasBuildPrimitive& a, const TlasBuildPrimitive& b) {
                             return a.centroid[bestSplitAxis] < b.centroid[bestSplitAxis];
                         });
    }

    tlasNodes[currentNodeIndex].leftChildIndex = recursiveBuildSAH(buildPrimitives, start, bestSplitPrimitiveMid, tlasNodes);
    tlasNodes[currentNodeIndex].rightChildIndex = recursiveBuildSAH(buildPrimitives, bestSplitPrimitiveMid, end, tlasNodes);
    
    // Recalculate parent AABB from children to ensure tightness after potential splits/partitions
    const BVHNode& leftChild = tlasNodes[static_cast<uint32_t>(tlasNodes[currentNodeIndex].leftChildIndex)];
    const BVHNode& rightChild = tlasNodes[static_cast<uint32_t>(tlasNodes[currentNodeIndex].rightChildIndex)];
    tlasNodes[currentNodeIndex].minBounds = glm::min(leftChild.minBounds, rightChild.minBounds);
    tlasNodes[currentNodeIndex].maxBounds = glm::max(leftChild.maxBounds, rightChild.maxBounds);

    return currentNodeIndex;
}


void BVH::buildTLAS(
    const std::vector<BVHCPU>& bvhs,
    BVHCPU& outTlas,
    std::vector<glm::mat4>& outOriginalTransforms) {

    outTlas.nodes.clear();
    outTlas.rootIndex = 0; // Default for an empty TLAS
    outTlas.transform = glm::mat4(1.0f); // TLAS is in world space
    outOriginalTransforms.clear();

    if (bvhs.empty()) {
        GE_CORE_INFO("buildTLAS called with empty BVHs list.");
        // Ensure a single dummy node if BVH must not be empty by contract elsewhere
        outTlas.nodes.emplace_back(); // Default node
        outTlas.nodes[0].leftChildIndex = BVH_INVALID_NODE_INDEX;
        outTlas.nodes[0].rightChildIndex = BVH_INVALID_NODE_INDEX;
        outTlas.nodes[0].minBounds = glm::vec3(0.0f);
        outTlas.nodes[0].maxBounds = glm::vec3(0.0f);
        outTlas.nodes[0].primitiveIdx = static_cast<uint32_t>(-1);
        outTlas.rootIndex = 0;
        return;
    }

    std::vector<TlasBuildPrimitive> buildPrimitives;
    buildPrimitives.reserve(bvhs.size());
    outOriginalTransforms.reserve(bvhs.size());

    for (uint32_t i = 0; i < bvhs.size(); ++i) {
        const auto& blas = bvhs[i];
        if (blas.nodes.empty() || blas.rootIndex >= blas.nodes.size()) {
            GE_CORE_WARN("Skipping BLAS %u in TLAS construction as it has no nodes or invalid root.", i);
            // To keep outOriginalTransforms aligned if some BVHs are skipped, 
            // we might need to push a placeholder or adjust indexing logic later.
            // For now, assume valid BLASes are processed sequentially.
            // If an invalid BLAS means its transform should not be stored, this is fine.
            // If all BLAS transforms must be stored regardless of validity for TLAS, push default.
            outOriginalTransforms.push_back(blas.transform); // Store transform even if skipped, for consistency with original vector
            continue; 
        }

        auto worldAABB = calculateWorldAABBForBlas(blas);
        
        TlasBuildPrimitive prim;
        prim.worldMinBounds = worldAABB.first;
        prim.worldMaxBounds = worldAABB.second;
        prim.centroid = (prim.worldMinBounds + prim.worldMaxBounds) * 0.5f;
        prim.originalBvhCpuIndex = i; // Store original index

        buildPrimitives.push_back(prim);
        outOriginalTransforms.push_back(blas.transform); // Store if processed
    }

    if (buildPrimitives.empty()) {
        GE_CORE_WARN("No valid BLASes to build TLAS from.");
        outTlas.nodes.emplace_back(); // Default node
        outTlas.nodes[0].leftChildIndex = BVH_INVALID_NODE_INDEX;
        outTlas.nodes[0].rightChildIndex = BVH_INVALID_NODE_INDEX;
        outTlas.nodes[0].minBounds = glm::vec3(0.0f);
        outTlas.nodes[0].maxBounds = glm::vec3(0.0f);
        outTlas.nodes[0].primitiveIdx = static_cast<uint32_t>(-1);
        outTlas.rootIndex = 0;
        return;
    }
    
    outTlas.nodes.reserve(2 * buildPrimitives.size() + 1); 

    outTlas.rootIndex = recursiveBuildSAH(buildPrimitives, 0, static_cast<int>(buildPrimitives.size()), outTlas.nodes);

    GE_CORE_INFO("TLAS built with {} nodes.", outTlas.nodes.size());
}


void BVH::refitAABBsRecursive(uint32_t nodeIndex, std::vector<BVHNode>& tlasNodes) {
    if (nodeIndex == static_cast<uint32_t>(BVH_INVALID_NODE_INDEX) || nodeIndex >= tlasNodes.size()) {
        GE_CORE_WARN("refitAABBsRecursive called with invalid nodeIndex %u", nodeIndex);
        return;
    }

    BVHNode& node = tlasNodes[nodeIndex];
    if (node.leftChildIndex == BVH_INVALID_NODE_INDEX) { 
        return; 
    }

    uint32_t leftChildIdx = static_cast<uint32_t>(node.leftChildIndex);
    uint32_t rightChildIdx = static_cast<uint32_t>(node.rightChildIndex);

    // It's possible child indices are valid numbers but point outside current node list
    // if BVH construction had errors. This check is after the BVH_INVALID_NODE_INDEX check.
    bool leftValid = leftChildIdx < tlasNodes.size() && leftChildIdx != static_cast<uint32_t>(BVH_INVALID_NODE_INDEX);
    bool rightValid = rightChildIdx < tlasNodes.size() && rightChildIdx != static_cast<uint32_t>(BVH_INVALID_NODE_INDEX);

    if (leftValid) refitAABBsRecursive(leftChildIdx, tlasNodes);
    if (rightValid) refitAABBsRecursive(rightChildIdx, tlasNodes);

    if (leftValid && rightValid) {
        const BVHNode& leftChild = tlasNodes[leftChildIdx];
        const BVHNode& rightChild = tlasNodes[rightChildIdx];
        node.minBounds = glm::min(leftChild.minBounds, rightChild.minBounds);
        node.maxBounds = glm::max(leftChild.maxBounds, rightChild.maxBounds);
    } else if (leftValid) {
        const BVHNode& leftChild = tlasNodes[leftChildIdx];
        node.minBounds = leftChild.minBounds;
        node.maxBounds = leftChild.maxBounds;
        GE_CORE_WARN("Refitting node %u with only left child %u.", nodeIndex, leftChildIdx);
    } else if (rightValid) {
        const BVHNode& rightChild = tlasNodes[rightChildIdx];
        node.minBounds = rightChild.minBounds;
        node.maxBounds = rightChild.maxBounds;
        GE_CORE_WARN("Refitting node %u with only right child %u.", nodeIndex, rightChildIdx);
    } else {
        GE_CORE_ERROR("Cannot refit node %u AABB, both children (%u, %u) are invalid or out of bounds.", nodeIndex, leftChildIdx, rightChildIdx);
        // Keep node AABB as is, or set to a known invalid/degenerate state if preferred.
        // node.minBounds = glm::vec3(std::numeric_limits<float>::max());
        // node.maxBounds = glm::vec3(std::numeric_limits<float>::lowest());
    }
}


bool BVH::updateTLAS(
    BVHCPU& tlasToUpdate,
    const std::vector<BVHCPU>& currentBvhs,
    std::vector<glm::mat4>& originalAndUpdatedTransforms) {

    if (tlasToUpdate.nodes.empty() || tlasToUpdate.rootIndex >= tlasToUpdate.nodes.size()) {
        GE_CORE_INFO("updateTLAS called on an empty or invalid TLAS. Full rebuild might be better.");
        return false; 
    }

    if (currentBvhs.size() != originalAndUpdatedTransforms.size()) {
        GE_CORE_WARN("TLAS update failed: Mismatch current BVHs count (%zu) vs tracked transforms (%zu). Full rebuild required.", currentBvhs.size(), originalAndUpdatedTransforms.size());
        return false; 
    }

    bool needsRefit = false;

    for (uint32_t i = 0; i < tlasToUpdate.nodes.size(); ++i) {
        BVHNode& tlasNode = tlasToUpdate.nodes[i];

        if (tlasNode.leftChildIndex == BVH_INVALID_NODE_INDEX) { 
            uint32_t originalBlasIndex = tlasNode.primitiveIdx;

            if (originalBlasIndex >= currentBvhs.size() || originalBlasIndex >= originalAndUpdatedTransforms.size()) {
                GE_CORE_ERROR("TLAS leaf node %u has invalid originalBlasIndex %u. Max is %zu.", i, originalBlasIndex, std::min(currentBvhs.size(), originalAndUpdatedTransforms.size()) -1);
                continue; 
            }
            
            const BVHCPU& currentBlas = currentBvhs[originalBlasIndex];
            if (currentBlas.nodes.empty() || currentBlas.rootIndex >= currentBlas.nodes.size()) {
                GE_CORE_WARN("BLAS %u (referenced by TLAS leaf %u) is empty or invalid during update.", originalBlasIndex, i);
                if (glm::any(glm::notEqual(tlasNode.minBounds, glm::vec3(0.f))) || glm::any(glm::notEqual(tlasNode.maxBounds, glm::vec3(0.f)))) {
                    tlasNode.minBounds = glm::vec3(0.f);
                    tlasNode.maxBounds = glm::vec3(0.f);
                    needsRefit = true; 
                }
                continue;
            }

            if (currentBlas.transform != originalAndUpdatedTransforms[originalBlasIndex]) {
                needsRefit = true;
                originalAndUpdatedTransforms[originalBlasIndex] = currentBlas.transform;
                
                auto newWorldAABB = calculateWorldAABBForBlas(currentBlas);
                tlasNode.minBounds = newWorldAABB.first;
                tlasNode.maxBounds = newWorldAABB.second;
            } else {
                // Optional: check if BLAS local AABB itself changed. For now, only transform change triggers AABB update.
                // To do this, we would need to store the BLAS's root AABB at TLAS build time and compare.
                // This simplified version assumes BLAS local AABBs are static unless their transform changes.
            }
        }
    }

    if (needsRefit) {
        if (tlasToUpdate.rootIndex < tlasToUpdate.nodes.size()) {
            refitAABBsRecursive(tlasToUpdate.rootIndex, tlasToUpdate.nodes);
        } else {
            GE_CORE_ERROR("TLAS rootIndex %u is invalid for node count %zu during refit attempt.", tlasToUpdate.rootIndex, tlasToUpdate.nodes.size());
            return false; 
        }
        return true;
    }

    return false; 
}

} // namespace Rapture