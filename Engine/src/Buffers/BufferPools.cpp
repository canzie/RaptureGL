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

        // allocate the vertex data
        auto vertexAllocation = allocateBuffer(vao, BufferType::Vertex, vertexDataSize);
        // allocate the index data
        auto indexAllocation = allocateBuffer(vao, BufferType::Index, indexDataSize);


        if (!vertexAllocation || !indexAllocation) {
            GE_CORE_ERROR("Failed to allocate vertex or index buffer");
            return meshData;
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
            if (allocation->bufferType == type && allocation->sizeBytes >= size) {
                if (allocation->isAllocated) {
                    continue;
                }
                //allocation->print();
                // there is space and the allocation is not in use
                allocation->isAllocated = true;
                splitBufferPoolAllocation(allocation, size, vao);
                return allocation;
            }
        }

        GE_CORE_ERROR("BufferPoolManager::allocateBuffer: Failed to allocate buffer");
        return nullptr;
    }

    void BufferPoolManager::splitBufferPoolAllocation(std::shared_ptr<BufferAllocation> allocation, size_t size, std::shared_ptr<VertexArray> vao) {
        if (!vao) {
            GE_CORE_ERROR("BufferPoolManager::splitBufferPoolAllocation: Attempted to split allocation for null VAO");
            return;
        }

        unsigned int vaoId = vao->getID();
        if (vaoId == 0) {
            GE_CORE_ERROR("BufferPoolManager::splitBufferPoolAllocation: Attempted to split allocation for VAO with invalid ID: {0}", vaoId);
            return;
        }


        // if this value is smaller then 0, it would make a useless allocation, with a potentially buffed offset
        if (allocation->sizeBytes - size > 0) {
            auto newAllocation = std::make_shared<BufferAllocation>(
                allocation->offsetBytes + size,
                allocation->sizeBytes - size, 
                false, 
                allocation->bufferType, 
                allocation->bufferUsage);
            //newAllocation->print();

            m_vaoToBufferAllocationsMap[vaoId].push_back(newAllocation);

            // update the old allocation size
            allocation->sizeBytes = size;
        }
    }

    void BufferPoolManager::freeMeshData(MeshBufferData& meshData) {
        std::lock_guard<std::mutex> lock(m_mutex);

        meshData.vertexAllocation->isAllocated = false;
        meshData.indexAllocation->isAllocated = false;
    }

    // guarantees a usable VAO
    std::shared_ptr<VertexArray> BufferPoolManager::findOrCreateVertexArray(const BufferLayout& layout, size_t vertexDataSize, size_t indexDataSize, unsigned int indexType) {
        // Hash the layout to use as a key
        size_t layoutHash = layout.hash();
        
        // Check if we already have a VAO for this layout
        auto it = m_layoutToVAOMap.find(layoutHash);
        if (it != m_layoutToVAOMap.end() && it->second) {
            auto vao = it->second;
            unsigned int vaoId = vao->getID();
            if (vaoId == 0) {
                GE_CORE_ERROR("BufferPoolManager: Found VAO with invalid ID in layout map");
                return nullptr;
            }

            auto bufferAllocationsIt = m_vaoToBufferAllocationsMap.find(vaoId);
            if (bufferAllocationsIt != m_vaoToBufferAllocationsMap.end() && !bufferAllocationsIt->second.empty()) {
                bool hasVBO = false;
                bool hasIBO = false;
                
                for (auto& bufferAllocation : bufferAllocationsIt->second) {
                    if (!hasVBO && 
                        bufferAllocation->bufferType == BufferType::Vertex && 
                        bufferAllocation->sizeBytes >= vertexDataSize) {
                        hasVBO = true;
                    } 
                    else if (!hasIBO && 
                        bufferAllocation->bufferType == BufferType::Index && 
                        bufferAllocation->sizeBytes >= indexDataSize) {
                        hasIBO = true;
                    }

                    if (hasVBO && hasIBO) {
                        return vao;
                    }
                }
            }
        }

        // Create a new VAO for this layout
        auto vao = std::make_shared<VertexArray>();
        if (!vao) {
            GE_CORE_ERROR("BufferPoolManager: Failed to create new VAO");
            return nullptr;
        }

        unsigned int vaoId = vao->getID();
        if (vaoId == 0) {
            GE_CORE_ERROR("BufferPoolManager: Newly created VAO has invalid ID");
            return nullptr;
        }

        size_t vertexPoolSize;
        size_t indexPoolSize;
        calculateNewBufferPairSize(vertexDataSize, indexDataSize, vertexPoolSize, indexPoolSize);

        GE_CORE_INFO("BufferPoolManager: Creating new VAO with vertex pool size: {0}MB and index pool size: {1}MB", vertexPoolSize/1024.0f/1024.0f, indexPoolSize/1024.0f/1024.0f);

        auto vertexBuffer = std::make_shared<VertexBuffer>(vertexPoolSize);
        auto indexBuffer = std::make_shared<IndexBuffer>(indexPoolSize, indexType);

        vao->setVertexBuffer(vertexBuffer);
        vao->setIndexBuffer(indexBuffer);
        vao->setBufferLayout(layout);
        
        // Store it in the maps
        m_layoutToVAOMap[layout.hash()] = vao;
        m_vaoToBufferAllocationsMap[vaoId].push_back(std::make_shared<BufferAllocation>(0, vertexPoolSize, false, BufferType::Vertex, BufferUsage::Static));
        m_vaoToBufferAllocationsMap[vaoId].push_back(std::make_shared<BufferAllocation>(0, indexPoolSize, false, BufferType::Index, BufferUsage::Static));
        
        return vao;
    }

    void BufferPoolManager::calculateNewBufferPairSize(size_t vertexDataSize, size_t indexDataSize, size_t& vertexPoolSize, size_t& indexPoolSize) {


        // calculate the size of the vertex buffer pool
        vertexPoolSize = SMALL_BUFFER_POOL_SIZE;
        
        // Check if we need to use a larger pool size based on vertex data size
        if (vertexDataSize > SMALL_BUFFER_POOL_SIZE * NEXT_BUFFER_SIZE_THRESHOLD) {
            vertexPoolSize = MEDIUM_BUFFER_POOL_SIZE;
            
            if (vertexDataSize > MEDIUM_BUFFER_POOL_SIZE * NEXT_BUFFER_SIZE_THRESHOLD) {
                vertexPoolSize = LARGE_BUFFER_POOL_SIZE;
                
                if (vertexDataSize > LARGE_BUFFER_POOL_SIZE * NEXT_BUFFER_SIZE_THRESHOLD) {
                    vertexPoolSize = HUGE_BUFFER_POOL_SIZE;
                }
            }
        }

        if (vertexPoolSize < vertexDataSize) {
            GE_CORE_ERROR("BufferPoolManager::allocateMeshData: Vertex data size({0}MB) exceeds maximum threshold of {1}MB", vertexDataSize/1024.0f/1024.0f, vertexPoolSize/1024.0f/1024.0f);
            indexPoolSize=0;
            vertexPoolSize=0;
            return;
        }

        if (vertexPoolSize < indexDataSize) {
            indexPoolSize = std::min(vertexPoolSize*2, MAX_POOL_SIZE);
            return;
        }

        // calculate the size of the index buffer pool
        indexPoolSize = vertexPoolSize;

    }


} // namespace Rapture 