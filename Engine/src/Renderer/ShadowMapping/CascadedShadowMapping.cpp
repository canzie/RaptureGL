#include "CascadedShadowMapping.h"
#include "../../logger/log.h"
#include "../../AssetsManager/AssetManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

namespace Rapture {

    CascadedShadowMapping::CascadedShadowMapping(uint32_t width, uint32_t height, uint8_t numCascades)
        : m_Width(width), m_Height(height), m_NumCascades(numCascades)
    {
        // Initialize shadow map framebuffer with multiple attachments
        FramebufferSpecification spec;
        spec.width = width;
        spec.height = height;
        
        // Add depth attachment first
        spec.attachments = {FramebufferTextureFormat::DEPTH24STENCIL8};
        
        // Add color attachments for additional cascades (n-1)
        for (uint8_t i = 1; i < m_NumCascades; i++) {
            // Use floating point format for shadow maps
            spec.attachments.push_back(FramebufferTextureFormat::RGB32F);
        }

        // Create single framebuffer with multiple attachments
        m_ShadowMap = std::make_shared<Framebuffer>(spec);
        
        std::filesystem::path s_shaderPath = std::filesystem::path("E:/Dev/Games/LiDAR Game v1/LiDAR-Game/Engine/src/Shaders/GLSL");

        auto [shader, shaderHandle] = AssetManager::importAsset<Shader>(s_shaderPath / "ShadowMapping.vs.glsl");
        if (!shader)
        {
            GE_CORE_ERROR("Failed to get shader for shadow mapping");
            return;
        }

        m_Shader = shader;
    }
    
    CascadedShadowMapping::~CascadedShadowMapping()
    {
        // Resources will be automatically cleaned up by shared_ptr
    }
    
    void CascadedShadowMapping::bind()
    {
        if (m_ShadowMap) {
            m_ShadowMap->bind();
            m_Shader->bind();
        }
    }
    
    void CascadedShadowMapping::unbind()
    {
        if (m_ShadowMap) {
            m_Shader->unBind();
            m_ShadowMap->unbind();
        }
    }
    
    /**
     * Calculates the split depths for each cascade using a hybrid of linear and logarithmic distribution
     * 
     * @param nearPlane The camera's near plane distance
     * @param farPlane The camera's far plane distance
     * @param lambda Blending factor between logarithmic (0.0) and linear (1.0) distributions
     * @return Vector of split distances in view space
     */
    std::vector<float> CascadedShadowMapping::calculateCascadeSplits(float nearPlane, float farPlane, float lambda)
    {
        std::vector<float> splitDepths(m_NumCascades + 1);
        
        // First split is always at near plane
        splitDepths[0] = nearPlane;
        
        // Calculate splits using hybrid approach
        for (uint8_t i = 1; i < m_NumCascades; i++) {
            float p = static_cast<float>(i) / m_NumCascades;
            
            // Logarithmic split calculation
            float log = nearPlane * std::pow(farPlane / nearPlane, p);
            
            // Linear split calculation
            float linear = nearPlane + (farPlane - nearPlane) * p;
            
            // Blend between logarithmic and linear based on lambda
            splitDepths[i] = lambda * log + (1.0f - lambda) * linear;
        }
        
        // Last split is always at far plane
        splitDepths[m_NumCascades] = farPlane;
        
        return splitDepths;
    }
    
    /**
     * Extracts the 8 corners of a camera frustum slice for a specific cascade
     * 
     * This method takes the camera's view and projection matrices along with a specific depth range
     * and extracts the world-space coordinates of the 8 corners that define that frustum slice.
     * These corner points are later used to create a tight-fitting orthographic projection for the light.
     *
     * @param cameraProjectionMatrix The camera's projection matrix
     * @param cameraViewMatrix The camera's view matrix (transforms from world to camera space)
     * @param cascadeNearPlane The near plane for this specific cascade slice
     * @param cascadeFarPlane The far plane for this specific cascade slice
     * @param cameraProjectionType Whether the camera uses perspective or orthographic projection
     * @return Array of 8 world-space corners of the frustum slice
     */
    std::array<glm::vec3, 8> CascadedShadowMapping::extractFrustumCorners(
        const glm::mat4& cameraProjectionMatrix,
        const glm::mat4& cameraViewMatrix,
        float cascadeNearPlane,
        float cascadeFarPlane,
        ProjectionType cameraProjectionType)
    {
        // Define the 8 corners of a canonical view frustum in NDC space
        // These are the same for both projection types (perspective and orthographic)
        // NDC space is a cube from (-1,-1,-1) to (1,1,1)
        std::array<glm::vec4, 8> ndcCorners = {
            // Near face corners (z = -1 in NDC)
            glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f), // Near-bottom-left
            glm::vec4( 1.0f, -1.0f, -1.0f, 1.0f), // Near-bottom-right
            glm::vec4( 1.0f,  1.0f, -1.0f, 1.0f), // Near-top-right
            glm::vec4(-1.0f,  1.0f, -1.0f, 1.0f), // Near-top-left
            
            // Far face corners (z = 1 in NDC)
            glm::vec4(-1.0f, -1.0f,  1.0f, 1.0f), // Far-bottom-left
            glm::vec4( 1.0f, -1.0f,  1.0f, 1.0f), // Far-bottom-right
            glm::vec4( 1.0f,  1.0f,  1.0f, 1.0f), // Far-top-right
            glm::vec4(-1.0f,  1.0f,  1.0f, 1.0f)  // Far-top-left
        };
        
        // Create a new projection matrix specific to this cascade's depth range
        glm::mat4 cascadeProjectionMatrix;
        
        if (cameraProjectionType == ProjectionType::Perspective)
        {
            // For perspective projection, use magic constants for FOV and calculate aspect ratio
            //const float fovDegrees = 45.0f; // Magic constant for FOV in degrees
            //const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);
            
               // For perspective:
            float fovY = 2.0f * atan(1.0f / cameraProjectionMatrix[1][1]);
            float aspectRatio = cameraProjectionMatrix[1][1] / cameraProjectionMatrix[0][0];
            

            // Create perspective projection matrix with the cascade's depth range
            cascadeProjectionMatrix = glm::perspective(
                glm::radians(fovY), 
                aspectRatio, 
                cascadeNearPlane, 
                cascadeFarPlane
            );
        }
        else // Orthographic
        {
            // For orthographic projection, use magic constants for the view size
            //const float orthoSize = 100.0f; // Magic constant for orthographic view size
            //const float halfSize = orthoSize * 0.5f;
            
            float right = 1.0f / cameraProjectionMatrix[0][0];
            float top = 1.0f / cameraProjectionMatrix[1][1];

            // Create orthographic projection matrix with the cascade's depth range
            cascadeProjectionMatrix = glm::ortho(
                -right, right,
                -top, top,
                cascadeNearPlane, 
                cascadeFarPlane
            );
        }
        
        // Calculate the inverse of the combined view-projection matrix for this cascade
        // This transforms from NDC space to world space
        glm::mat4 inverseViewProj = glm::inverse(cascadeProjectionMatrix * cameraViewMatrix);
        
        // Transform each NDC corner to world space
        std::array<glm::vec3, 8> worldSpaceCorners;
        for (size_t i = 0; i < 8; i++) {
            // Transform the corner from NDC to world space
            glm::vec4 worldSpaceCorner = inverseViewProj * ndcCorners[i];
            
            // Apply perspective divide to get the actual world position
            // This is needed even for orthographic projection for consistency
            worldSpaceCorners[i] = glm::vec3(worldSpaceCorner) / worldSpaceCorner.w;
        }
        
        return worldSpaceCorners;
    }
    
    /**
     * Calculates view-projection matrices for each cascade
     * 
     * @param lightDir Directional light direction
     * @param viewMatrix Camera view matrix
     * @param projMatrix Camera projection matrix
     * @param nearPlane Camera near plane
     * @param farPlane Camera far plane
     * @param cameraProjectionType Type of projection used by the camera (perspective or orthographic)
     * @return Vector of cascade data for each cascade
     */
    std::vector<CascadeData> CascadedShadowMapping::calculateCascades(
        const glm::vec3& lightDir, 
        const glm::mat4& viewMatrix, 
        const glm::mat4& projMatrix,
        float nearPlane,
        float farPlane,
        ProjectionType cameraProjectionType)
    {
        // Calculate cascade splits
        std::vector<float> cascadeSplits = calculateCascadeSplits(nearPlane, farPlane, 0.5f);
        std::vector<CascadeData> cascadeData(m_NumCascades);
        
        // Light direction and up vector for view matrix
        glm::vec3 lightDirection = glm::normalize(lightDir);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        
        // Avoid light direction parallel to up vector
        if (std::abs(glm::dot(lightDirection, up)) > 0.99f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        
        // Process each cascade
        for (uint8_t cascadeIdx = 0; cascadeIdx < m_NumCascades; cascadeIdx++) {
            // Store the cascade's depth range
            cascadeData[cascadeIdx].nearPlane = cascadeSplits[cascadeIdx];
            cascadeData[cascadeIdx].farPlane = cascadeSplits[cascadeIdx + 1];
            
            // Extract frustum corners for this cascade's depth range
            std::array<glm::vec3, 8> frustumCorners = extractFrustumCorners(
                projMatrix,           // Camera projection matrix
                viewMatrix,           // Camera view matrix
                cascadeSplits[cascadeIdx],     // Near plane for this cascade
                cascadeSplits[cascadeIdx + 1], // Far plane for this cascade
                cameraProjectionType           // Camera projection type
            );
            
            // Calculate frustum center by averaging all 8 corners
            glm::vec3 frustumCenter = glm::vec3(0.0f);
            for (const auto& corner : frustumCorners) {
                frustumCenter += corner;
            }
            frustumCenter /= 8.0f;
            
            // Create light view matrix looking at the frustum center
            // This positions the light 100 units away from the frustum center along the light direction
            glm::mat4 lightViewMatrix = glm::lookAt(
                frustumCenter - lightDirection * 100.0f,  // Light position
                frustumCenter,                            // Look-at target (frustum center)
                up                                        // Up vector
            );
            
            // Find the bounds of the frustum in light space to create a tight-fitting orthographic projection
            float minX = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float minY = std::numeric_limits<float>::max();
            float maxY = std::numeric_limits<float>::lowest();
            float minZ = std::numeric_limits<float>::max();
            float maxZ = std::numeric_limits<float>::lowest();
            
            // Transform each frustum corner to light space and find the bounds
            for (const auto& corner : frustumCorners) {
                // Transform world-space corner to light space
                glm::vec4 lightSpaceCorner = lightViewMatrix * glm::vec4(corner, 1.0f);
                
                // Update bounds
                minX = std::min(minX, lightSpaceCorner.x);
                maxX = std::max(maxX, lightSpaceCorner.x);
                minY = std::min(minY, lightSpaceCorner.y);
                maxY = std::max(maxY, lightSpaceCorner.y);
                minZ = std::min(minZ, lightSpaceCorner.z);
                maxZ = std::max(maxZ, lightSpaceCorner.z);
            }
            
            // Add padding to frustum bounds to avoid artifacts at the edges
            float padding = 10.0f;
            minX -= padding;
            maxX += padding;
            minY -= padding;
            maxY += padding;
            
            // Extend Z range to avoid shadow acne (depth bias)
            maxZ += 100.0f;
            
            // Create orthographic projection for the light
            // Note: Directional lights always use orthographic projection for shadow mapping
            glm::mat4 lightProjectionMatrix = glm::ortho(
                minX, maxX,         // Left, right
                minY, maxY,         // Bottom, top
                minZ, maxZ          // Near, far (in light space)
            );
            
            // Store the combined light view-projection matrix
            cascadeData[cascadeIdx].lightViewProj = lightProjectionMatrix * lightViewMatrix;
        }
        
        return cascadeData;
    }
}
