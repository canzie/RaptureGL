#include "ShadowMapping.h"
#include "../../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"

#include <glad/glad.h>

namespace Rapture {
        


    Rapture::ShadowMap::ShadowMap(uint32_t width, uint32_t height)
    : m_ViewProjectionMatrix(glm::mat4(1.0f)),
      m_Width(width),
      m_Height(height)
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

        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f); // Add a small bias to depth values

        // Also modify the depth test function
        glDepthFunc(GL_LEQUAL); // Allow equal depths to pass the test

    }

    void Rapture::ShadowMap::unbind()
    {

        // In ShadowMap::unbind()
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthFunc(GL_LESS); // Restore default depth function

        m_ShadowMap->unbind();
        m_Shader->unBind();


    }

    void Rapture::ShadowMap::resize(uint32_t width, uint32_t height)
    {
        // Only resize if dimensions have changed
        if (m_Width == width && m_Height == height) {
            return;
        }
        
        GE_CORE_INFO("Resizing shadow map from {}x{} to {}x{}", m_Width, m_Height, width, height);
        
        // Update member variables
        m_Width = width;
        m_Height = height;
        
        // Create new framebuffer spec
        FramebufferSpecification spec;
        spec.width = width;
        spec.height = height;
        spec.attachments = {FramebufferTextureFormat::DEPTH24STENCIL8};
        spec.attachments[0].isBindless = true;
        spec.attachments[0].isShadowMap = true;
        
        // Create new shadow map framebuffer
        m_ShadowMap = Framebuffer::create(spec);
        
        if (!m_ShadowMap || !m_ShadowMap->isValid()) {
            GE_CORE_ERROR("Failed to resize shadow map");
        }
    }

    void ShadowMap::setShaderUniforms(const glm::mat4 &mesh_transform)
    {
        ShadowMapData data;
        data.lightViewProjection[0] = m_ViewProjectionMatrix;

        

        m_UBO->setData(&data, sizeof(ShadowMapData));
        m_Shader->setMat4("u_model", mesh_transform);

    }

    void ShadowMap::setWVPMatrix(const glm::mat4 viewproj)
    {
        m_ViewProjectionMatrix = viewproj;
    }
}