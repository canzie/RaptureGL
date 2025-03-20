#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <utility>
#include <unordered_map>
#include <memory>
#include <string>
#include <queue>
#include <functional>

#include "../../logger/Log.h"
#include "../../DataTypes.h"
#include "../Buffers.h"

namespace Rapture {

    struct BufferPoolSlot {
        std::shared_ptr<Buffer> buffer;
        size_t size;
        BufferType type;
        BufferUsage usage;
        bool inUse;
        uint64_t lastUsedFrame;
    };

    class BufferPool {
    public:
        static BufferPool& getInstance() {
            static BufferPool instance;
            return instance;
        }

        // Acquire buffer from pool or create a new one
        std::shared_ptr<Buffer> acquireBuffer(BufferType type, size_t size, BufferUsage usage = BufferUsage::Static) {
            // Try to find a suitable buffer in the pool
            std::string key = generateKey(type, size, usage);
            auto it = m_availableBuffers.find(key);
            
            if (it != m_availableBuffers.end() && !it->second.empty()) {
                // Found an available buffer of the right type/size/usage
                size_t index = it->second.front();
                it->second.pop();
                
                BufferPoolSlot& slot = m_bufferSlots[index];
                slot.inUse = true;
                slot.lastUsedFrame = m_currentFrame;
                
                return slot.buffer;
            }
            
            // No suitable buffer found, create a new one
            std::shared_ptr<Buffer> newBuffer = Buffer::Create(type, size, usage);
            if (!newBuffer) {
                GE_CORE_ERROR("Failed to create buffer of type {0} and size {1}", (int)type, size);
                return nullptr;
            }
            
            // Store in pool for future reuse
            size_t newIndex = m_bufferSlots.size();
            m_bufferSlots.push_back({newBuffer, size, type, usage, true, m_currentFrame});
            
            return newBuffer;
        }
        
        // Return a buffer to the pool for reuse
        void releaseBuffer(std::shared_ptr<Buffer> buffer) {
            // Find the buffer in our slots
            for (size_t i = 0; i < m_bufferSlots.size(); i++) {
                if (m_bufferSlots[i].buffer == buffer && m_bufferSlots[i].inUse) {
                    m_bufferSlots[i].inUse = false;
                    
                    // Add to available buffers
                    std::string key = generateKey(m_bufferSlots[i].type, m_bufferSlots[i].size, m_bufferSlots[i].usage);
                    m_availableBuffers[key].push(i);
                    return;
                }
            }
            
            // Buffer not found in pool - might be externally created
            GE_CORE_WARN("Attempted to release a buffer not managed by the pool");
        }
        
        // Update frame counter - call once per frame
        void nextFrame() {
            m_currentFrame++;
            
            // Optional: Garbage collection for unused buffers after a certain period
            if (m_currentFrame % 100 == 0) {
                collectGarbage();
            }
        }
        
        // Clean up unused buffers that haven't been used for a while
        void collectGarbage(uint64_t maxUnusedFrames = 300) {
            std::vector<size_t> buffersToRemove;
            
            // Find buffers that haven't been used in a while
            for (size_t i = 0; i < m_bufferSlots.size(); i++) {
                if (!m_bufferSlots[i].inUse && (m_currentFrame - m_bufferSlots[i].lastUsedFrame) > maxUnusedFrames) {
                    buffersToRemove.push_back(i);
                }
            }
            
            // Remove them from available queues
            for (size_t index : buffersToRemove) {
                const BufferPoolSlot& slot = m_bufferSlots[index];
                std::string key = generateKey(slot.type, slot.size, slot.usage);
                
                // Create a new queue without the buffer to remove
                std::queue<size_t> newQueue;
                std::queue<size_t>& oldQueue = m_availableBuffers[key];
                
                while (!oldQueue.empty()) {
                    size_t queuedIndex = oldQueue.front();
                    oldQueue.pop();
                    
                    if (queuedIndex != index) {
                        newQueue.push(queuedIndex);
                    }
                }
                
                m_availableBuffers[key] = newQueue;
            }
            
            // Actual removal would require more complex data structure
            // For now, just log how many buffers would be removed
            if (!buffersToRemove.empty()) {
                GE_CORE_INFO("Buffer garbage collection would remove {0} buffers", buffersToRemove.size());
            }
        }
        
    private:
        BufferPool() : m_currentFrame(0) {}
        ~BufferPool() = default;
        
        BufferPool(const BufferPool&) = delete;
        BufferPool& operator=(const BufferPool&) = delete;
        
        // Generate a unique key for buffer lookup
        std::string generateKey(BufferType type, size_t size, BufferUsage usage) {
            // Round size up to nearest power of 2 or block size for better reuse
            size_t alignedSize = alignSize(size);
            return std::to_string((int)type) + "_" + std::to_string(alignedSize) + "_" + std::to_string((int)usage);
        }
        
        // Align buffer size for better reuse (e.g., to powers of 2 or fixed block sizes)
        size_t alignSize(size_t size) {
            // Simple power-of-2 alignment
            size_t aligned = 1;
            while (aligned < size) {
                aligned *= 2;
            }
            return aligned;
        }
        
        std::vector<BufferPoolSlot> m_bufferSlots;
        std::unordered_map<std::string, std::queue<size_t>> m_availableBuffers;
        uint64_t m_currentFrame;
    };

}