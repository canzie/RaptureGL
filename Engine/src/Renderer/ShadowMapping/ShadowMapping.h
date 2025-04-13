#pragma once

#include "../Framebuffer.h"
#include "../../Shaders/Shader.h"
#include "../../Logger/Log.h"
#include "../../AssetsManager/AssetManager.h"
#include "../Frustum.h"
#include "../../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include "ShadowMappingBase.h"

#include <glm/glm.hpp>

#include <memory>

namespace Rapture {

    class ShadowMap : public ShadowMapBase {

    public:

        ShadowMap(uint32_t width, uint32_t height);
        ~ShadowMap();

        virtual void bind() override;
        virtual void unbind() override;


        virtual void setShaderUniforms(const glm::mat4& mesh_transform) override;

        uint32_t getShadowMapID() { return m_ShadowMap->getDepthAttachmentRendererID(); }
        uint64_t getShadowMapHandle() { return m_ShadowMap->getDepthAttachmentTextureHandle(); }


    private:


    };


}