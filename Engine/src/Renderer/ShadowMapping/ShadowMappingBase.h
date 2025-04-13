#pragma once

#include "../Framebuffer.h"
#include "../../Shaders/Shader.h"
#include "../../AssetsManager/AssetManager.h"
#include "../../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>
#include <cstdint>

namespace Rapture {


    // TODO
    // Finish base class
    // use ubos for because the csm will have multiple viewproj matrices
    // each shadowmap will have its own ubo

    struct ShadowMapData {
        glm::mat4 lightViewProjection[4];
    };

    class ShadowMapBase {

    public:

        virtual void bind() = 0;
        virtual void unbind() = 0;

        virtual void setShaderUniforms(const glm::mat4& mesh_transform) = 0;
        void setWVPMatrices(const glm::mat4& viewproj) {
            if (m_ViewProjectionMatrices.size() > 0) {
                m_ViewProjectionMatrices[0] = viewproj;
            } else {
                m_ViewProjectionMatrices.push_back(viewproj);
            }
        };

        uint64_t getShadowMapTextureHandle() { 
            if (m_ShadowMap) {
                return m_ShadowMap->getDepthAttachmentTextureHandle();
            }
            return 0;
        }

        std::shared_ptr<Shader> getShader() { return m_Shader; }

        //void setWVPMatrices(const glm::mat4& view, const glm::mat4& projection);

    protected:
        std::shared_ptr<Framebuffer> m_ShadowMap;
        std::shared_ptr<Shader> m_Shader;
        AssetHandle m_SMShaderHandle;

        std::vector<glm::mat4> m_ViewProjectionMatrices;
        std::shared_ptr<UniformBuffer> m_UBO;
    };

}
