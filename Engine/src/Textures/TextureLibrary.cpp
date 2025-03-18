#include "Texture.h"
#include "../Logger/Log.h"
#include <filesystem>

namespace Rapture {

std::unordered_map<std::string, std::shared_ptr<Texture2D>> TextureLibrary::s_textures;

void TextureLibrary::init()
{
    GE_CORE_INFO("Initializing TextureLibrary");
}

void TextureLibrary::shutdown()
{
    GE_CORE_INFO("Shutting down TextureLibrary");
    s_textures.clear();
}

void TextureLibrary::add(const std::string& name, const std::shared_ptr<Texture2D>& texture)
{
    if (s_textures.find(name) != s_textures.end()) {
        GE_CORE_WARN("Texture '{0}' already exists in the library, overwriting", name);
    }
    
    s_textures[name] = texture;
    GE_CORE_INFO("Added texture '{0}' to the library", name);
}

void TextureLibrary::add(const std::shared_ptr<Texture2D>& texture)
{
    // Use pointer address as a unique identifier if no name is provided
    std::string name = "Texture_" + std::to_string(reinterpret_cast<uintptr_t>(texture.get()));
    add(name, texture);
}

std::shared_ptr<Texture2D> TextureLibrary::load(const std::string& filepath)
{
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

std::shared_ptr<Texture2D> TextureLibrary::get(const std::string& name)
{
    auto it = s_textures.find(name);
    if (it != s_textures.end()) {
        return it->second;
    }
    
    GE_CORE_WARN("TextureLibrary: Texture '{0}' not found", name);
    return nullptr;
}

} // namespace Rapture 