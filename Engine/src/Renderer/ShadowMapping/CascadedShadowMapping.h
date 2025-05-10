#pragma once

#include "../Framebuffer.h"
#include "../../Shaders/Shader.h"
#include <glm/glm.hpp>
#include <vector>
#include "ShadowMappingBase.h"

namespace Rapture {

    // Projection type enum (to avoid dependency on Frustum.h)
    enum class ProjectionType
    {
        Perspective,
        Orthographic
    };

    struct CascadeData {
        float nearPlane;
        float farPlane;
        glm::mat4 lightViewProj;
    };

    class CascadedShadowMapping : public ShadowMapBase {

    public:
        CascadedShadowMapping(uint32_t width, uint32_t height, uint8_t numCascades, float lambda = 0.8f);
        ~CascadedShadowMapping();

        virtual void bind() override;
        virtual void unbind() override;

        virtual void setShaderUniforms(const glm::mat4& mesh_transform) override;
        
        // Returns the calculated split depths for each cascade using a hybrid approach
        std::vector<float> calculateCascadeSplits(float nearPlane, float farPlane, float lambda = 0.5f);
        
        // Calculates the light space matrices for each cascade
        std::vector<CascadeData> calculateCascades(
            const glm::vec3& lightDir, 
            const glm::mat4& viewMatrix, 
            const glm::mat4& projMatrix,
            float nearPlane,
            float farPlane,
            ProjectionType cameraProjectionType = ProjectionType::Perspective);
            
        // Accessor methods
        inline uint8_t getNumCascades() const { return m_NumCascades; }
        inline std::shared_ptr<Framebuffer> getShadowMap() const { return m_ShadowMap; }

        uint64_t getCascadeTextureHandle() const;
        uint32_t getCascadeTextureID() const;
        std::vector<glm::mat4> getViewProjectionMatrices() { return m_ViewProjectionMatrices; }
        
        float getLambda() const { return m_Lambda; }
        void setLambda(float lambda) { m_Lambda = std::clamp(lambda, 0.0f, 1.0f); }



    public:
        static uint8_t MAX_CASCADES;

    private:
        // Extracts view frustum corners for a specific cascade depth slice
        // All parameters relate to the camera, not the light
        std::array<glm::vec3, 8> extractFrustumCorners(
            const glm::mat4& cameraProjectionMatrix, // The camera's projection matrix
            const glm::mat4& cameraViewMatrix,       // The camera's view matrix
            float cascadeNearPlane,                  // Near plane for this specific cascade
            float cascadeFarPlane,                   // Far plane for this specific cascade
            ProjectionType cameraProjectionType);    // Type of projection used by the camera

        uint32_t m_Width;
        uint32_t m_Height;
        uint8_t m_NumCascades;
        float m_Lambda;
        std::vector<glm::mat4> m_ViewProjectionMatrices;


    };

}