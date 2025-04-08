#include "ShadowMapping.h"

#include "../../WindowContext/Application.h"

#include <glad/glad.h>

namespace Rapture {
        


    Rapture::ShadowMap::ShadowMap(uint32_t width, uint32_t height)
    : m_gWVP(glm::mat4(1.0f))
    {
        FramebufferSpecification spec;
        spec.width = width;
        spec.height = height;
        spec.attachments = {FramebufferTextureFormat::DEPTH24STENCIL8};

        m_ShadowMap = Framebuffer::create(spec);
        m_ShadowMap->invalidate(true);

        if (!m_ShadowMap || !m_ShadowMap->isValid())
        {
            GE_CORE_ERROR("Failed to create shadow map");
            return;
        }

        Application& app = Application::getInstance();
        std::filesystem::path s_shaderPath = app.getCurrentProject()->getConfig().shaderPath;

        auto [shader, shaderHandle] = AssetManager::importAsset<Shader>(s_shaderPath / "ShadowMapping.vs.glsl");
        if (!shader)
        {
            GE_CORE_ERROR("Failed to get shader for shadow mapping");
            m_ShadowMap.reset();
            return;
        }

        m_SMShaderHandle = shaderHandle;
        m_Shader = shader;
    }

    void Rapture::ShadowMap::bind()
    {
        if (!m_ShadowMap)
        {
            GE_CORE_ERROR("Shadow map not created");
            return;
        }
        
        // check shader
        if (!m_Shader.lock())
        {
            // check if shader can be reloaded using assethandle
            getShader();
            if (!m_Shader.lock()) return;
        }

        m_ShadowMap->bind();
        auto shader = m_Shader.lock();
        shader->bind();
        shader->setMat4("gWVP", m_gWVP);

    }

    void ShadowMap::bindForReading()
    {
        glActiveTexture(static_cast<GLenum>(TextureActiveSlot::SHADOW));
        glBindTexture(GL_TEXTURE_2D, m_ShadowMap->getDepthAttachmentRendererID());
    }

    void Rapture::ShadowMap::unbind()
    {
        m_ShadowMap->unbind();

        if (m_Shader.lock())
        {
            m_Shader.lock()->unBind();
        }

    }

    void Rapture::ShadowMap::setShader(AssetHandle shaderHandle)
    {
        if (m_SMShaderHandle == shaderHandle)
        {
            return;
        }

        auto shader = AssetManager::getAsset<Shader>(shaderHandle);
        if (shader)
        {
            m_Shader = shader;
            m_SMShaderHandle = shaderHandle;
        }
        
    }

    std::shared_ptr<Shader> ShadowMap::getShader()
    {
        if (auto shader = m_Shader.lock())
        {
            return shader;
        } else {
            shader = AssetManager::getAsset<Shader>(m_SMShaderHandle);
            if (shader)
            {
                m_Shader = shader;
                return shader;
            }
        }
        return nullptr;
    }

    void ShadowMap::setWVPMatrix(const glm::mat4 &gWVP)
    {
        m_gWVP = gWVP;
    }
}