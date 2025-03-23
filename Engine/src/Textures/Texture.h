#pragma once

#include <string>
#include <memory>
#include <unordered_map>

namespace Rapture {

enum class TextureActiveSlot : uint32_t {
    ALBEDO=0,
    NORMAL=1,
    METALLIC=2,
    ROUGHNESS=3,
    AO=4,
    EMISSION=5,
    HEIGHT=6,
    SPECULAR=7,
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
};

class Texture2D : public Texture {
public:
    static std::shared_ptr<Texture2D> create(const std::string& path);
    static std::shared_ptr<Texture2D> create(uint32_t width, uint32_t height);
};

class TextureLibrary {
public:
    static void init();
    static void shutdown();
    
    static void add(const std::string& name, const std::shared_ptr<Texture2D>& texture);
    static void add(const std::shared_ptr<Texture2D>& texture);
    static std::shared_ptr<Texture2D> load(const std::string& filepath,const std::string& name="");
    static std::shared_ptr<Texture2D> get(const std::string& name);
    
private:
    static std::unordered_map<std::string, std::shared_ptr<Texture2D>> s_textures;
};

} // namespace Rapture 