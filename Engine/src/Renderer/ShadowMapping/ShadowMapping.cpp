#include "ShadowMapping.h"
#include "../../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"

#include <glad/glad.h>

namespace Rapture {
        


    Rapture::ShadowMap::ShadowMap(uint32_t width, uint32_t height)
    {


        FramebufferSpecification spec;
        spec.width = width;
        spec.height = height;
        spec.attachments = {FramebufferTextureFormat::DEPTH24STENCIL8};
        spec.attachments[0].isBindless = true;
        spec.attachments[0].isShadowMap = true;

        m_ShadowMap = Framebuffer::create(spec);

        if (!m_ShadowMap || !m_ShadowMap->isValid())
        {
            GE_CORE_ERROR("Failed to create shadow map");
            return;
        }


        std::filesystem::path s_shaderPath = std::filesystem::path("E:/Dev/Games/LiDAR Game v1/LiDAR-Game/Engine/src/Shaders/GLSL");

        

        auto [shader, shaderHandle] = AssetManager::importAsset<Shader>(s_shaderPath / "ShadowMapping.vs.glsl");
        if (!shader)
        {
            GE_CORE_ERROR("Failed to get shader for shadow mapping");
            m_ShadowMap.reset();
            return;
        }
        
        m_UBO = std::make_shared<UniformBuffer>(sizeof(ShadowMapData), BufferUsage::Stream, nullptr, SHADOW_MATRICES_BINDING_POINT_IDX);

        m_SMShaderHandle = shaderHandle;
        m_Shader = shader;
    }

    ShadowMap::~ShadowMap()
    {
        m_ShadowMap.reset();
        m_Shader.reset();
    }

    void Rapture::ShadowMap::bind()
    {
        if (!m_ShadowMap)
        {
            GE_CORE_ERROR("Shadow map not created");
            return;
        }

        m_ShadowMap->bind();
        m_Shader->bind();
        m_UBO->bindBase(SHADOW_MATRICES_BINDING_POINT_IDX);

    }

    void Rapture::ShadowMap::unbind()
    {
        m_ShadowMap->unbind();
        m_Shader->unBind();
    }

    void ShadowMap::setShaderUniforms(const glm::mat4 &mesh_transform)
    {
        ShadowMapData data;
        if (m_ViewProjectionMatrices.size() == 0) {
            GE_CORE_ERROR("No view projection matrices found");
            return;
        }

        data.lightViewProjection[0] = m_ViewProjectionMatrices[0]*mesh_transform;

        if (m_ViewProjectionMatrices.size() > 0)
        {
            m_UBO->setData(&data, sizeof(ShadowMapData));
        }
    }

}