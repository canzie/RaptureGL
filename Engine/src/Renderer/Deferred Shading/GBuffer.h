#pragma once

#include "../Framebuffer.h"
#include <memory>

namespace Rapture {

    class GBuffer {
    public:
        GBuffer(uint32_t width, uint32_t height, bool useHighPrecision = true);
        ~GBuffer() = default;
        
        void resize(uint32_t width, uint32_t height);
        void bind();
        void unbind();
        
        // Get texture IDs for shader binding
        uint32_t getPositionTextureID() const;
        uint32_t getNormalTextureID() const;
        uint32_t getAlbedoTextureID() const;
        uint32_t getMaterialTextureID() const;
        uint32_t getDepthTextureID() const;

        const FramebufferSpecification& getSpecification() const { return m_framebuffer->getSpecification(); }
        const uint32_t& getFramebufferID() const { return m_framebuffer->getFramebufferID(); }

        // Bind the textures to the correct texture unit
        void bindTextures();
        void unbindTextures();

        void bindTexturesCompute();
        
        // Set which buffers to clear on bind
        void setClearMode(bool clearColor, bool clearDepth);
        
    private:
        std::shared_ptr<Framebuffer> m_framebuffer;
        bool m_clearColor = true;
        bool m_clearDepth = true;
        bool m_highPrecision = true;
    };
}