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
        
        // Set which buffers to clear on bind
        void setClearMode(bool clearColor, bool clearDepth);
        
    private:
        std::shared_ptr<Framebuffer> m_framebuffer;
        bool m_clearColor = true;
        bool m_clearDepth = true;
        bool m_highPrecision = true;
    };
}