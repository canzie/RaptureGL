#include "ShadowMapping.h"


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

        // Configure shadow map texture parameters specifically for shadow mapping
        // These parameters are critical for proper shadow mapping
        uint32_t depthTextureID = m_ShadowMap->getDepthAttachmentRendererID();
        glBindTexture(GL_TEXTURE_2D, depthTextureID);
        
        // Set linear filtering for smoother shadows
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Set proper wrapping for shadow map
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        
        // Set border color to white (1.0) - fragments beyond shadow map will be considered lit
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        
        // Enable hardware PCF with linear filtering
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        
        glBindTexture(GL_TEXTURE_2D, 0);
        
        GE_CORE_INFO("Shadow map texture configured with specialized parameters for shadow mapping");

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

        m_ShadowMap->bind();


    }

    void ShadowMap::bindForReading()
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::SHADOW)); 
        glBindTexture(GL_TEXTURE_2D, m_ShadowMap->getDepthAttachmentRendererID());
    }

    void ShadowMap::unbindForReading()
    {
        glActiveTexture(GL_TEXTURE0 + static_cast<uint32_t>(TextureActiveSlot::SHADOW)); 
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Rapture::ShadowMap::unbind()
    {
        m_ShadowMap->unbind();

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