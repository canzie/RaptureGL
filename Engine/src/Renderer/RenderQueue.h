#pragma once

#include <vector>
#include <algorithm>
#include "RenderCommands.h"
#include <string>
#include <queue>
#include <variant>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <thread>
#include "../Debug/TracyProfiler.h"

namespace Rapture {

    enum class RenderQueueType {
        GEOMETRY,
        POSTPROCESS,
        SHADOWMAP
    };

    //using CommandVariant = std::variant<RenderCommand, PostProcessCommand>;
    using CommandVariant = std::variant<std::monostate, RenderCommand, PostProcessCommand, AnimationSetupCommand>;

    class RenderQueue {
    public:
        RenderQueue(const std::string& name, RenderQueueType type) 
            : m_name(name), m_type(type), m_isDone(false) {}

        // Delete copy constructor and assignment operator
        RenderQueue(const RenderQueue&) = delete;
        RenderQueue& operator=(const RenderQueue&) = delete;

        // Add move constructor and move assignment operator
        RenderQueue(RenderQueue&& other) noexcept
            : m_name(std::move(other.m_name)),
              m_type(other.m_type),
              m_isDone(other.m_isDone)
        {
            // Move queue contents
            while (!other.m_queue.empty()) {
                m_queue.push(other.m_queue.front());
                other.m_queue.pop();
            }
        }
        
        RenderQueue& operator=(RenderQueue&& other) noexcept {
            if (this != &other) {
                // Clear current queue
                while (!m_queue.empty()) {
                    m_queue.pop();
                }
                
                // Move from other queue
                while (!other.m_queue.empty()) {
                    m_queue.push(other.m_queue.front());
                    other.m_queue.pop();
                }
                
                // Move other members
                const_cast<std::string&>(m_name) = std::move(const_cast<std::string&>(other.m_name));
                const_cast<RenderQueueType&>(m_type) = other.m_type;
                m_isDone = other.m_isDone;
            }
            return *this;
        }

        // New method: Signal that the queue is done receiving new elements
        void markAsDone() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_isDone = true;
            }
            m_cv.notify_all(); // Wake up any waiting threads
        }

        // New method: Check if the queue is done
        bool isDone() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_isDone && m_queue.empty();
        }

        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            while (!m_queue.empty()) {
                m_queue.pop();
            }
            m_isDone = false;
        }
        
        void add(const CommandVariant& cmd) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(cmd);
            m_cv.notify_one(); // Notify waiting consumers
        }
        
        void sort() {
            GE_RENDER_ERROR("Sorting not implemented for RenderQueue");
        }
        
        // Now returns a copy of the front element and removes it
        CommandVariant serve() {
            RAPTURE_PROFILE_FUNCTION();
            std::unique_lock<std::mutex> lock(m_mutex);
            
            // Wait until the queue is not empty or it's done
            m_cv.wait(lock, [this]() {
                return !m_queue.empty() || m_isDone;
            });
            
            if (m_queue.empty()) {
                return std::monostate();
            }

            CommandVariant cmd = m_queue.front();
            m_queue.pop();
            return cmd;
        }

        // Try to get an element, returns immediately if queue is empty
        bool tryServe(CommandVariant& outCmd) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty()) {
                return false;
            }
            
            outCmd = m_queue.front();
            m_queue.pop();
            return true;
        }

        const CommandVariant& peek() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.front();
        }

        bool empty() const {
            RAPTURE_PROFILE_FUNCTION();
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }


    public:
        const std::string m_name;
        const RenderQueueType m_type;

    private:
        // Commands to render
        std::queue<CommandVariant> m_queue;
        
        // Thread synchronization
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        bool m_isDone;
    };

    // Structure for queue build requests
    struct QueueBuildRequest {
        std::shared_ptr<Scene> scene;
        std::shared_ptr<RenderQueue> resultQueue;
    };

    // Class which takes in a scene and returns a Queue of commands
    class CommandQueueBuilder {
    public:
        // Initialize with specified number of worker threads
        static void init(unsigned int numThreads = 2);
        
        // Shutdown and join all worker threads
        static void shutdownWorkers();
        
        // Check if the system is initialized
        static bool isInitialized() { return s_initialized; }

        // Synchronous queue building
        static RenderQueue buildGeometryCommandQueue(const std::shared_ptr<Scene>& scene);
        static RenderQueue buildPostProcessCommandQueue(const std::shared_ptr<Scene>& scene);
        
        // Asynchronous queue building
        static std::shared_ptr<RenderQueue> buildGeometryCommandQueueAsync(const std::shared_ptr<Scene>& scene);
        
        // Process any completed queues (called from main thread)
        static void processCompletedQueues();

    private:
        // Thread pool management
        static std::atomic_bool s_initialized;
        static std::atomic_bool s_shuttingDown;
        static std::vector<std::thread> s_workerThreads;
        
        // Work queue management
        static std::mutex s_queueMutex;
        static std::condition_variable s_queueCV;
        static std::queue<QueueBuildRequest> s_pendingBuilds;
        
        // Worker thread function
        static void queueBuilderThread();
        
        // Build implementation used by both sync and async paths
        static void buildGeometryQueue(const QueueBuildRequest& request);
    };
}
