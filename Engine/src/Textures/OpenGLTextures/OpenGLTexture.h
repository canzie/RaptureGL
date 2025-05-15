#pragma once

#include "../Texture.h"
#include <glad/glad.h>

namespace Rapture {

class OpenGLTexture2D : public Texture2D {
public:
    OpenGLTexture2D(const std::string& path);
    OpenGLTexture2D(const std::vector<std::string>& filepaths);
    OpenGLTexture2D(uint32_t width, uint32_t height, uint32_t channels);
    OpenGLTexture2D(TextureSpecification specification);


    virtual ~OpenGLTexture2D() override;

    virtual uint32_t getWidth() const override { return m_width; }
    virtual uint32_t getHeight() const override { return m_height; }
    virtual uint32_t getDepth() const override { return m_depth; }

    virtual uint32_t getRendererID() const override { return m_rendererID; }

    virtual void bind(uint32_t slot = 0) const override;
    virtual void unbind() const override;

    virtual void bindCompute(uint32_t slot = 0) const override;

    // NOTE: dont know if this is needed so it is not a virtual function
    // might remove it later when i've used it more and know more about it
    virtual void unbindCompute() const override;

    virtual void setData(void* data, uint32_t size) override;

    virtual void barrier() const override;
    
    // Implement texture parameter setters
    virtual void setMinFilter(TextureFilter filter) override;
    virtual void setMagFilter(TextureFilter filter) override;
    virtual void setWrapS(TextureWrap wrap) override;
    virtual void setWrapT(TextureWrap wrap) override;

    // Bindless texture implementation
    virtual bool makeResident() override;
    virtual void makeNonResident() override;
    virtual bool isResident() const override { return m_isResident; }
    virtual uint64_t getTextureHandle() const override { return m_textureHandle; }

    virtual void clear(glm::vec4 color) override;
    
    // Static methods for bindless texture functionality
    static uint64_t generateTextureHandleFromID(uint32_t textureID);
    static bool makeTextureResident(uint64_t textureHandle);
    static void makeTextureNonResident(uint64_t textureHandle);

    // Static factory methods
    static std::shared_ptr<OpenGLTexture2D> createFromPath(const std::string& path);
    static std::shared_ptr<OpenGLTexture2D> createBlank(uint32_t width, uint32_t height, uint32_t channels);
    static std::shared_ptr<OpenGLTexture2D> createCubemap(const std::vector<std::string>& filepaths);

private:

    // Helper to generate texture handle
    void generateTextureHandle();
    GLenum getTextureTypeGL() const;

private:
    std::string m_path;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_depth = 0;
    uint32_t m_rendererID = 0;
    GLenum m_internalFormat = GL_RGBA8;
    GLenum m_dataFormat = GL_RGBA;

    mutable uint32_t m_activeSlot = 0;

    TextureType m_textureType = TextureType::TEXTURE2D;
    
    // Bindless texture data
    uint64_t m_textureHandle = 0;
    bool m_isResident = false;
};

} // namespace Rapture