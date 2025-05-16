#pragma once

#include "glm/glm.hpp"
#include <vector>

namespace Rapture {

struct BVHNode {
    alignas(4) int leftChildIndex; // Index of left child.
    alignas(4) int rightChildIndex; // Index of right child.

    alignas(4) uint32_t primitiveIdx;  

    alignas(16) glm::vec3 minBounds;
    alignas(16) glm::vec3 maxBounds;

};

struct BVHCPU {
    std::vector<BVHNode> nodes;
    uint32_t rootIndex=0; // relative to a specific mesh bvh
    uint32_t absoluteRootIndex=0; // index inside a full bvh buffer with bvhs from other meshes, should be better when we have a tlas
    glm::mat4 transform;
};

}