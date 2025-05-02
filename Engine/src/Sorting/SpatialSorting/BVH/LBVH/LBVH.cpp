#include "LBVH.h"

#include "../../../../AssetsManager/AssetManager.h"
#include "../../../../Logger/Log.h"
#include "../../../../Scenes/Components/Components.h"
#include "../../../../Debug/TracyProfiler.h"
#include <string> 
#include <algorithm> 

#include "../../../../WindowContext/Application.h"

#define MAX_TRIANGLE_COUNT 100000

namespace Rapture {




    std::shared_ptr<LBVH> LBVHManager::s_lbvh = nullptr;
    std::vector<std::pair<glm::vec3, glm::vec3>> LBVHManager::s_Boxes;
    std::vector<glm::mat4> LBVHManager::s_transforms;
    bool LBVHManager::s_isInitialized = false;
    std::vector<std::pair<glm::vec3, glm::vec3>> LBVHManager::s_BoxesSubset;
    std::vector<glm::mat4> LBVHManager::s_transformsSubset;
    // Initialize new static members for depth-based retrieval
    std::vector<std::pair<glm::vec3, glm::vec3>> LBVHManager::s_BoxesAtDepth;
    std::vector<glm::mat4> LBVHManager::s_transformsAtDepth;
    int LBVHManager::s_depthLevel = -1;

    LBVH::LBVH(uint32_t maxTriangleCount = MAX_TRIANGLE_COUNT)
    : m_RadixSort(maxTriangleCount),
      m_isDirty(true),
      m_lastTriangleCount(0)
    {
        auto& app = Application::getInstance();
        auto project = app.getProject();
        if (!project) {
            GE_RENDER_ERROR("LBVH::LBVH - Project not found, unable to start LBVH");
            return;
        }
        auto shaderPath = project->getConfig().shaderPath;

        auto [structureShader, structureHandle] = AssetManager::importAsset<Shader>(shaderPath / m_StructureShaderPath);
        auto [aabbShader, aabbHandle] = AssetManager::importAsset<Shader>(shaderPath / m_AABBShaderPath);

        m_StructureShader = structureShader;
        m_AABBShader = aabbShader;

        m_SortedMortonElementsBuffer = nullptr;
        m_PrimitiveAABBsBuffer = nullptr;

        m_BVHNodesBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(LBVHNode) * 2 * maxTriangleCount - 1, BufferUsage::Dynamic);
        m_LBVHConstructionInfoBuffer = std::make_shared<ShaderStorageBuffer>(sizeof(LBVHConstructionInfo) * 2 * maxTriangleCount - 1, BufferUsage::Dynamic);


    }

    LBVH::~LBVH()
    {

    }

    void LBVH::generate(const MeshBufferData& meshBufferData)
    {
        RAPTURE_PROFILE_FUNCTION();
        RAPTURE_PROFILE_GPU_SCOPE("LBVH::generate");

        // Radix sort pass(es)
        m_RadixSort.sort(meshBufferData);
        m_SortedMortonElementsBuffer = m_RadixSort.getSortedIndicesBuffer();
        m_PrimitiveAABBsBuffer = m_RadixSort.getPrimitiveAABBsBuffer();

        

        if (meshBufferData.triangleCount == 0) {
            return;
        }
        if (m_BVHNodesBuffer == nullptr) {
            GE_CORE_ERROR("BVHNodesBuffer is not initialized");
            return;
        }
        if (m_SortedMortonElementsBuffer == nullptr) {
            GE_CORE_ERROR("SortedMortonElementsBuffer is not initialized");
            return;
        }
        if (m_PrimitiveAABBsBuffer == nullptr) {
            GE_CORE_ERROR("PrimitiveAABBsBuffer is not initialized");
            return;
        }


        const uint32_t workgroupSize = 256; // Assuming local_size_x = 256 in shader
        uint32_t numWorkGroupsStructure = (meshBufferData.triangleCount + workgroupSize - 1) / workgroupSize;

        
        // Structure pass
        m_StructureShader->bind();
        m_StructureShader->setUint("u_numPrimitives", meshBufferData.triangleCount);

        m_SortedMortonElementsBuffer->bindBase(0);
        m_BVHNodesBuffer->bindBase(1);
        m_PrimitiveAABBsBuffer->bindBase(2);
        m_LBVHConstructionInfoBuffer->bindBase(3);

        m_StructureShader->dispatchCompute(meshBufferData.triangleCount, 1, 1);
        
        ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true});
        
        m_StructureShader->unBind();

        
        // AABB pass
        m_AABBShader->bind();
        m_AABBShader->setUint("u_numPrimitives", meshBufferData.triangleCount);

        m_BVHNodesBuffer->bindBase(0);
        m_LBVHConstructionInfoBuffer->bindBase(1);

        m_AABBShader->dispatchCompute(meshBufferData.triangleCount, 1, 1);
        
        
        ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true});

        m_AABBShader->unBind();

        // Store the triangle count for later use when reading back the buffer
        m_lastTriangleCount = meshBufferData.triangleCount;
        // Mark the CPU data as dirty since the GPU data has been updated
        m_isDirty = true;


    }
        
    std::vector<BVHNode> LBVH::getCPUBVHNodes()
    {
        RAPTURE_PROFILE_FUNCTION();

        // Only read from GPU if the data is marked dirty or the CPU cache is empty
        if (!m_isDirty && !m_cpuBVHNodes.empty()) {
            return {};
        }
        
        if (!m_BVHNodesBuffer) {
            GE_CORE_ERROR("BVHNodesBuffer is null, cannot read data.");
            // Return potentially empty or stale data, but log error.
            // Consider throwing an exception or returning an optional/pointer.
            return {}; 
        }

        if (m_lastTriangleCount == 0) {
            GE_CORE_WARN("LBVH::getCPUBVHNodes called before generate or with zero triangles. Returning empty vector.");
            m_cpuBVHNodes.clear(); // Ensure it's empty
            m_isDirty = false; // We've handled this state
            return {};
        }

        // Calculate the expected number of nodes in the BVH
        // A complete BVH for N primitives has 2N - 1 nodes.
        size_t nodeCount = (m_lastTriangleCount > 0) ? (2 * m_lastTriangleCount - 1) : 0;
        size_t bufferSize = nodeCount * sizeof(LBVHNode);

        // Resize the CPU vector to hold the data
        std::vector<LBVHNode> nodes = std::vector<LBVHNode>(nodeCount);

        GE_CORE_INFO("Reading BVH nodes from GPU. Triangle count: {}, Node count: {}, Buffer size: {}", 
                    m_lastTriangleCount, nodeCount, bufferSize);

        // Read the data from the GPU buffer
        // Ensure the buffer has finished writing before reading
        ShaderStorageBuffer::barrier(SSBOBarrierFlags{true, true}); // Add a memory barrier before reading
            
        // Map the buffer to read data
        void* mappedData = m_BVHNodesBuffer->map(0, bufferSize);
        if (mappedData) {
            // Copy the data from the mapped buffer to the CPU vector
            memcpy(nodes.data(), mappedData, bufferSize);
            // Unmap the buffer now that we're done reading
            m_BVHNodesBuffer->unmap();
        } else {
            GE_CORE_ERROR("Failed to map BVHNodesBuffer for reading.");
            // Clear the vector to indicate failure, or handle error appropriately
            nodes.clear(); 
        }

        // Data is now up-to-date on the CPU (or cleared on error)
        m_isDirty = false;
        
        std::vector<BVHNode> BVHNodes = std::vector<BVHNode>(nodes.size());
        for (uint32_t i = 0; i < nodes.size(); i++) {
            BVHNodes[i].maxBounds = glm::vec3(nodes[i].aabbMaxX, nodes[i].aabbMaxY, nodes[i].aabbMaxZ);
            BVHNodes[i].minBounds = glm::vec3(nodes[i].aabbMinX, nodes[i].aabbMinY, nodes[i].aabbMinZ);
            BVHNodes[i].leftChildIndex = nodes[i].left;
            BVHNodes[i].rightChildIndex = nodes[i].right;
            BVHNodes[i].primitiveCount = nodes[i].primitiveIdx;
        }


        return BVHNodes;
    }

    // sorts all of the geometry in an entire scene, this way we can calculated the max_triangles
    void LBVH::generate(std::shared_ptr<Scene> scene)
    {
        RAPTURE_PROFILE_FUNCTION();

        auto& registry = scene->getRegistry();
        auto view = registry.view<MeshComponent, TransformComponent>();
        for (auto entity : view) {
            auto& meshComponent = view.get<MeshComponent>(entity);
            auto& transformComponent = view.get<TransformComponent>(entity);
            generate(meshComponent.mesh->getMeshData());

            auto nodes = getCPUBVHNodes();
            BVHCPU bvhCPU;
            bvhCPU.nodes = nodes;
            bvhCPU.transform = transformComponent.transformMatrix();
            m_cpuBVHNodes.push_back(bvhCPU);
        }
    }



    void LBVHManager::init(std::shared_ptr<Scene> scene)
    {
        RAPTURE_PROFILE_FUNCTION();

        s_lbvh = std::make_shared<LBVH>();
        s_isInitialized = true;

        s_lbvh->generate(scene);

        auto nodes = s_lbvh->getAllCPUBVHNodes();
        size_t totalNodes = 0;
        for (const auto& bvhLevel : nodes) {
            for (const auto& node : bvhLevel.nodes) {
                    glm::vec3 size = node.maxBounds - node.minBounds;
                    glm::vec3 center = (node.minBounds + node.maxBounds) * 0.5f;
                    if (glm::length(size) < 0.0001f) {
                        continue;
                    }
                    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), center) *
                                        glm::scale(glm::mat4(1.0f), size);
                    modelMatrix = bvhLevel.transform * modelMatrix; 
                    s_Boxes.push_back({node.minBounds, node.maxBounds}); 
                    s_transforms.push_back(modelMatrix);
                    totalNodes++;
            }
        }

        GE_CORE_INFO("LBVHManager - Generated {} BVH boxes for visualization.", totalNodes);

    }

    void LBVHManager::shutdown()
    {
        s_lbvh = nullptr; // Release shared pointer
        s_Boxes.clear();
        s_transforms.clear();
        s_BoxesSubset.clear();
        s_transformsSubset.clear();
        // Clear new depth-based subset vectors
        s_BoxesAtDepth.clear();
        s_transformsAtDepth.clear();
        s_isInitialized = false;
        GE_CORE_INFO("LBVHManager shutdown.");
    }

    // Add the new static function implementation
    void LBVHManager::printTreeStructure(uint32_t meshIndex) {
    }

    std::shared_ptr<LBVH> LBVHManager::getLBVH()
    {
        if (!s_isInitialized) {
            GE_CORE_ERROR("LBVHManager is not initialized");
            return nullptr;
        }
        return s_lbvh;
    }

    std::vector<std::pair<glm::vec3, glm::vec3>> LBVHManager::getBoxes(uint32_t start, uint32_t end)
    {
        if (!s_isInitialized) {
            GE_CORE_ERROR("LBVHManager is not initialized");
            return s_Boxes;
        }
        if (start >= s_Boxes.size() || end >= s_Boxes.size()) {
            GE_CORE_ERROR("Invalid range for boxes");
            return s_Boxes;
        }
        if (start > end) {
            GE_CORE_ERROR("Invalid range for boxes");
            return s_Boxes;
        }

        std::vector<std::pair<glm::vec3, glm::vec3>> subBoxes(s_Boxes.begin() + start, s_Boxes.begin() + end);
        return subBoxes;
    }

    std::vector<glm::mat4> LBVHManager::getTransforms(uint32_t start, uint32_t end)
    {
        if (!s_isInitialized) {
            GE_CORE_ERROR("LBVHManager is not initialized");
            return s_transforms;
        }
        if (start >= s_transforms.size() || end >= s_transforms.size()) {
            GE_CORE_ERROR("Invalid range for transforms");
            return s_transforms;
        }
        if (start > end) {
            GE_CORE_ERROR("Invalid range for transforms");
            return s_transforms;
        }

        std::vector<glm::mat4> subTransforms(s_transforms.begin() + start, s_transforms.begin() + end);
        return subTransforms;
    }

    void LBVHManager::setInterval(uint32_t start, uint32_t end)
    {
        if (start >= s_Boxes.size() || end >= s_Boxes.size()) {
            GE_CORE_ERROR("LBVHManager::setInterval - Invalid range for boxes");
            return; 
        }
        if (start > end) {
            GE_CORE_ERROR("LBVHManager::setInterval - Invalid range for boxes");
            return;
        }

        s_BoxesSubset = getBoxes(start, end);
        s_transformsSubset = getTransforms(start, end);
    }

    std::vector<std::pair<glm::vec3, glm::vec3>>& LBVHManager::getBoxesSubset()
    {
        return s_BoxesSubset;
    }

    std::vector<glm::mat4>& LBVHManager::getTransformsSubset()
    {
        return s_transformsSubset;
    }


    std::vector<std::pair<glm::vec3, glm::vec3>>& LBVHManager::getBoxes()
    {
        return s_Boxes;
    }

    std::vector<glm::mat4>& LBVHManager::getTransforms()
    {
        return s_transforms;
    }
    
    


    // --- Depth-based retrieval implementation ---

    // Helper function to recursively traverse the BVH and collect nodes at a specific depth
    void traverseNodeForDepth(
        const BVHCPU& bvhCpu,
        uint32_t nodeIndex,
        uint32_t currentDepth,
        uint32_t targetDepth,
        std::vector<std::pair<glm::vec3, glm::vec3>>& boxesAtDepth,
        std::vector<glm::mat4>& transformsAtDepth)
    {
        // Base case: invalid node index
        if (nodeIndex == UINT32_MAX || nodeIndex >= bvhCpu.nodes.size()) {
            return;
        }

        const auto& node = bvhCpu.nodes[nodeIndex];

        // If we are at the target depth, collect the node's data
        if (currentDepth == targetDepth) {
            glm::vec3 size = node.maxBounds - node.minBounds;
            glm::vec3 center = (node.minBounds + node.maxBounds) * 0.5f;

            // Avoid adding degenerate boxes
            if (glm::length(size) >= 0.0001f) {
                glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), center) *
                                        glm::scale(glm::mat4(1.0f), size);
                modelMatrix = bvhCpu.transform * modelMatrix;

                boxesAtDepth.push_back({node.minBounds, node.maxBounds});
                transformsAtDepth.push_back(modelMatrix);
            }
            // Stop recursion here, we only want nodes *at* this depth
            return;
        }

        if (node.leftChildIndex == 0 || node.rightChildIndex == 0) {
            return;
        }

        // If we are below the target depth, recurse into children
        if (currentDepth < targetDepth) {
            // Only recurse if it's an internal node (primitiveCount == 0)
            // Although the depth check already handles this implicitly,
            // checking primitiveCount avoids unnecessary recursion if a leaf is reached before targetDepth.
            if (node.primitiveCount == 0) {
                 traverseNodeForDepth(bvhCpu, node.leftChildIndex, currentDepth + 1, targetDepth, boxesAtDepth, transformsAtDepth);
                 traverseNodeForDepth(bvhCpu, node.rightChildIndex, currentDepth + 1, targetDepth, boxesAtDepth, transformsAtDepth);
            }
        }
    }

    void LBVHManager::setDepthLevel(uint32_t depth)
    {
        RAPTURE_PROFILE_FUNCTION();
        if (!s_isInitialized) {
            GE_CORE_ERROR("LBVHManager::setDepthLevel - LBVHManager is not initialized.");
            return;
        }
        if (!s_lbvh) {
             GE_CORE_ERROR("LBVHManager::setDepthLevel - LBVH instance is null.");
             return;
        }

        if (depth == s_depthLevel) {
            return;
        }

        s_BoxesAtDepth.clear();
        s_transformsAtDepth.clear();

        const auto& allBvhs = s_lbvh->getAllCPUBVHNodes();

        if (allBvhs.empty()) {
            GE_CORE_WARN("LBVHManager::setDepthLevel - No BVHs available to traverse.");
            return;
        }


        for (const auto& bvhCpu : allBvhs) {
            if (bvhCpu.nodes.empty()) continue; // Skip empty BVHs
            traverseNodeForDepth(bvhCpu, bvhCpu.rootIndex, 0, depth, s_BoxesAtDepth, s_transformsAtDepth);
        }


    }

    std::vector<std::pair<glm::vec3, glm::vec3>>& LBVHManager::getBoxesAtDepth()
    {
        return s_BoxesAtDepth;
    }

    std::vector<glm::mat4>& LBVHManager::getTransformsAtDepth()
    {
        return s_transformsAtDepth;
    }

    // --- End of Depth-based retrieval implementation ---

}