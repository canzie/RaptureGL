#include "ModelLoader.h"
#include "../Logger/Log.h"
#include <chrono>
#include <random>
#include <sstream>
#include <algorithm>

namespace Rapture {

// Static instance of the singleton
ModelLoader& ModelLoader::getInstance() {
    static ModelLoader instance;
    return instance;
}

ModelLoader::ModelLoader() 
    : m_shuttingDown(false)
    , m_activeLoadCount(0)
    , m_initialized(false)
{
    GE_CORE_INFO("ModelLoader: Instance created");
}

ModelLoader::~ModelLoader() 
{
    if (m_initialized && !m_shuttingDown) {
        GE_CORE_WARN("ModelLoader: Destructing without explicit shutdown! Forcing shutdown...");
        shutdown();
    }
}

void ModelLoader::init(unsigned int numThreads)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    
    if (m_initialized) {
        GE_CORE_WARN("ModelLoader: Already initialized!");
        return;
    }
    
    if (m_shuttingDown) {
        GE_CORE_ERROR("ModelLoader: Cannot initialize while shutting down!");
        return;
    }
    
    GE_CORE_INFO("ModelLoader: Initializing with {0} worker threads", numThreads);
    
    // Ensure at least one thread
    numThreads = std::max(1u, numThreads);
    
    // Start worker threads
    for (unsigned int i = 0; i < numThreads; ++i) {
        m_workerThreads.emplace_back(&ModelLoader::workerFunction, this);
        GE_CORE_INFO("ModelLoader: Started worker thread {0}", i + 1);
    }
    
    m_initialized = true;
    GE_CORE_INFO("ModelLoader: Initialized successfully");
}

void ModelLoader::shutdown()
{
    bool wasInitialized = false;
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        wasInitialized = m_initialized;
        
        if (!m_initialized) {
            GE_CORE_WARN("ModelLoader: Not initialized, nothing to shut down!");
            return;
        }
        
        if (m_shuttingDown) {
            GE_CORE_WARN("ModelLoader: Already shutting down!");
            return;
        }
        
        GE_CORE_INFO("ModelLoader: Shutting down...");
        
        // Set initialized to false first to prevent new requests
        m_initialized = false;
        
        // Then signal threads to stop
        m_shuttingDown = true;
    }
    
    // Only proceed if we were initialized
    if (wasInitialized) {
        // Notify all waiting threads
        m_queueCondition.notify_all();
        
        // Log active operations
        size_t activeOps = m_activeLoadCount.load();
        if (activeOps > 0) {
            GE_CORE_WARN("ModelLoader: Shutting down with {0} active loading operations", activeOps);
        }
        
        // Join all worker threads
        for (auto& thread : m_workerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        // Clear containers
        {
            std::lock_guard<std::mutex> queueLock(m_queueMutex);
            std::lock_guard<std::mutex> statusLock(m_statusMutex);
            
            // Call callbacks for any remaining requests with failure
            while (!m_loadQueue.empty()) {
                auto request = m_loadQueue.front();
                m_loadQueue.pop();
                
                if (request.callback) {
                    GE_CORE_WARN("ModelLoader: Canceling queued model load '{0}' due to shutdown", request.path);
                    request.callback(false);
                }
            }
            
            m_workerThreads.clear();
            m_modelLoadStatus.clear();
            m_activeLoadCount = 0;
        }
        
        GE_CORE_INFO("ModelLoader: Shut down successfully");
    }
}

std::string ModelLoader::loadModel(
    const std::string& path, 
    std::shared_ptr<Scene> targetScene, 
    std::function<void(bool)> callback,
    bool isAbsolute)
{
    if (!m_initialized) {
        GE_CORE_ERROR("ModelLoader: Cannot load model, loader not initialized!");
        if (callback) {
            callback(false);
        }
        return "";
    }
    
    if (m_shuttingDown) {
        GE_CORE_ERROR("ModelLoader: Cannot load model, loader is shutting down!");
        if (callback) {
            callback(false);
        }
        return "";
    }
    
    if (!targetScene) {
        GE_CORE_ERROR("ModelLoader: Cannot load model, target scene is null!");
        if (callback) {
            callback(false);
        }
        return "";
    }
    
    // Generate a unique model ID
    std::string modelID = generateModelID(path);
    
    // Create a model load request
    ModelLoadRequest request;
    request.path = path;
    request.modelID = modelID;
    request.targetScene = targetScene;
    request.callback = callback;
    request.isAbsolute = isAbsolute;
    
    // Add the request to the queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        // Double-check state after acquiring lock
        if (!m_initialized || m_shuttingDown) {
            GE_CORE_ERROR("ModelLoader: Cannot queue model load, loader state changed!");
            if (callback) {
                callback(false);
            }
            return "";
        }
        
        m_loadQueue.push(request);
        
        // Add to tracking map
        std::lock_guard<std::mutex> statusLock(m_statusMutex);
        m_modelLoadStatus[modelID] = false;
    }
    
    GE_CORE_INFO("ModelLoader: Queued model '{0}' with ID '{1}'", path, modelID);
    
    // Notify a worker thread
    m_queueCondition.notify_one();
    
    return modelID;
}

bool ModelLoader::isModelLoaded(const std::string& modelID) const
{
    std::lock_guard<std::mutex> lock(m_statusMutex);
    auto it = m_modelLoadStatus.find(modelID);
    
    if (it != m_modelLoadStatus.end()) {
        return it->second;
    }
    
    return false;
}

size_t ModelLoader::getQueueSize() const
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_loadQueue.size();
}

size_t ModelLoader::getActiveLoadCount() const
{
    return m_activeLoadCount;
}

std::string ModelLoader::generateModelID(const std::string& path)
{
    // Create a unique ID based on path and a random component
    std::stringstream ss;
    
    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch().count();
    ss << value << "_";
    
    // Add random component to ensure uniqueness
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1000, 9999);
    ss << distrib(gen) << "_";
    
    // Add sanitized path
    std::string sanitizedPath = path;
    // Replace non-alphanumeric characters with underscores
    std::replace_if(sanitizedPath.begin(), sanitizedPath.end(), 
                  [](char c) { return !std::isalnum(c); }, '_');
    
    ss << sanitizedPath;
    
    return ss.str();
}

void ModelLoader::workerFunction()
{
    GE_CORE_INFO("ModelLoader: Worker thread started");
    
    while (!m_shuttingDown) {
        ModelLoadRequest request;
        bool hasRequest = false;
        
        // Get a request from the queue
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            // Wait for a request or shutdown signal
            m_queueCondition.wait(lock, [this] {
                return !m_loadQueue.empty() || m_shuttingDown;
            });
            
            // Check if we're shutting down
            if (m_shuttingDown) {
                break;
            }
            
            // Get the request
            if (!m_loadQueue.empty()) {
                request = m_loadQueue.front();
                m_loadQueue.pop();
                hasRequest = true;
                m_activeLoadCount++;
            }
        }
        
        // Process the request
        if (hasRequest) {
            GE_CORE_INFO("ModelLoader: Loading model '{0}' with ID '{1}'", request.path, request.modelID);
            
            bool success = false;
            
            try {
                // Create a glTF loader and load the model
                glTF2Loader loader(request.targetScene);
                success = loader.loadModel(request.path, request.isAbsolute);
                
                // Check if we're shutting down
                if (m_shuttingDown) {
                    // Abandoning process due to shutdown
                    GE_CORE_WARN("ModelLoader: Abandoning model '{0}' processing due to shutdown", request.path);
                    success = false;
                } else {
                    // Update the model load status
                    {
                        std::lock_guard<std::mutex> lock(m_statusMutex);
                        m_modelLoadStatus[request.modelID] = success;
                    }
                    
                    if (success) {
                        GE_CORE_INFO("ModelLoader: Successfully loaded model '{0}' with ID '{1}'", 
                                    request.path, request.modelID);
                    } else {
                        GE_CORE_ERROR("ModelLoader: Failed to load model '{0}' with ID '{1}'", 
                                     request.path, request.modelID);
                    }
                }
            }
            catch (const std::exception& e) {
                GE_CORE_ERROR("ModelLoader: Exception while loading model '{0}': {1}", 
                             request.path, e.what());
                success = false;
            }
            
            // Call the callback if provided
            if (request.callback) {
                request.callback(success);
            }
            
            m_activeLoadCount--;
        }
    }
    
    GE_CORE_INFO("ModelLoader: Worker thread stopped");
}

} // namespace Rapture 