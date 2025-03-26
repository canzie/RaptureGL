#pragma once

#include <vector>
#include <array>
#include <mutex>
#include <map>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <condition_variable>

#include "../logger/Log.h"
#include "Buffers.h"
#include "VertexArray.h"  // Make sure this is included before using VertexArray
#include "OpenGLBuffers/VertexBuffers/OpenGLVertexBuffer.h"
#include "OpenGLBuffers/IndexBuffers/OpenGLIndexBuffer.h"

namespace Rapture {

    // Constants for buffer pool configuration
    constexpr size_t MIN_POOL_SIZE = 4 * 1024 * 1024;      // 4 MB minimum pool size
    constexpr size_t MAX_POOL_SIZE = 64 * 1024 * 1024;     // 64 MB maximum pool size
    constexpr size_t DEFAULT_ALIGNMENT = 16;               // Default memory alignment
    constexpr size_t MAX_BUFFER_POOLS = 8;                 // Maximum number of buffer pools
    
    constexpr size_t SMALL_BUFFER_POOL_SIZE = 64 * 1024 * 1024; // 4 MB small buffer pool size
    constexpr size_t MEDIUM_BUFFER_POOL_SIZE = 16 * 1024 * 1024; // 16 MB medium buffer pool size
    constexpr size_t LARGE_BUFFER_POOL_SIZE = 32 * 1024 * 1024; // 32 MB large buffer pool size
    constexpr size_t HUGE_BUFFER_POOL_SIZE = 64 * 1024 * 1024; // 64 MB huge buffer pool size

    constexpr float NEXT_BUFFER_SIZE_THRESHOLD = 0.15f; // 15% threshold for increasing buffer size


    // Struct to represent an allocation within a buffer pool
    // TODOl should probably be split in 2 classes, becaus buffertype and usage will be duplicated a lot
    struct BufferAllocation {
        size_t offsetBytes;        // Offset from the start of the buffer in bytes
        size_t sizeBytes;          // Size of the allocation in bytes
        BufferType bufferType;     // Type of buffer (VBO, IBO, etc.)
        BufferUsage bufferUsage;   // Usage of the buffer (STATIC, DYNAMIC, etc.)
        bool isAllocated;          // Whether this allocation is currently in use
        
        BufferAllocation() = default;
        BufferAllocation(size_t offset, size_t size, bool allocated = false, BufferType type = BufferType::Vertex, BufferUsage usage = BufferUsage::Static)
            : offsetBytes(offset), sizeBytes(size), isAllocated(allocated), bufferType(type), bufferUsage(usage) {}

        void print() const {
            GE_CORE_INFO("BufferAllocation: {0} bytes allocated at offset {1} for {2}, which is {3}", sizeBytes, offsetBytes, bufferType == BufferType::Vertex ? "Vertex" : "Index", isAllocated ? "allocated" : "not allocated");
        }

    };

    // Struct to store mesh buffer data references
    struct MeshBufferData {
        std::shared_ptr<VertexArray> vao;     // Reference to the VAO
        std::shared_ptr<BufferAllocation> vertexAllocation;
        std::shared_ptr<BufferAllocation> indexAllocation;
        size_t indexCount;
        unsigned int indexType;               // GL_UNSIGNED_SHORT, GL_UNSIGNED_INT, etc.
        size_t vertexOffsetInVertices;

        MeshBufferData(
            std::shared_ptr<BufferAllocation> vertexAllocation=nullptr, 
            std::shared_ptr<BufferAllocation> indexAllocation=nullptr, 
            unsigned int indexType=0, 
            size_t indexCount=0,
            size_t vertexOffsetInVertices=0,

            std::shared_ptr<VertexArray> vao=nullptr)
            : vertexAllocation(vertexAllocation), 
              indexAllocation(indexAllocation), 
              indexType(indexType),
              indexCount(indexCount),
              vertexOffsetInVertices(vertexOffsetInVertices),
              vao(vao) {}

        void print() const {
            GE_CORE_TRACE("========== MeshBufferData: VAO: {0} ==========", vao->getID());
            vertexAllocation->print();
            indexAllocation->print();
            GE_CORE_INFO("MeshBufferData: Index Count: {0}, Index Type: {1}", indexCount, indexType);
        }
    };

    // Manager class for all buffer pools
    class BufferPoolManager {
    public:
        BufferPoolManager();
        ~BufferPoolManager();
        
        // Initialize the buffer pool manager
        static void init();
        
        // Shutdown and cleanup
        static void shutdown();
        
        // Get the singleton instance
        static BufferPoolManager& getInstance();

        void printBufferAllocations() {
            for (auto& [i, vao] : m_layoutToVAOMap) {
                GE_CORE_INFO("BufferPoolManager:: vao: {0}", vao->getID());
                vao->getBufferLayout().print();
                GE_CORE_INFO("BufferPoolManager:: buffrlayour hash: {0}", vao->getBufferLayout().hash());
                auto& allocations = m_vaoToBufferAllocationsMap[vao->getID()];
                for (auto& allocation : allocations) {
                    allocation->print();
                }
            }
        }
        
        // Allocate mesh data in buffer pools
        MeshBufferData allocateMeshData(
            const BufferLayout& layout,
            const void* vertexData, size_t vertexDataSize,
            const void* indexData, size_t indexDataSize, size_t indexCount,
            unsigned int indexType
        );
        
        // Free mesh data from buffer pools
        void freeMeshData(MeshBufferData& meshData);
        
    private:
        // Prevent copying
        BufferPoolManager(const BufferPoolManager&) = delete;
        BufferPoolManager& operator=(const BufferPoolManager&) = delete;
        
        // Find or create a vertex array for a specific buffer layout
        std::shared_ptr<VertexArray> findOrCreateVertexArray(const BufferLayout& layout, size_t vertexDataSize, size_t indexDataSize, unsigned int indexType);
        
        std::shared_ptr<BufferAllocation> allocateBuffer(std::shared_ptr<VertexArray> vao, BufferType type, size_t size);
        void splitBufferPoolAllocation(std::shared_ptr<BufferAllocation> allocation, size_t size, std::shared_ptr<VertexArray> vao);
        
        // Calculate the maximum index size bytes for a given buffer layout
        void calculateNewBufferPairSize(size_t vertexDataSize, size_t indexDataSize, size_t& vertexPoolSize, size_t& indexPoolSize);
    
    private:
        static std::unique_ptr<BufferPoolManager> s_instance;
        static std::once_flag s_initInstanceFlag;
        
        std::mutex m_mutex;

        // potential issue in the future is that when the buffers inside of a vao are full
        // a new vao will be created, and the old one will be overwritten, while we would still like access because space can be cleared
        // however, with a large enogh initial buffer, we can avoid this case for now, until the system works
        std::unordered_map<size_t, std::shared_ptr<VertexArray>> m_layoutToVAOMap;

        std::unordered_map<unsigned int, std::vector<std::shared_ptr<BufferAllocation>>> m_vaoToBufferAllocationsMap;
    };
}