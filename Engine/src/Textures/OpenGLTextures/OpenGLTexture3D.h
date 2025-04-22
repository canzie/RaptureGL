#pragma once

#include "../Texture.h"
#include <glad/glad.h>

namespace Rapture {

class OpenGLTexture3D : public Texture3D {
public:

    OpenGLTexture3D(TextureSpecification specification);


    virtual ~OpenGLTexture3D() override;

    virtual uint32_t getWidth() const override { return m_specification.width; }
    virtual uint32_t getHeight() const override { return m_specification.height; }
    virtual uint32_t getDepth() const override { return m_specification.depth; }
    virtual uint32_t getRendererID() const override { return m_rendererID; }

    virtual void bind(uint32_t slot = 0) const override;
    virtual void unbind() const override;

    virtual void bindCompute(uint32_t slot = 0) const override;

    virtual void barrier() const override;
    
    // Implement texture parameter setters
    virtual void setMinFilter(TextureFilter filter) override;
    virtual void setMagFilter(TextureFilter filter) override;
    virtual void setWrapS(TextureWrap wrap) override;
    virtual void setWrapT(TextureWrap wrap) override;
    virtual void setWrapR(TextureWrap wrap) override;

    // Bindless texture implementation
    virtual bool makeResident() override;
    virtual void makeNonResident() override;
    virtual bool isResident() const override { return m_isResident; }
    virtual uint64_t getTextureHandle() const override { return m_textureHandle; }
    
private:
    void setWrap(TextureWrap wrap, GLenum pname);
    
    // Helper to generate texture handle
    void generateTextureHandle();

private:
    TextureSpecification m_specification;
    uint32_t m_rendererID = 0;
    GLenum m_internalFormat = GL_RGBA8;
    GLenum m_dataFormat = GL_RGBA;
    GLenum m_dataType = GL_UNSIGNED_BYTE;

    // Bindless texture data
    uint64_t m_textureHandle = 0;
    bool m_isResident = false;
};

} // namespace Rapture