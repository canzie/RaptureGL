#pragma once

#include "../Texture.h"
#include <glad/glad.h>

namespace Rapture {

class OpenGLTexture2D : public Texture2D {
public:
    OpenGLTexture2D(const std::string& path);
    OpenGLTexture2D(uint32_t width, uint32_t height, uint32_t channels);
    virtual ~OpenGLTexture2D() override;

    virtual uint32_t getWidth() const override { return m_width; }
    virtual uint32_t getHeight() const override { return m_height; }
    virtual uint32_t getRendererID() const override { return m_rendererID; }

    virtual void bind(uint32_t slot = 0) const override;
    virtual void unbind() const override;

    virtual void setData(void* data, uint32_t size) override;
    
    // Implement texture parameter setters
    virtual void setMinFilter(TextureFilter filter) override;
    virtual void setMagFilter(TextureFilter filter) override;
    virtual void setWrapS(TextureWrap wrap) override;
    virtual void setWrapT(TextureWrap wrap) override;

private:
    // Helper to convert enum to GL constant
    GLenum convertFilterToGL(TextureFilter filter);
    GLenum convertWrapToGL(TextureWrap wrap);

private:
    std::string m_path;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_rendererID = 0;
    GLenum m_internalFormat = GL_RGBA8;
    GLenum m_dataFormat = GL_RGBA;
};

} // namespace Rapture