#pragma once

#include "../Scenes/Scene.h"

#include <glm/glm.hpp>
#include <cstdint>
#include <array>
#include <vector>

namespace Rapture {
namespace Entropy {


struct Bounds {
    glm::vec3 min;
    glm::vec3 max;

    glm::vec3 getCenter() {
        return (min + max) * 0.5f;
    }

    glm::vec3 getExtents() {
        return (max - min) * 0.5f;
    }

    bool contains(const Bounds& other) const {
        return other.min.x >= min.x && other.max.x <= max.x &&
               other.min.y >= min.y && other.max.y <= max.y &&
               other.min.z >= min.z && other.max.z <= max.z;
    }

    bool intersects(const Bounds& other) const {
        return (max.x >= other.min.x && min.x <= other.max.x) &&
               (max.y >= other.min.y && min.y <= other.max.y) &&
               (max.z >= other.min.z && min.z <= other.max.z);
    }
};



// Tree representations for octrees
// 1. standard node based tree
// 2. heap ()
// 3. array of nodes ✅

struct OctreeNode {
    
    Bounds bounds;

    uint32_t parent;
    std::array<uint32_t, 8> children;

};

// used as a lead node, needs to be different than a regular octree node
// because its should contain different children and the amount of children is not fixed
// unlike the octree having a max of 8 children
struct OctreeLeafNode {
    uint32_t parent;
    
    std::vector<uint32_t> entities;
};

// reconstructs the octree each frame
class StaticOctree {

public:
    StaticOctree(std::shared_ptr<Scene> scene);

    void buildTree(std::shared_ptr<Scene> scene);

    void query(const Bounds& bounds, std::vector<uint32_t>& results);




private:
    std::vector<OctreeNode> m_nodes;

    static const int MAX_DEPTH = 8;
    static constexpr float MIN_NODE_SIZE = 0.5f;
};

// updates the octree each frame
class DynamicOctree {

};


} // namespace Entropy
} // namespace Rapture
