#pragma once


#include "../../../RadixSortGPU/RadixSort.h"

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <utility>

#include "../../../../Scenes/Scene.h"


namespace Rapture {



struct BVHNode {
    glm::vec3 minBounds;
    glm::vec3 maxBounds;
    uint32_t leftChildIndex; // Index of left child.
    uint32_t rightChildIndex; // Index of right child.
    uint32_t parentIndex; // Index of parent node.
    uint32_t primitiveCount;        // 0 for internal, >0 for leaf

};

// output of the builder; it is necessary to allocate the (empty) buffer
struct LBVHNode {
    int left;// pointer to the left child or INVALID_POINTER in case of leaf
    int right;// pointer to the right child or INVALID_POINTER in case of leaf
    uint32_t primitiveIdx;// custom value that is copied from the input Element or 0 in case of inner node
    float aabbMinX;// aabb of the node
    float aabbMinY;
    float aabbMinZ;
    float aabbMaxX;
    float aabbMaxY;
    float aabbMaxZ;
};


struct LBVHConstructionInfo {
    uint32_t parent;// pointer to the parent
    int visitationCount;// number of threads that arrived
};

struct BVHCPU {
    std::vector<BVHNode> nodes;
    uint32_t rootIndex=0;
    glm::mat4 transform;
};

class LBVH {
    public:
        LBVH(uint32_t maxTriangleCount);
        ~LBVH();

        void generate(const MeshBufferData& meshBufferData);
        void generate(std::shared_ptr<Scene> scene);


        // Returns a const reference to the vector containing all generated BVHs (one per mesh)
        const std::vector<BVHCPU>& getAllCPUBVHNodes() const { return m_cpuBVHNodes; };

    private:
        // Reads the BVH node data from the GPU buffer to the CPU if necessary
        // and returns a const reference to the cached CPU-side vector.
        std::vector<BVHNode> getCPUBVHNodes();

    private:
        std::shared_ptr<Shader> m_StructureShader;
        std::shared_ptr<Shader> m_AABBShader;

        RadixSort m_RadixSort;

        std::shared_ptr<ShaderStorageBuffer> m_SortedMortonElementsBuffer;
        std::shared_ptr<ShaderStorageBuffer> m_BVHNodesBuffer;
        std::shared_ptr<ShaderStorageBuffer> m_PrimitiveAABBsBuffer;
        std::shared_ptr<ShaderStorageBuffer> m_LBVHConstructionInfoBuffer;
        
        std::string m_StructureShaderPath = "Sorting/BVH/LBVH/LBVH_Structure.cs.glsl";
        std::string m_AABBShaderPath = "Sorting/BVH/LBVH/LBVH_AABB.cs.glsl";

        // CPU-side cache for BVH nodes
        std::vector<BVHCPU> m_cpuBVHNodes;
        // Dirty flag to track if GPU data needs to be re-read
        bool m_isDirty = true;
        // Store the triangle count from the last generation for buffer size calculation
        uint32_t m_lastTriangleCount = 0;



};

class LBVHManager {
    public:
        static void init(std::shared_ptr<Scene> scene);
        static void shutdown();

        static std::shared_ptr<LBVH> getLBVH();
        static std::vector<std::pair<glm::vec3, glm::vec3>> getBoxes(uint32_t start, uint32_t end);
        static std::vector<glm::mat4> getTransforms(uint32_t start, uint32_t end);

        static void setInterval(uint32_t start, uint32_t end);

        static std::vector<std::pair<glm::vec3, glm::vec3>>& getBoxesSubset();
        static std::vector<glm::mat4>& getTransformsSubset();

        static std::vector<std::pair<glm::vec3, glm::vec3>>& getBoxes();
        static std::vector<glm::mat4>& getTransforms();

        // New function to print the structure of the managed BVH trees
        static void printTreeStructure(uint32_t meshIndex);

        // New methods for depth-based retrieval
        static void setDepthLevel(uint32_t depth);
        static std::vector<std::pair<glm::vec3, glm::vec3>>& getBoxesAtDepth();
        static std::vector<glm::mat4>& getTransformsAtDepth();


    private:
        static std::shared_ptr<LBVH> s_lbvh;
        static std::vector<std::pair<glm::vec3, glm::vec3>> s_Boxes;
        static std::vector<glm::mat4> s_transforms;

        // two dummies for debug purposes,
        // for when i only need a subset of the BVH
        static std::vector<std::pair<glm::vec3, glm::vec3>> s_BoxesSubset;
        static std::vector<glm::mat4> s_transformsSubset;

        // New members for depth-based subsets
        static std::vector<std::pair<glm::vec3, glm::vec3>> s_BoxesAtDepth;
        static std::vector<glm::mat4> s_transformsAtDepth;

        static int s_depthLevel;

        static bool s_isInitialized;

};



};


