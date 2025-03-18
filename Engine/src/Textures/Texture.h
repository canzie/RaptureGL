#pragma once

#include <string>
#include <memory>
#include <unordered_map>

namespace Rapture {

class Texture {
public:
    virtual ~Texture() = default;
    
    virtual uint32_t getWidth() const = 0;
    virtual uint32_t getHeight() const = 0;
    virtual uint32_t getRendererID() const = 0;
    
    virtual void bind(uint32_t slot = 0) const = 0;
    virtual void unbind() const = 0;
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
    static std::shared_ptr<Texture2D> load(const std::string& filepath);
    static std::shared_ptr<Texture2D> get(const std::string& name);
    
private:
    static std::unordered_map<std::string, std::shared_ptr<Texture2D>> s_textures;
};

} // namespace Rapture 