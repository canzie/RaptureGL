#pragma once

#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <unordered_map>
#include "../Scenes/Scene.h"
#include "glTF/glTF2Loader.h"

namespace Rapture {

    // Struct to hold model loading request data
    struct ModelLoadRequest {
        std::string path;
        std::string modelID;
        std::shared_ptr<Scene> targetScene;
        std::function<void(bool)> callback;
        bool isAbsolute;
    };

    class ModelLoader {
    public:
        // Delete copy constructor and assignment operator
        ModelLoader(const ModelLoader&) = delete;
        ModelLoader& operator=(const ModelLoader&) = delete;

        // Get the singleton instance
        static ModelLoader& getInstance();

        // Initialize the loader with a specific number of worker threads
        void init(unsigned int numThreads = 1);
        
        // Shutdown the loader and stop all threads
        void shutdown();

        // Queue a model to be loaded asynchronously
        // Returns a unique ID for the model being loaded
        std::string loadModel(const std::string& path, 
                           std::shared_ptr<Scene> targetScene, 
                           std::function<void(bool)> callback = nullptr,
                           bool isAbsolute = false);
        
        // Check if a specific model is loaded
        bool isModelLoaded(const std::string& modelID) const;
        
        // Get the number of models currently in the loading queue
        size_t getQueueSize() const;
        
        // Get the number of models currently being processed
        size_t getActiveLoadCount() const;
        
        // Check if the loader is initialized
        bool isInitialized() const { return m_initialized; }
        
        // Check if the loader is shutting down
        bool isShuttingDown() const { return m_shuttingDown; }

    private:
        // Private constructor for singleton
        ModelLoader();
        
        // Private destructor
        ~ModelLoader();
        
        // Worker thread function
        void workerFunction();
        
        // Generate a unique ID for a model
        std::string generateModelID(const std::string& path);

    private:
        // Queue of model loading requests
        std::queue<ModelLoadRequest> m_loadQueue;
        
        // Mutex for thread-safe queue operations
        mutable std::mutex m_queueMutex;
        
        // Condition variable for worker threads
        std::condition_variable m_queueCondition;
        
        // Worker threads
        std::vector<std::thread> m_workerThreads;
        
        // Flag to signal worker threads to stop
        std::atomic<bool> m_shuttingDown;
        
        // Map to track loading status of models
        mutable std::mutex m_statusMutex;
        std::unordered_map<std::string, bool> m_modelLoadStatus;
        
        // Count of currently active loading operations
        std::atomic<size_t> m_activeLoadCount;
        
        // Flag to indicate if the loader has been initialized
        std::atomic<bool> m_initialized;
    };

}
