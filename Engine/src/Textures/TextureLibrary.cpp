#include "Texture.h"
#include "../Logger/Log.h"
#include "../Debug/Profiler.h"
#include <filesystem>
#include <stb_image.h>

namespace Rapture {

std::mutex TextureLibrary::s_queueMutex;
std::mutex TextureLibrary::s_completedMutex;
std::queue<TextureLoadRequest> TextureLibrary::s_pendingTextures;
std::queue<TextureLoadRequest> TextureLibrary::s_completedTextures;
std::vector<std::thread> TextureLibrary::s_workerThreads;
std::atomic<bool> TextureLibrary::s_threadRunning(false);


std::unordered_map<std::string, std::shared_ptr<Texture2D>> TextureLibrary::s_textures;

void TextureLibrary::init(unsigned int numThreads)
{
    RAPTURE_PROFILE_FUNCTION();
    GE_CORE_INFO("Initializing TextureLibrary");
    if (s_threadRunning) {
        GE_CORE_WARN("TextureLibrary already initialized");
        return;
    }

    s_threadRunning = true;

    unsigned int maxThreads = std::thread::hardware_concurrency();
    s_workerThreads.resize(std::min(numThreads, maxThreads));
    for (size_t i = 0; i < s_workerThreads.size(); ++i) {
        s_workerThreads[i] = std::thread(&TextureLibrary::textureLoadThread);
    }
}

void TextureLibrary::shutdown()
{
    RAPTURE_PROFILE_FUNCTION();
    GE_CORE_INFO("TextureLibrary: Beginning shutdown");
    
    // First, stop all worker threads
    shutdownWorkers();
    
    // Clear all queues to prevent any pending operations
    {
        std::lock_guard<std::mutex> lock(s_queueMutex);
        while (!s_pendingTextures.empty()) {
            s_pendingTextures.pop();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(s_completedMutex);
        while (!s_completedTextures.empty()) {
            s_completedTextures.pop();
        }
    }
    
    // Log texture count before clearing
    GE_CORE_INFO("TextureLibrary: Cleaning up {} textures", s_textures.size());
    
    // Clear texture cache - this releases all shared_ptr references
    s_textures.clear();
    
    GE_CORE_INFO("TextureLibrary: Shutdown complete");
}

void TextureLibrary::add(const std::string& name, const std::shared_ptr<Texture2D>& texture)
{
    RAPTURE_PROFILE_FUNCTION();
    
    if (s_textures.find(name) != s_textures.end()) {
        GE_CORE_WARN("Texture '{0}' already exists in the library, overwriting", name);
    }
    
    s_textures[name] = texture;
    GE_CORE_INFO("Added texture '{0}' to the library", name);
}

void TextureLibrary::add(const std::shared_ptr<Texture2D>& texture)
{
    // Use pointer address as a unique identifier if no name is provided
    // Check if a texture with the same renderer ID already exists
    uint32_t rendererID = texture->getRendererID();
    for (const auto& [existingName, existingTexture] : s_textures) {
        if (existingTexture->getRendererID() == rendererID) {
            GE_CORE_INFO("Texture with renderer ID {0} already exists as '{1}', skipping addition", rendererID, existingName);
            return;
        }
    }
    
    std::string name = "Texture_" + std::to_string(texture->getRendererID());
    add(name, texture);
}

std::shared_ptr<Texture2D> TextureLibrary::load(const std::string& filepath)
{
    RAPTURE_PROFILE_FUNCTION();
    
    // Use filepath as name, but cleanup to get just the filename
    std::string filename = std::filesystem::path(filepath).filename().string();
    
    // Check if already loaded
    auto it = s_textures.find(filename);
    if (it != s_textures.end()) {
        return it->second;
    }
    
    // Load the texture
    auto texture = Texture2D::create(filepath);
    if (texture) {
        add(filename, texture);
        return texture;
    }
    
    GE_CORE_ERROR("TextureLibrary: Failed to load texture '{0}'", filepath);
    return nullptr;
}

std::shared_ptr<Texture2D> TextureLibrary::loadAsync(const std::string &filepath)
{
    RAPTURE_PROFILE_FUNCTION();
    
    // Use filepath as name, but cleanup to get just the filename
    std::string filename = std::filesystem::path(filepath).filename().string();
    
    // Check if already loaded
    auto it = s_textures.find(filename);
    if (it != s_textures.end()) {
        return it->second;
    }

    // Get texture dimensions
    int width, height, channels;
    if (!getTextureDimensions(filepath, width, height, channels)) {
        GE_CORE_ERROR("TextureLibrary: Failed to get dimensions for '{0}'", filepath);
        return nullptr;
    }
    
   // Create texture with correct dimensions but no data yet
    auto texture = Texture2D::create(width, height, channels);
    if (!texture) {
        GE_CORE_ERROR("TextureLibrary: Failed to create texture for '{0}'", filepath);
        return nullptr;
    }
    
    // Add to library immediately with placeholder
    add(filename, texture);

    // Queue the texture for async loading
    TextureLoadRequest request;
    request.path = filepath;
    request.name = filename;
    request.width = width;
    request.height = height;
    request.channels = channels;
    request.texture = texture;

    // Add to pending queue
    {
        std::lock_guard<std::mutex> lock(s_queueMutex);
        s_pendingTextures.push(request);
    }
    
    // Start worker thread if not already running
    if (!s_threadRunning) {
        GE_CORE_ERROR("TextureLibrary: Thread not running, failed to load texture '{0}'", filepath);
        return nullptr;
    }

    s_workerThreads.emplace_back(&TextureLibrary::textureLoadThread);

    
    return texture;
}

std::shared_ptr<Texture2D> TextureLibrary::get(const std::string& name)
{
    RAPTURE_PROFILE_FUNCTION();
    
    auto it = s_textures.find(name);
    if (it != s_textures.end()) {
        return it->second;
    }
    
    GE_CORE_WARN("TextureLibrary: Texture '{0}' not found", name);
    return nullptr;
}

void TextureLibrary::shutdownWorkers()
{
    RAPTURE_PROFILE_FUNCTION();
    GE_CORE_INFO("TextureLibrary: Shutting down worker threads");
    
    // Set the flag to false to signal threads to stop
    if (s_threadRunning.exchange(false)) {
        // Wait for the thread to join if joinable
        if (s_workerThreads.size() > 0 && s_workerThreads[0].joinable()) {
            GE_CORE_INFO("TextureLibrary: Waiting for worker thread to join");
            for (auto& thread : s_workerThreads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
            s_workerThreads.clear();
            GE_CORE_INFO("TextureLibrary: Worker threads joined successfully");
        } else {
            GE_CORE_WARN("TextureLibrary: Worker threads not joinable");
        }
    } else {
        GE_CORE_INFO("TextureLibrary: Worker threads already stopped");
    }
}

void TextureLibrary::processLoadingQueue()
{
    std::vector<TextureLoadRequest> completedRequests;

    {
        std::lock_guard<std::mutex> lock(s_completedMutex);
        while (!s_completedTextures.empty()) {
            completedRequests.push_back(s_completedTextures.front());
            s_completedTextures.pop();
        }
    }

    for (auto& request : completedRequests) {
        if (request.texture && !request.data.empty()) {
            request.texture->setData(request.data.data(), request.data.size());
        }

        if (request.callback) {
            request.callback(request.texture);
        }
    }
}

void TextureLibrary::textureLoadThread()
{
    GE_CORE_INFO("TextureLibrary: Texture loading thread started");
    
    while (s_threadRunning) {
        TextureLoadRequest request;
        bool hasRequest = false;
        
        // Check if shutdown was requested before accessing the mutex
        if (!s_threadRunning) break;
        
        // Get a request from the queue
        {
            std::lock_guard<std::mutex> lock(s_queueMutex);
            if (!s_pendingTextures.empty()) {
                request = s_pendingTextures.front();
                s_pendingTextures.pop();
                hasRequest = true;
            }
        }
        
        // Check if shutdown was requested after acquiring a request
        if (!s_threadRunning) break;
        
        if (hasRequest) {
            // Load image data but check shutdown flag regularly
            stbi_set_flip_vertically_on_load(0);
            unsigned char* data = stbi_load(request.path.c_str(), &request.width, &request.height, &request.channels, 0);
            
            // Check if shutdown was requested after loading
            if (!s_threadRunning) {
                if (data) stbi_image_free(data);
                break;
            }
            
            if (data) {
                // Store the data in the request for later processing on main thread
                size_t dataSize = request.width * request.height * request.channels;
                request.data.resize(dataSize);
                std::memcpy(request.data.data(), data, dataSize);
                
                // Free stbi data
                stbi_image_free(data);
                
                // Queue for GPU upload on main thread
                std::lock_guard<std::mutex> lock(s_completedMutex);
                s_completedTextures.push(request);

            } else {
                GE_CORE_ERROR("TextureLibrary: Failed to load texture data '{0}'", request.path);
            }
        } else {
            // No work to do, sleep to avoid busy waiting
            // Use shorter sleep durations to respond to shutdown quicker
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    
    GE_CORE_INFO("TextureLibrary: Texture loading thread stopped");
}

bool TextureLibrary::getTextureDimensions(const std::string &path, int &width, int &height, int &channels)
{
    // Use stbi_info to only read the header without decoding image data
    if (stbi_info(path.c_str(), &width, &height, &channels)) {
        return true;
    }
    
    GE_CORE_ERROR("Failed to read texture dimensions from '{}'", path);
    width = height = channels = 0;
    return false;
}

} // namespace Rapture 