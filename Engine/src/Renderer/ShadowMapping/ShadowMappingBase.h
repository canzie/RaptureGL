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

    // gets used in the ubo for drawing to the shadowmap textures
    struct ShadowMapData {
        glm::mat4 lightViewProjection[4];
    };

    class ShadowMapBase {

    public:

        virtual void bind() = 0;
        virtual void unbind() = 0;

        virtual void setShaderUniforms(const glm::mat4& mesh_transform) = 0;


        std::shared_ptr<Shader> getShader() { return m_Shader; }

    protected:
        std::shared_ptr<Framebuffer> m_ShadowMap;
        std::shared_ptr<Shader> m_Shader;
        AssetHandle m_SMShaderHandle;

        std::shared_ptr<UniformBuffer> m_UBO;
    };

}
