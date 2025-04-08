#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <functional>
#include <filesystem>

#include "../Utils/GLCapabilities.h"

namespace Rapture {

enum class TextureActiveSlot : uint32_t {
    ALBEDO=0,
    CUBEMAP=0, // Cubemaps dont use other slots
    NORMAL=1, 
    METALLIC=2,
    MATERIAL=2, // used for gbuffer
    POSTITION=3, // used for gbuffer
    ROUGHNESS=3,
    AO=4,
    DEPTH=4,
    EMISSION=5,
    HEIGHT=6,
    SPECULAR=7,
    SHADOW=8
    
};

// SSBO binding points for various buffer types
enum class SSBOBindingPoint : uint32_t {
    MATERIAL_DATA = 0,
    TRANSFORM_DATA = 1,
    LIGHT_DATA = 2,
    PARTICLE_DATA = 3,
    INSTANCE_DATA = 4,
    MESH_DATA = 5,
    DRAW_COMMANDS = 6,
    TEXTURE_HANDLES = 7,
    // Add more as needed
};

// Texture filtering modes
enum class TextureFilter {
    Nearest,                // GL_NEAREST
    Linear,                 // GL_LINEAR
    NearestMipmapNearest,   // GL_NEAREST_MIPMAP_NEAREST
    LinearMipmapNearest,    // GL_LINEAR_MIPMAP_NEAREST
    NearestMipmapLinear,    // GL_NEAREST_MIPMAP_LINEAR
    LinearMipmapLinear      // GL_LINEAR_MIPMAP_LINEAR
};

// Texture wrapping modes
enum class TextureWrap {
    ClampToEdge,            // GL_CLAMP_TO_EDGE
    MirroredRepeat,         // GL_MIRRORED_REPEAT
    Repeat                  // GL_REPEAT
};


class Texture {
public:
    virtual ~Texture() = default;
    
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual uint32_t getRendererID() const = 0;
    
    virtual void bind(uint32_t slot = 0) const = 0;
    virtual void unbind() const = 0;
    
    // Texture parameter setters
    virtual void setMinFilter(TextureFilter filter) = 0;
    virtual void setMagFilter(TextureFilter filter) = 0;
    virtual void setWrapS(TextureWrap wrap) = 0;
    virtual void setWrapT(TextureWrap wrap) = 0;

    // Bindless texture methods
    virtual bool makeResident() = 0;
    virtual void makeNonResident() = 0;
    virtual bool isResident() const = 0;
    virtual uint64_t getTextureHandle() const = 0;
};

class Texture2D : public Texture {
public:
    virtual void setData(void* data, uint32_t size) = 0;

protected:
    static std::shared_ptr<Texture2D> create(const std::string& path);
    static std::shared_ptr<Texture2D> create(uint32_t width, uint32_t height, uint32_t channels);

    static std::shared_ptr<Texture2D> createCubemap(const std::vector<std::string>& filepaths);

    // Give TextureLibrary access to protected create methods
    friend class TextureLibrary;
};

struct TextureLoadRequest {
    std::string path;
    std::string name;
    std::vector<unsigned char> data;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::shared_ptr<Texture2D> texture;
    std::function<void(std::shared_ptr<Texture2D>)> callback = nullptr;
};

class TextureLibrary {
public:
    static void init(unsigned int numThreads = 4);
    static void shutdown();
    
    // Add a texture to the library
    static void add(const std::string& name, const std::shared_ptr<Texture2D>& texture);
    static void add(const std::shared_ptr<Texture2D>& texture);

    // Load a texture from a file
    static std::shared_ptr<Texture2D> load(const std::string& filepath);
    static std::shared_ptr<Texture2D> loadAsync(const std::string& filepath);
    static std::shared_ptr<Texture2D> loadAsync(const std::filesystem::path filepath);

    // Load a cubemap from a list of filepaths
    static std::shared_ptr<Texture2D> loadCubemap(const std::vector<std::filesystem::path>& filepaths);
    static std::shared_ptr<Texture2D> loadCubemap(const std::vector<std::string>& filepaths);
    static std::shared_ptr<Texture2D> loadCubemapAsync(const std::vector<std::string>& filepaths);


    // Get a texture from the library
    static std::shared_ptr<Texture2D> get(const std::string& name);
    
    static bool getTextureDimensions(const std::string& path, int& width, int& height, int& channels);

    // Multithreaded Operations
    static void shutdownWorkers();

    static void processLoadingQueue();

    // Bindless texture support - get handles for all textures
    static std::vector<uint64_t> getAllTextureHandles();
    static void makeAllTexturesResident();
    static void makeAllTexturesNonResident();
    

private:
    // Worker thread function for loading textures from disk
    static void textureLoadThread();

private:
    static std::unordered_map<std::string, std::shared_ptr<Texture2D>> s_textures;

        // Thread-safe queues
    static std::mutex s_queueMutex;
    static std::mutex s_completedMutex;
    static std::queue<TextureLoadRequest> s_pendingTextures;
    static std::queue<TextureLoadRequest> s_completedTextures;
    
        // Thread management
    static std::vector<std::thread> s_workerThreads;
    static std::atomic<bool> s_threadRunning;

};



} // namespace Rapture 