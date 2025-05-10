#include "CascadedShadowMapping.h"
#include "../../Logger/Log.h"
#include "../../AssetsManager/AssetManager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include "../../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include "../../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"

#include "../../WindowContext/Application.h"


namespace Rapture {

    uint8_t CascadedShadowMapping::MAX_CASCADES = 4;

    CascadedShadowMapping::CascadedShadowMapping(uint32_t width, uint32_t height, uint8_t numCascades, float lambda)
        : m_Width(width), m_Height(height), 
        m_NumCascades(numCascades), m_Lambda(lambda)
    {
       GE_CORE_INFO("CascadedShadowMapping: Creating with {0} cascades at {1}x{2} resolution", 
            m_NumCascades, m_Width, m_Height);

        // Initialize shadow map framebuffer with a depth texture array
        FramebufferSpecification spec;
        spec.width = width;
        spec.height = height;
        spec.samples = 1;
        
        // Create a single depth texture array attachment instead of multiple color attachments
        FramebufferTextureSpecification depthArraySpec(FramebufferTextureFormat::DEPTH24STENCIL8);
        depthArraySpec.isBindless = true;
        depthArraySpec.isShadowMap = true;
        depthArraySpec.isTextureArray = true;
        depthArraySpec.arrayLayers = m_NumCascades;
        
        spec.attachments.push_back(depthArraySpec);

        // Create framebuffer with depth texture array
        m_ShadowMap = Framebuffer::create(spec);
        
        if (!m_ShadowMap || !m_ShadowMap->isValid()) {
            GE_CORE_ERROR("CascadedShadowMapping: Failed to create framebuffer");
            return;
        }
        

        auto& app = Application::getInstance();
        auto project = app.getProject();
        if (!project) {
            GE_RENDER_ERROR("CascadedShadowMapping::init - Project not found, unable to start cascaded shadow mapping");
            return;
        }
        auto shaderPath = project->getConfig().shaderPath;

        // Load shader with vertex, fragment, and geometry shaders for texture array support
        auto [shader, shaderHandle] = AssetManager::importAsset<Shader>(shaderPath / "CascadedShadowMapping.vs.glsl");
        
        if (!shader)
        {
            GE_CORE_ERROR("CascadedShadowMapping: Failed to get shader");
            return;
        }
        
        // Check if shader has a geometry shader (should be detected by AssetImporter)
        // If not, issue a warning as texture arrays require a geometry shader for proper layer selection
        if (!m_ShadowMap->hasDepthTextureArray()) {
            GE_CORE_WARN("CascadedShadowMapping: Using texture arrays but no geometry shader detected. "
                         "Ensure CascadedShadowMapping.gs.glsl exists and is properly loaded.");
        } else {
            GE_CORE_INFO("CascadedShadowMapping: Successfully loaded shader with texture array support");
        }

        m_Shader = shader;
        m_UBO = std::make_shared<UniformBuffer>(sizeof(ShadowMapData), BufferUsage::Stream, nullptr, SHADOW_MATRICES_BINDING_POINT_IDX);
        
        if (!m_UBO) {
            GE_CORE_ERROR("CascadedShadowMapping: Failed to create UBO");
            return;
        }
        
        // Initialize view projection matrices
        m_ViewProjectionMatrices.resize(m_NumCascades, glm::mat4(1.0f));
        
        GE_CORE_INFO("CascadedShadowMapping: Successfully initialized with depth texture array");
    }
    
    CascadedShadowMapping::~CascadedShadowMapping()
    {
        // Resources will be automatically cleaned up by shared_ptr
        GE_CORE_INFO("CascadedShadowMapping: Destructor called");
    }
    
    void CascadedShadowMapping::bind()
    {
        if (!m_ShadowMap) {
            GE_CORE_ERROR("CascadedShadowMapping: Cannot bind null framebuffer");
            return;
        }
        
        if (!m_Shader) {
            GE_CORE_ERROR("CascadedShadowMapping: Cannot bind null shader");
            return;
        }
        
        if (!m_UBO) {
            GE_CORE_ERROR("CascadedShadowMapping: Cannot bind null UBO");
            return;
        }
        
        m_ShadowMap->bind();
        m_Shader->bind();
        m_UBO->bindBase(SHADOW_MATRICES_BINDING_POINT_IDX);
        
    }
    
    void CascadedShadowMapping::unbind()
    {
        if (m_ShadowMap && m_Shader) {
            m_Shader->unBind();
            m_ShadowMap->unbind();
        } else {
            GE_CORE_WARN("CascadedShadowMapping: Attempted to unbind null framebuffer or shader");
        }
    }

    void CascadedShadowMapping::setShaderUniforms(const glm::mat4 &mesh_transform)
    {
        if (!m_UBO) {
            GE_CORE_ERROR("CascadedShadowMapping::setShaderUniforms: UBO is null");
            return;
        }
        
        if (m_ViewProjectionMatrices.empty()) {
            GE_CORE_ERROR("CascadedShadowMapping::setShaderUniforms: No view-projection matrices available");
            return;
        }
        
        ShadowMapData data;
        for (uint8_t i = 0; i < m_NumCascades; i++) {
            data.lightViewProjection[i] = m_ViewProjectionMatrices[i];
        }

        m_Shader->setMat4("u_model", mesh_transform);
        m_UBO->setData(&data, sizeof(ShadowMapData));
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
        // Validate input parameters
        if (nearPlane <= 0.0f) {
            GE_CORE_ERROR("CascadedShadowMapping::calculateCascadeSplits: Near plane must be positive, got {0}", nearPlane);
            nearPlane = 0.1f; // Default fallback
        }
        
        if (farPlane <= nearPlane) {
            GE_CORE_ERROR("CascadedShadowMapping::calculateCascadeSplits: Far plane ({0}) must be greater than near plane ({1})", 
                farPlane, nearPlane);
            farPlane = nearPlane * 100.0f; // Default fallback
        }
        
        
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

    uint64_t CascadedShadowMapping::getCascadeTextureHandle() const
    {
        uint64_t arrayHandle = 0;
        
        if (!m_ShadowMap) {
            GE_CORE_ERROR("CascadedShadowMapping::getCascadeTextureHandles: Shadow map is null");
            return arrayHandle;
        }

        if (m_ShadowMap->hasDepthTextureArray()) {
            arrayHandle = m_ShadowMap->getDepthTextureArrayHandle();
        }

        if (arrayHandle == 0) {
            GE_CORE_ERROR("CascadedShadowMapping: Received invalid handle (0) for depth texture array");
        }

        return arrayHandle;
    }

    uint32_t CascadedShadowMapping::getCascadeTextureID() const
    {
        uint32_t arrayID = 0;
        
        if (!m_ShadowMap) {
            GE_CORE_ERROR("CascadedShadowMapping::getCascadeTextureHandles: Shadow map is null");
            return arrayID;
        }

        if (m_ShadowMap->hasDepthTextureArray()) {
            arrayID = m_ShadowMap->getDepthTextureArrayID();
        }

        if (arrayID == 0) {
            GE_CORE_ERROR("CascadedShadowMapping: Received invalid ID (0) for depth texture array");
        }

        return arrayID;
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
        // Validate input parameters
        if (glm::any(glm::isnan(cameraProjectionMatrix[0])) || 
            glm::any(glm::isnan(cameraViewMatrix[0]))) {
            GE_CORE_ERROR("CascadedShadowMapping::extractFrustumCorners: Received NaN in input matrices");
        }
        
        if (cascadeNearPlane <= 0.0f) {
            GE_CORE_ERROR("CascadedShadowMapping::extractFrustumCorners: Near plane must be positive, got {0}", 
                cascadeNearPlane);
            cascadeNearPlane = 0.1f; // Fallback
        }
        
        if (cascadeFarPlane <= cascadeNearPlane) {
            GE_CORE_ERROR("CascadedShadowMapping::extractFrustumCorners: Far plane ({0}) must be greater than near plane ({1})", 
                cascadeFarPlane, cascadeNearPlane);
            cascadeFarPlane = cascadeNearPlane * 10.0f; // Fallback
        }
        
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
            // Extract FOV and aspect ratio from projection matrix
            float fovY = 0.0f;
            float aspectRatio = 1.0f;
            
            try {
                // For perspective: extract parameters from projection matrix
                fovY = 2.0f * atan(1.0f / cameraProjectionMatrix[1][1]);
                aspectRatio = cameraProjectionMatrix[1][1] / cameraProjectionMatrix[0][0];
                
                if (fovY <= 0.0f || fovY > glm::radians(180.0f)) {
                    GE_CORE_ERROR("CascadedShadowMapping: Invalid FOV extracted: {0} radians", fovY);
                    fovY = glm::radians(45.0f); // Default fallback
                }
                
                if (aspectRatio <= 0.0f) {
                    GE_CORE_ERROR("CascadedShadowMapping: Invalid aspect ratio extracted: {0}", aspectRatio);
                    aspectRatio = 1.0f; // Default fallback
                }
            }
            catch (const std::exception& e) {
                GE_CORE_ERROR("CascadedShadowMapping: Exception extracting perspective parameters: {0}", e.what());
                // Use fallback values
                fovY = glm::radians(45.0f);
                aspectRatio = 1.0f;
            }

            // Create perspective projection matrix with the cascade's depth range
            cascadeProjectionMatrix = glm::perspective(
                fovY, 
                aspectRatio, 
                cascadeNearPlane, 
                cascadeFarPlane
            );
        }
        else // Orthographic
        {
            float right = 0.0f;
            float top = 0.0f;
            
            try {
                // Extract orthographic dimensions from projection matrix
                right = 1.0f / cameraProjectionMatrix[0][0];
                top = 1.0f / cameraProjectionMatrix[1][1];
                
                if (right <= 0.0f) {
                    GE_CORE_ERROR("CascadedShadowMapping: Invalid right value extracted: {0}", right);
                    right = 10.0f; // Default fallback
                }
                
                if (top <= 0.0f) {
                    GE_CORE_ERROR("CascadedShadowMapping: Invalid top value extracted: {0}", top);
                    top = 10.0f; // Default fallback
                }
            }
            catch (const std::exception& e) {
                GE_CORE_ERROR("CascadedShadowMapping: Exception extracting orthographic parameters: {0}", e.what());
                // Use fallback values
                right = 10.0f;
                top = 10.0f;
            }


            // Create orthographic projection matrix with the cascade's depth range
            cascadeProjectionMatrix = glm::ortho(
                -right, right,
                -top, top,
                cascadeNearPlane, 
                cascadeFarPlane
            );
        }
        
        // Check for invalid transform matrix
        if (glm::any(glm::isnan(cascadeProjectionMatrix[0]))) {
            GE_CORE_ERROR("CascadedShadowMapping: Generated cascade projection matrix contains NaN");
            return std::array<glm::vec3, 8>{}; // Return empty corners
        }
        
        // Calculate the inverse of the combined view-projection matrix for this cascade
        // This transforms from NDC space to world space
        glm::mat4 inverseViewProj;
        
        try {
            inverseViewProj = glm::inverse(cascadeProjectionMatrix * cameraViewMatrix);
            
            // Check for invalid inverse matrix
            if (glm::any(glm::isnan(inverseViewProj[0]))) {
                GE_CORE_ERROR("CascadedShadowMapping: Inverse view-projection matrix contains NaN");
                return std::array<glm::vec3, 8>{}; // Return empty corners
            }
        }
        catch (const std::exception& e) {
            GE_CORE_ERROR("CascadedShadowMapping: Exception calculating inverse matrix: {0}", e.what());
            return std::array<glm::vec3, 8>{}; // Return empty corners
        }
        
        // Transform each NDC corner to world space
        std::array<glm::vec3, 8> worldSpaceCorners;
        for (size_t i = 0; i < 8; i++) {
            // Transform the corner from NDC to world space
            glm::vec4 worldSpaceCorner = inverseViewProj * ndcCorners[i];
            
            // Check for invalid transformed corner
            if (glm::any(glm::isnan(worldSpaceCorner)) || worldSpaceCorner.w == 0.0f) {
                GE_CORE_ERROR("CascadedShadowMapping: Invalid frustum corner calculated (NaN or w=0)");
                worldSpaceCorners[i] = glm::vec3(0.0f); // Fallback
            } else {
                // Apply perspective divide to get the actual world position
                worldSpaceCorners[i] = glm::vec3(worldSpaceCorner) / worldSpaceCorner.w;
            }
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
        // Calculate cascade splits - Use lambda=0.95 for better distribution
        std::vector<float> cascadeSplits = calculateCascadeSplits(nearPlane, farPlane, m_Lambda); 


        std::vector<CascadeData> cascadeData(m_NumCascades);
        m_ViewProjectionMatrices.resize(m_NumCascades);
        
        // Light direction and up vector for view matrix
        glm::vec3 lightDirection = glm::normalize(lightDir);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(lightDirection, up)) > 0.99f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        for (uint8_t cascadeIdx = 0; cascadeIdx < m_NumCascades; cascadeIdx++) {
            cascadeData[cascadeIdx].nearPlane = cascadeSplits[cascadeIdx];
            cascadeData[cascadeIdx].farPlane = cascadeSplits[cascadeIdx + 1];

            // 1. Extract World-Space Frustum Corners
            std::array<glm::vec3, 8> frustumCorners = extractFrustumCorners(
                projMatrix, viewMatrix,
                cascadeSplits[cascadeIdx], cascadeSplits[cascadeIdx + 1],
                cameraProjectionType
            );

            // 2. Calculate Frustum Center and Bounding Sphere Radius
            glm::vec3 frustumCenter = glm::vec3(0.0f);
            for (const auto& corner : frustumCorners) {
                frustumCenter += corner;
            }
            frustumCenter /= 8.0f;

            float sphereRadius = 0.0f;
            for (const auto& corner : frustumCorners) {
                float distance = glm::length(corner - frustumCenter);
                sphereRadius = std::max(sphereRadius, distance);
            }
            // Round up the radius to ensure all corners are inside
            sphereRadius = std::ceil(sphereRadius * 16.0f) / 16.0f; // Optional: snap radius for minor stability


            // 3. Create Light View Matrix
            // Move the light's position back along its direction relative to the sphere center
            // The distance needs to account for the sphere radius to ensure the near plane includes the sphere
            // We'll calculate the precise near/far later by projecting corners.
            float lightDistance = sphereRadius; // Use sphereRadius as a base, adjust near/far later
            glm::mat4 lightViewMatrix = glm::lookAt(
                frustumCenter - lightDirection * lightDistance, // Position light relative to sphere center
                frustumCenter,                                  // Look at sphere center
                up
            );

            // 4. Define Orthographic Bounds using Bounding Sphere
            // The radius directly defines the extents in light space because the view looks at the center
            float minX = -sphereRadius;
            float maxX = sphereRadius;
            float minY = -sphereRadius;
            float maxY = sphereRadius;

            // Declare snappedCenterWorld outside the if block
            glm::vec4 snappedCenterWorld = glm::vec4(frustumCenter, 1.0f);

            // --- Stabilization: Snap Ortho Bounds to Texel Grid ---
            if (m_Width > 0 && m_Height > 0) {
                // Calculate world units per texel based on the sphere radius derived projection size
                float extentX = maxX - minX; // Should be 2.0f * sphereRadius
                float extentY = maxY - minY; // Should be 2.0f * sphereRadius
                float unitsPerTexelX = extentX / static_cast<float>(m_Width);
                float unitsPerTexelY = extentY / static_cast<float>(m_Height);

                // Transform the sphere center into light view space
                glm::vec4 centerLightSpace = lightViewMatrix * glm::vec4(frustumCenter, 1.0f);

                // Snap the center x/y in light space to the texel grid
                // We want floor snapping to prevent potential precision issues with moving the boundary
                centerLightSpace.x = floor(centerLightSpace.x / unitsPerTexelX) * unitsPerTexelX;
                centerLightSpace.y = floor(centerLightSpace.y / unitsPerTexelY) * unitsPerTexelY;

                // Transform the snapped center back to world space
                glm::mat4 inverseLightView = glm::inverse(lightViewMatrix);
                snappedCenterWorld = inverseLightView * centerLightSpace; // Assign to the outer scope variable

                // Recalculate the light view matrix using the snapped world space center
                lightViewMatrix = glm::lookAt(
                    glm::vec3(snappedCenterWorld) - lightDirection * lightDistance,
                    glm::vec3(snappedCenterWorld), // Look at the snapped center
                    up
                );
            } else {
                 GE_CORE_WARN("Cascade {}: Shadow map width/height is zero, skipping stabilization.", cascadeIdx);
            }

            // 7. Calculate Ortho Z Bounds by Projecting World Corners onto Light Direction
            // The Z bounds calculation remains the same, using the *original* frustum corners
            // and the *final* stabilized lightViewMatrix
            float minLightDist = std::numeric_limits<float>::max();
            float maxLightDist = std::numeric_limits<float>::lowest();
            for (const auto& corner : frustumCorners) {
                // Project corner onto light direction relative to the *snapped* frustum center
                float distance = glm::dot(corner - glm::vec3(snappedCenterWorld), lightDirection); // Now uses the correct variable
                minLightDist = std::min(minLightDist, distance);
                maxLightDist = std::max(maxLightDist, distance);
            }
            float orthoNear = minLightDist;
            float orthoFar = maxLightDist;

            // Adjust based on distance from the *snapped* center along the light direction
            orthoNear = minLightDist;
            orthoFar = maxLightDist;

            // Add a buffer/margin to the far plane if needed
            orthoFar += 100.0f; // Extend far plane further out


            // Optional: Ensure near plane is not negative after adjustments if needed
            // The near/far values are distances *along* the light direction *relative* to the lookAt target (frustumCenter)
            // The glm::ortho function expects distances *from* the viewpoint.

            // 8. Create Orthographic Matrix using the sphere-based bounds and stabilized view
            glm::mat4 lightProjectionMatrix = glm::ortho(
                minX, maxX,         // Left, right (from sphere radius)
                minY, maxY,         // Bottom, top (from sphere radius)
                orthoNear, orthoFar // Near, far (calculated via projection relative to center)
            );

            // 9. Store Final Matrix
            cascadeData[cascadeIdx].lightViewProj = lightProjectionMatrix * lightViewMatrix;
            m_ViewProjectionMatrices[cascadeIdx] = cascadeData[cascadeIdx].lightViewProj;
        }

        return cascadeData;
    }
}
