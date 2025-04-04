#include "GBuffer.h"

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

    void GBuffer::setClearMode(bool clearColor, bool clearDepth)
    {
        m_clearColor = clearColor;
        m_clearDepth = clearDepth;
    }

}
