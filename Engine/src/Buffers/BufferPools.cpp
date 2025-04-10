#pragma once

#include "BufferPools.h"
#include <algorithm>
#include <cassert>

#include "OpenGLBuffers/VertexBuffers/OpenGLVertexBuffer.h"
#include "OpenGLBuffers/IndexBuffers/OpenGLIndexBuffer.h"
#include "VertexArray.h"

namespace Rapture {

    // Initialize static members
    std::unique_ptr<BufferPoolManager> BufferPoolManager::s_instance = nullptr;
    std::once_flag BufferPoolManager::s_initInstanceFlag;


    //------------------------------------------------------------------
    // BufferPoolManager Implementation
    //------------------------------------------------------------------

    void BufferPoolManager::init() {
        // call once maks it thread safe
        std::call_once(s_initInstanceFlag, []() {
            s_instance = std::unique_ptr<BufferPoolManager>(new BufferPoolManager());
            GE_CORE_INFO("BufferPoolManager initialized");
        });
    }

    void BufferPoolManager::shutdown() {
        if (s_instance) {
            s_instance->m_vaoToBufferAllocationsMap.clear();
            s_instance->m_layoutToVAOMap.clear();
            s_instance.reset();
            GE_CORE_INFO("BufferPoolManager shutdown");
        }
    }

    BufferPoolManager& BufferPoolManager::getInstance() {
        if (!s_instance) {
            init();
        }
        return *s_instance;
    }

    BufferPoolManager::BufferPoolManager() {

    }

    BufferPoolManager::~BufferPoolManager() {
        // Clear all pools and resources

    }

    MeshBufferData BufferPoolManager::allocateMeshData(
        const BufferLayout& layout,
        const void* vertexData, size_t vertexDataSize,
        const void* indexData, size_t indexDataSize, size_t indexCount,
        unsigned int indexType)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        MeshBufferData meshData;
        
        // Find or create a vertex array with the given buffer layout
        auto vao = findOrCreateVertexArray(layout, vertexDataSize, indexDataSize, indexType);

        if (!vao) {
             GE_CORE_ERROR("BufferPoolManager::allocateMeshData: Failed to find or create a suitable VAO.");
             return meshData;
        }


        // Allocate the vertex data within the chosen VAO
        auto vertexAllocation = allocateBuffer(vao, BufferType::Vertex, vertexDataSize);
        // Allocate the index data within the chosen VAO
        auto indexAllocation = allocateBuffer(vao, BufferType::Index, indexDataSize);


        if (!vertexAllocation || !indexAllocation) {
            GE_CORE_ERROR("BufferPoolManager::allocateMeshData: Failed to allocate vertex ({0} bytes) or index ({1} bytes) buffer within VAO {2}, even after finding/creating it.",
                          vertexDataSize, indexDataSize, vao ? vao->getID() : 0);
            if (vertexAllocation && !vertexAllocation->isAllocated) meshData.vertexAllocation->isAllocated = false;
            if (indexAllocation && !indexAllocation->isAllocated) meshData.indexAllocation->isAllocated = false;
            return MeshBufferData{};
        }

        meshData.vao = vao;
        meshData.vertexAllocation = vertexAllocation;
        meshData.indexAllocation = indexAllocation;
        meshData.indexType = indexType;
        meshData.indexCount = indexCount;
        meshData.vertexOffsetInVertices = vertexAllocation->offsetBytes / vao->getBufferLayout().vertexSize;

        vao->getVertexBuffer()->setData(vertexData, vertexDataSize, vertexAllocation->offsetBytes);
        vao->getIndexBuffer()->setData(indexData, indexDataSize, indexAllocation->offsetBytes);

        
        return meshData;
    }

    std::shared_ptr<BufferAllocation> BufferPoolManager::allocateBuffer(std::shared_ptr<VertexArray> vao, BufferType type, size_t size) {
        if (!vao) {
            GE_CORE_ERROR("BufferPoolManager::allocateBuffer: Attempted to allocate buffer for null VAO");
            return nullptr;
        }

        unsigned int vaoId = vao->getID();
        if (vaoId == 0) {
            GE_CORE_ERROR("BufferPoolManager::allocateBuffer: Attempted to allocate buffer for VAO with invalid ID: {0}", vaoId);
            return nullptr;
        }
        

        auto& allocations = m_vaoToBufferAllocationsMap[vaoId];
        for (auto allocation : allocations) {
            if (allocation->bufferType == type && !allocation->isAllocated && allocation->sizeBytes >= size) {
                allocation->isAllocated = true;
                
                splitBufferPoolAllocation(allocation, size, vao);
                
                return allocation;
            }
        }

        GE_CORE_WARN("BufferPoolManager::allocateBuffer: Failed to find a suitable free block for {0} bytes of type {1} in VAO ID: {2}", size, (type == BufferType::Vertex ? "Vertex" : "Index"), vaoId);
        for(const auto& alloc : allocations) {
            alloc->print();
        }
        return nullptr;
    }

    void BufferPoolManager::splitBufferPoolAllocation(std::shared_ptr<BufferAllocation> allocation, size_t allocatedSize, std::shared_ptr<VertexArray> vao) {
         if (!allocation || !vao) {
            GE_CORE_ERROR("BufferPoolManager::splitBufferPoolAllocation: Invalid allocation or VAO pointer.");
            return;
        }

        unsigned int vaoId = vao->getID();
         if (vaoId == 0) {
            GE_CORE_ERROR("BufferPoolManager::splitBufferPoolAllocation: Attempted to split allocation for VAO with invalid ID: {0}", vaoId);
            return;
        }

        size_t remainingSize = allocation->sizeBytes - allocatedSize;

        if (remainingSize > 0) {
             
            auto newAllocation = std::make_shared<BufferAllocation>(
                allocation->offsetBytes + allocatedSize,
                remainingSize,
                false,
                allocation->bufferType,
                allocation->bufferUsage);


            m_vaoToBufferAllocationsMap[vaoId].push_back(newAllocation);
            

            allocation->sizeBytes = allocatedSize;

        }
    }

    void BufferPoolManager::freeMeshData(MeshBufferData& meshData) {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!meshData.vao || !meshData.vertexAllocation || !meshData.indexAllocation) {
            GE_CORE_WARN("BufferPoolManager::freeMeshData: Attempting to free invalid MeshBufferData.");
            return;
        }

        unsigned int vaoId = meshData.vao->getID();
        
        if (meshData.vertexAllocation) {
             meshData.vertexAllocation->isAllocated = false;
        }
        if (meshData.indexAllocation) {
             meshData.indexAllocation->isAllocated = false;
        }
    }

    std::shared_ptr<VertexArray> BufferPoolManager::findOrCreateVertexArray(const BufferLayout& layout, size_t vertexDataSize, size_t indexDataSize, unsigned int indexType) {
        size_t layoutHash = layout.hash();
        auto mapIt = m_layoutToVAOMap.find(layoutHash);
        if (mapIt != m_layoutToVAOMap.end()) {
            auto& vaoList = mapIt->second;

            for (auto& vao : vaoList) {
                 if (!vao) {
                      GE_CORE_WARN("BufferPoolManager::findOrCreateVertexArray: Found null VAO pointer in list for hash {0}. Skipping.", layoutHash);
                      continue;
                 }
                unsigned int vaoId = vao->getID();
                if (vaoId == 0) {
                    GE_CORE_WARN("BufferPoolManager::findOrCreateVertexArray: Found VAO with invalid ID (0) in list for hash {0}. Skipping.", layoutHash);
                    continue;
                }


                auto allocMapIt = m_vaoToBufferAllocationsMap.find(vaoId);
                if (allocMapIt != m_vaoToBufferAllocationsMap.end()) {
                    bool foundVboSpace = false;
                    bool foundIboSpace = false;
                    
                    for (const auto& allocation : allocMapIt->second) {
                        if (!allocation->isAllocated) {
                            if (!foundVboSpace && allocation->bufferType == BufferType::Vertex && allocation->sizeBytes >= vertexDataSize) {
                                foundVboSpace = true;
                            }
                            if (!foundIboSpace && allocation->bufferType == BufferType::Index && allocation->sizeBytes >= indexDataSize) {
                                foundIboSpace = true;
                            }
                        }
                        if (foundVboSpace && foundIboSpace) {
                            return vao;
                        }
                    }
                } else {
                     GE_CORE_WARN("BufferPoolManager::findOrCreateVertexArray: VAO ID {0} found in layout map but not in allocation map!", vaoId);
                }
            }
        }

        // Create a new VAO for this layout
        auto vao = std::make_shared<VertexArray>();
        if (!vao) {
            GE_CORE_ERROR("BufferPoolManager::findOrCreateVertexArray: Failed to create new VAO object.");
            return nullptr;
        }

        unsigned int vaoId = vao->getID();
        if (vaoId == 0) {
            GE_CORE_ERROR("BufferPoolManager::findOrCreateVertexArray: Newly created VAO has invalid ID (0).");
            return nullptr;
        }

        size_t vertexPoolSize;
        size_t indexPoolSize;
        calculateNewBufferPairSize(vertexDataSize, indexDataSize, vertexPoolSize, indexPoolSize);

        if (vertexPoolSize == 0 || indexPoolSize == 0) {
             GE_CORE_ERROR("BufferPoolManager::findOrCreateVertexArray: Calculated buffer sizes are invalid (VBO: {0}, IBO: {1}). Cannot create VAO.", vertexPoolSize, indexPoolSize);
             return nullptr;
        }
        
        if (vertexPoolSize < vertexDataSize || indexPoolSize < indexDataSize) {
             GE_CORE_WARN("BufferPoolManager::findOrCreateVertexArray: Initial requested size (VBO: {0}, IBO: {1}) is larger than calculated pool size (VBO: {2}, IBO: {3}). Adjusting pool size.",
                           vertexDataSize, indexDataSize, vertexPoolSize, indexPoolSize);
             vertexPoolSize = std::max(vertexPoolSize, vertexDataSize);
             indexPoolSize = std::max(indexPoolSize, indexDataSize);
             GE_CORE_INFO("BufferPoolManager::findOrCreateVertexArray: Adjusted pool sizes to VBO: {0}, IBO: {1}", vertexPoolSize, indexPoolSize);
        }


        GE_CORE_INFO("BufferPoolManager::findOrCreateVertexArray: New VAO ({0}) - VBO Size: {1} bytes, IBO Size: {2} bytes", vaoId, vertexPoolSize, indexPoolSize);

        auto vertexBuffer = std::make_shared<VertexBuffer>(vertexPoolSize);
        auto indexBuffer = std::make_shared<IndexBuffer>(indexPoolSize, indexType);

        if (!vertexBuffer || vertexBuffer->getID() == 0 || !indexBuffer || indexBuffer->getID() == 0) {
             GE_CORE_ERROR("BufferPoolManager::findOrCreateVertexArray: Failed to create underlying VertexBuffer or IndexBuffer for new VAO {0}.", vaoId);
             return nullptr;
        }

        vao->setVertexBuffer(vertexBuffer);
        vao->setIndexBuffer(indexBuffer);
        vao->setBufferLayout(layout);
        
        m_layoutToVAOMap[layoutHash].push_back(vao);

        m_vaoToBufferAllocationsMap[vaoId].push_back(std::make_shared<BufferAllocation>(
            0, vertexPoolSize, false, BufferType::Vertex, BufferUsage::Static
        ));
        m_vaoToBufferAllocationsMap[vaoId].push_back(std::make_shared<BufferAllocation>(
            0, indexPoolSize, false, BufferType::Index, BufferUsage::Static
        ));
        GE_CORE_TRACE("BufferPoolManager::findOrCreateVertexArray: Initial allocations created for new VAO {0}.", vaoId);
        
        return vao;
    }

    void BufferPoolManager::calculateNewBufferPairSize(size_t vertexDataSize, size_t indexDataSize, size_t& vertexPoolSize, size_t& indexPoolSize) {

        if (vertexDataSize <= SMALL_BUFFER_POOL_SIZE * NEXT_BUFFER_SIZE_THRESHOLD) {
            vertexPoolSize = SMALL_BUFFER_POOL_SIZE;
        } else if (vertexDataSize <= MEDIUM_BUFFER_POOL_SIZE * NEXT_BUFFER_SIZE_THRESHOLD) {
            vertexPoolSize = MEDIUM_BUFFER_POOL_SIZE;
        } else if (vertexDataSize <= LARGE_BUFFER_POOL_SIZE * NEXT_BUFFER_SIZE_THRESHOLD) {
            vertexPoolSize = LARGE_BUFFER_POOL_SIZE;
        } else if (vertexDataSize <= HUGE_BUFFER_POOL_SIZE * NEXT_BUFFER_SIZE_THRESHOLD) {
            vertexPoolSize = HUGE_BUFFER_POOL_SIZE;
        } else {
             GE_CORE_WARN("BufferPoolManager::calculateNewBufferPairSize: Requested vertex size ({0} bytes) exceeds largest threshold. Sizing based on request.", vertexDataSize);
             vertexPoolSize = std::min(MAX_POOL_SIZE, (size_t)(vertexDataSize * 1.5));
        }

        if (vertexPoolSize < vertexDataSize) {
            GE_CORE_ERROR("BufferPoolManager::calculateNewBufferPairSize: Calculated vertex pool size ({0}) is smaller than vertexDataSize ({1})", vertexPoolSize, vertexDataSize);
            vertexPoolSize = 0;
            indexPoolSize = 0;
            return;
        }

        // can probavly be smaller 
        indexPoolSize = vertexPoolSize;
        if (indexPoolSize < indexDataSize) {
            GE_CORE_ERROR("BufferPoolManager::calculateNewBufferPairSize: Calculated index pool size ({0}) is smaller than indexDataSize ({1})", indexPoolSize, indexDataSize);
            vertexPoolSize = 0;
            indexPoolSize = 0;
            return;
        }
    }


} // namespace Rapture 