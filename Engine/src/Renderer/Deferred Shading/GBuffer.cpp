#include "GBuffer.h"
#include <glad/glad.h> // Include for OpenGL functions
#include "../../Textures/Texture.h" // Include for TextureActiveSlot

namespace Rapture {

    GBuffer::GBuffer(uint32_t width, uint32_t height, bool useHighPrecision)
        : m_framebuffer(Framebuffer::createGBuffer(width, height, useHighPrecision))
        , m_clearColor(true)
        , m_clearDepth(true)
        , m_highPrecision(useHighPrecision)
    {
    }

    void GBuffer::resize(uint32_t width, uint32_t height)
    {
        m_framebuffer->resize(width, height);
    }

    void GBuffer::bind()
    {
        m_framebuffer->bind();
    }

    void GBuffer::unbind()
    {
        m_framebuffer->unbind();
    }

    uint32_t GBuffer::getPositionTextureID() const
    {
        return m_framebuffer->getColorAttachmentRendererID(static_cast<uint32_t>(GBufferAttachmentType::POSTITION));
    }

    uint32_t GBuffer::getNormalTextureID() const
    {
        return m_framebuffer->getColorAttachmentRendererID(static_cast<uint32_t>(GBufferAttachmentType::NORMAL));
    }

    uint32_t GBuffer::getAlbedoTextureID() const
    {
        return m_framebuffer->getColorAttachmentRendererID(static_cast<uint32_t>(GBufferAttachmentType::ALBEDO));
    }

    uint32_t GBuffer::getMaterialTextureID() const
    {
        return m_framebuffer->getColorAttachmentRendererID(static_cast<uint32_t>(GBufferAttachmentType::MATERIAL));
    }

    uint32_t GBuffer::getDepthTextureID() const
    {
        return m_framebuffer->getDepthAttachmentRendererID();
    }

    void GBuffer::bindTextures()
    {
        // Bind GBuffer textures to specific slots defined in TextureActiveSlot
        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
        glBindTexture(GL_TEXTURE_2D, getAlbedoTextureID());

        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::NORMAL));
        glBindTexture(GL_TEXTURE_2D, getNormalTextureID());

        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::MATERIAL));
        glBindTexture(GL_TEXTURE_2D, getMaterialTextureID());

        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::POSTITION)); 
        glBindTexture(GL_TEXTURE_2D, getPositionTextureID());

        // Optionally bind depth if needed by the shader, choosing an unused slot
        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::DEPTH)); // Example: Slot 4
        glBindTexture(GL_TEXTURE_2D, getDepthTextureID());
    }

    void GBuffer::unbindTextures()
    {
        // Unbind textures from the slots
        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::ALBEDO));
        glBindTexture(GL_TEXTURE_2D, 0);

        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::NORMAL));
        glBindTexture(GL_TEXTURE_2D, 0);

        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::MATERIAL));
        glBindTexture(GL_TEXTURE_2D, 0);

        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::POSTITION)); // Note: Typo in enum (POSTION)
        glBindTexture(GL_TEXTURE_2D, 0);

        // Optionally unbind depth if it was bound
        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::DEPTH)); // Example: Slot 4
        glBindTexture(GL_TEXTURE_2D, 0);

        // Reset active texture unit to default
        glActiveTexture(GL_TEXTURE0);
    }

    void GBuffer::setClearMode(bool clearColor, bool clearDepth)
    {
        m_clearColor = clearColor;
        m_clearDepth = clearDepth;
    }

}
