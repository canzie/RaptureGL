#include "RadianceCascadesManager.h"
#include "../../WindowContext/Application.h" // For getting camera
#include "../../Scenes/Entity.h"             // For camera entity
#include "../../Scenes/Components/Components.h" // For CameraControllerComponent
#include "../../Debug/TracyProfiler.h" // For profiling
#include <glm/gtc/matrix_transform.hpp> // For glm::translate, glm::scale
#include <cmath>                        // For std::ceil, std::sqrt
#include <limits>                       // For std::numeric_limits
#include <algorithm>                    // For std::max, std::min


namespace Rapture {
    


std::shared_ptr<RadianceCascadeHierarchy> RadianceCascadesManager::m_hierarchy = nullptr;
std::shared_ptr<Shader> RadianceCascadesManager::m_computeShader = nullptr;
std::shared_ptr<ShaderStorageBuffer> RadianceCascadesManager::m_cascadeInfoSSBO = nullptr;
std::string RadianceCascadesManager::_populateShaderName = "RadianceCascadingCS/PopulateCascade_SSR.cs.glsl";
std::string RadianceCascadesManager::_testShaderName = "RadianceCascadingCS/test.cs.glsl"; // Placeholder shader
bool RadianceCascadesManager::m_initialized = false;

void RadianceCascadesManager::init(BuildParams params)
{
    if (m_initialized) {
        GE_CORE_WARN("RadianceCascadesManager::init - Already initialized. Skipping.");
        return;
    }


    m_hierarchy = std::make_shared<RadianceCascadeHierarchy>();

    m_hierarchy->buildCascades(params);

    auto& app = Application::getInstance();
    auto project = app.getProject();
    if (!project) {
        GE_RENDER_ERROR("RadianceCascadesManager::init - Project not found, unable to start radiance cascades manager");
        return;
    }

    auto [shader, handle] = AssetManager::importAsset<Shader>(project->getConfig().shaderPath / _populateShaderName);
    m_computeShader = shader;


    // 2. Create SSBO for cascade shader data
    size_t bufferSize = sizeof(RadianceCascadeShaderData) * m_hierarchy->getNumCascades();
    if (bufferSize == 0 || m_hierarchy->getNumCascades() == 0) {
        GE_CORE_WARN("RadianceCascadesManager - No cascades built. Skipping SSBO creation.");
        return;
    }

    m_cascadeInfoSSBO = std::make_shared<ShaderStorageBuffer>(bufferSize, BufferUsage::Stream); // Stream for frame updates
    GE_CORE_INFO("RadianceCascadesManager - Created CascadeInfo SSBO for {} cascades ({} bytes)", m_hierarchy->getNumCascades(), bufferSize);

    m_initialized = true;


    updateGpuBuffers();




}

void RadianceCascadesManager::shutdown() {
    // Ensure shutdown is called if not already
    if (!m_initialized) {
        GE_CORE_WARN("RadianceCascadesManager::shutdown - Not initialized. Skipping.");
        return;
    }

    m_initialized = false;
    m_cascadeInfoSSBO.reset();
    m_computeShader.reset();
}


void RadianceCascadesManager::calculateCascadeTransforms(std::vector<RadianceCascadeShaderData>& shaderData) {

    RAPTURE_PROFILE_FUNCTION();

    if (!m_initialized) {
        GE_CORE_WARN("RadianceCascadesManager::calculateCascadeTransforms - Not initialized. Skipping.");
        return;
    }

    // --- Get Camera Data ---
    auto& app = Application::getInstance();
    auto project = app.getCurrentProject();
    if (!project) {
        GE_CORE_WARN("RadianceCascadesManager::calculateCascadeTransforms - No current project.");
        return;
    }
    auto scene = project->getActiveScene();
    if (!scene) {
         GE_CORE_WARN("RadianceCascadesManager::calculateCascadeTransforms - No active scene.");
        return;
    }
    auto cameraEntity = scene->getMainCamera();
    if (!cameraEntity || !cameraEntity->isValid()) {
         GE_CORE_WARN("RadianceCascadesManager::calculateCascadeTransforms - No valid main camera entity.");
        return; // No camera yet
    }
    auto camComponent = cameraEntity->tryGetComponent<CameraControllerComponent>();
    if (!camComponent) {
        GE_CORE_WARN("RadianceCascadesManager::calculateCascadeTransforms - Camera missing Controller or Transform component.");
        return; // No camera component or transform yet
    }

    const glm::mat4 projectionMatrix = camComponent->camera.getProjectionMatrix();
    const glm::mat4 viewMatrix = camComponent->camera.getViewMatrix();
    const glm::mat4 inverseProjectionMatrix = glm::inverse(projectionMatrix);
    const glm::mat4 inverseViewMatrix = glm::inverse(viewMatrix);
    const glm::mat4 invViewProj = inverseViewMatrix * inverseProjectionMatrix;
    const glm::vec3 cameraWorldPos = camComponent->translation;

    // --- Define NDC corners ---
    const std::vector<glm::vec4> ndcCorners = {
            // Near plane (z=-1 in NDC is typically *far* in projection, z=1 is near, but depends on convention)
            // Assuming standard OpenGL convention: z = -1 (near), z = 1 (far)
            // Near plane
            {-1.0f, -1.0f, -1.0f, 1.0f}, { 1.0f, -1.0f, -1.0f, 1.0f},
            { 1.0f,  1.0f, -1.0f, 1.0f}, {-1.0f,  1.0f, -1.0f, 1.0f},
            // Far plane
            {-1.0f, -1.0f,  1.0f, 1.0f}, { 1.0f, -1.0f,  1.0f, 1.0f},
            { 1.0f,  1.0f,  1.0f, 1.0f}, {-1.0f,  1.0f,  1.0f, 1.0f}
    };

    auto& cascades = m_hierarchy->getCascades(); // Get mutable reference if needed later
    const BuildParams& buildParams = m_hierarchy->getBuildParams(); // Get build params

    // --- Populate Shader Data for Each Cascade ---
    for (size_t i = 0; i < cascades.size(); ++i) {
        const RadianceCascade& cascade = cascades[i]; // Use const reference as we only read cascade params here
        RadianceCascadeShaderData& data = shaderData[i]; // Reference to element in shaderData vector

        // --- Calculate World-Space AABB for the Cascade Frustum Slice ---
        // Convert cascade range [start, end] (world distance from camera) to NDC Z range
        // Note: This assumes a standard perspective projection where depth is non-linear
        // Z_ndc = (C * Z_eye + D) / (-Z_eye) where C = P[2][2], D = P[3][2]
        // We need Z_eye (negative distance)
        float clipNear = camComponent->near_plane;
        float clipFar = camComponent->far_plane;
        float rangeStart = cascade.getIntervalStart();
        float rangeEnd = cascade.getIntervalEnd();

        // Clamp range to camera frustum, avoid division by zero/tiny values
        float viewZStart = -(glm::max)(rangeStart, 0.001f);
        float viewZEnd   = -(glm::max)(rangeEnd, 0.001f);

        // Calculate NDC Z values corresponding to the view space Z range
        // Be careful with division by zero if viewZ is near zero
        float ndcZStart = (viewZStart == 0.0f) ? -1.0f : (projectionMatrix[2][2] * viewZStart + projectionMatrix[3][2]) / (-viewZStart);
        float ndcZEnd   = (viewZEnd   == 0.0f) ?  1.0f : (projectionMatrix[2][2] * viewZEnd   + projectionMatrix[3][2]) / (-viewZEnd);

        // Clamp NDC Z values to [-1, 1] range
        ndcZStart = glm::clamp(ndcZStart, -1.0f, 1.0f);
        ndcZEnd = glm::clamp(ndcZEnd, -1.0f, 1.0f);

        glm::vec3 cascadeMinWorldPos(std::numeric_limits<float>::max());
        glm::vec3 cascadeMaxWorldPos(std::numeric_limits<float>::lowest());

        // Project the 8 corners of the frustum slice to world space
        for(int cornerIdx = 0; cornerIdx < 4; ++cornerIdx) {
             // Near slice face corners
             glm::vec4 startNdc = ndcCorners[cornerIdx]; startNdc.z = ndcZStart;
             glm::vec4 startWorldH = invViewProj * startNdc;
             if (startWorldH.w != 0.0f) {
                 glm::vec3 startWorld = glm::vec3(startWorldH) / startWorldH.w;
                 cascadeMinWorldPos = glm::min(cascadeMinWorldPos, startWorld);
                 cascadeMaxWorldPos = glm::max(cascadeMaxWorldPos, startWorld);
             } else { GE_CORE_WARN("Cascade {}: Near corner {} projection resulted in w=0", i, cornerIdx); }

             // Far slice face corners
             glm::vec4 endNdc = ndcCorners[cornerIdx + 4]; endNdc.z = ndcZEnd;
             glm::vec4 endWorldH = invViewProj * endNdc;
              if (endWorldH.w != 0.0f) {
                 glm::vec3 endWorld = glm::vec3(endWorldH) / endWorldH.w;
                 cascadeMinWorldPos = glm::min(cascadeMinWorldPos, endWorld);
                 cascadeMaxWorldPos = glm::max(cascadeMaxWorldPos, endWorld);
              } else { GE_CORE_WARN("Cascade {}: Far corner {} projection resulted in w=0", i, cornerIdx+4); }
        }

        glm::vec3 cascadeWorldSize = cascadeMaxWorldPos - cascadeMinWorldPos;
        const float minSize = 0.001f; // Prevent zero size AABBs
        cascadeWorldSize = glm::max(cascadeWorldSize, glm::vec3(minSize));

        // --- Construct worldToGridTransform Matrix ---
        // Transforms world position -> [0, 1] UVW within the cascade's AABB
        glm::vec3 scaleVec(1.0f / cascadeWorldSize.x, 1.0f / cascadeWorldSize.y, 1.0f / cascadeWorldSize.z);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scaleVec);
        glm::mat4 translateMat = glm::translate(glm::mat4(1.0f), -cascadeMinWorldPos);
        glm::mat4 worldToGridTransform = scaleMat * translateMat;

        // --- Calculate Grid Cell Size ---
        glm::ivec3 gridDimensions = cascade.getGridDimensions();
        glm::ivec2 gridDimensions2D = cascade.getGridDimensions2D();
        glm::vec3 gridSizeF = glm::vec3(glm::max(glm::ivec3(1), gridDimensions)); // Ensure dimensions are at least 1
        glm::vec3 gridCellSizeWorld = cascadeWorldSize / gridSizeF;

        // --- Calculate Atlas Information ---
        // This logic needs to match the RadianceCascade constructor's atlas calculation
        uint32_t totalProbes = static_cast<uint32_t>(gridDimensions2D.x) * static_cast<uint32_t>(gridDimensions2D.y);
        int angularResolution = cascade.getAngularResolution();
        glm::ivec2 atlasProbeGridDim = {0, 0};
        glm::ivec2 atlasPixelDim = {0, 0};
        if (totalProbes > 0 && angularResolution > 0) {
             uint32_t probesPerRow = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(totalProbes))));

             atlasProbeGridDim = gridDimensions2D;
             atlasPixelDim = { cascade.getAtlasTexture()->getWidth(), cascade.getAtlasTexture()->getHeight() };
        } else {
             GE_CORE_WARN("Cascade {}: Zero probes ({}) or angular resolution ({}), atlas dims set to 0.", i, totalProbes, angularResolution);
        }

       //GE_CORE_INFO("  Cascade {}: Atlas dimensions: ({}, {}, {})", i, cascadeMinWorldPos.x, cascadeMaxWorldPos.y, cascadeWorldSize.z);

        // --- Fill the ShaderData struct ---
        data.gridMinWorldPos = cascadeMinWorldPos;
        data.rangeStart = rangeStart;
        data.gridCellSizeWorld = gridCellSizeWorld;
        data.rangeEnd = rangeEnd;
        data.gridDimensions = gridDimensions;
        data.angularResolution = angularResolution;
        data.worldToGridTransform = worldToGridTransform; // Optional for SSR, but calculated

        data.gridDimensions2D = gridDimensions2D;

        data.atlasProbeGridDim = atlasProbeGridDim;
        data.atlasPixelDim = atlasPixelDim;

        data.inverseProjectionMatrix = inverseProjectionMatrix;
        data.inverseViewMatrix = inverseViewMatrix;
        data.projectionMatrix = projectionMatrix;
        data.viewMatrix = viewMatrix;
        data.cameraWorldPos = cameraWorldPos;

        data.numRayDirections = angularResolution * angularResolution; // Calculated from actual cascade res
        data.numStepsPerRay = buildParams.numStepsPerRay;
        data.jitterStrength = buildParams.jitterStrength;

        data.atlasTextureHandle = cascade.getAtlasTexture()->getTextureHandle();
    }
}


void RadianceCascadesManager::updateGpuBuffers() {
    RAPTURE_PROFILE_FUNCTION();

    size_t numCascades = m_hierarchy->getNumCascades();
    if (numCascades == 0) {
        GE_CORE_WARN("RadianceCascadesManager::updateGpuBuffers - No cascades to update.");
        return;
    }
    if (!m_cascadeInfoSSBO) {
         GE_CORE_ERROR("RadianceCascadesManager::updateGpuBuffers - CascadeInfo SSBO is null.");
         return;
    }

    std::vector<RadianceCascadeShaderData> shaderData(numCascades);

    // Calculate transforms based on current camera
    calculateCascadeTransforms(shaderData);

    // Upload the data to the SSBO
    m_cascadeInfoSSBO->setData(shaderData.data(), shaderData.size() * sizeof(RadianceCascadeShaderData));
}

std::shared_ptr<ShaderStorageBuffer> RadianceCascadesManager::getSSBO()
{
    return m_cascadeInfoSSBO;
}

std::shared_ptr<Shader> RadianceCascadesManager::getComputeShader()
{
    return m_computeShader;
}

std::shared_ptr<RadianceCascadeHierarchy> RadianceCascadesManager::getHierarchy()
{
    return m_hierarchy;
}

// Add dispatch function if needed, e.g.:
/*
void RadianceCascadesManager::dispatchComputeShader() {
    if (!m_computeShader || !m_cascadeInfoSSBO) {
        GE_CORE_ERROR("Cannot dispatch radiance cascade shader: Shader or SSBO not initialized.");
        return;
    }

    m_computeShader->bind();
    m_cascadeInfoSSBO->bind(0); // Bind SSBO to binding point 0

    // Bind G-Buffer textures (Assuming you have access to them here)
    // gBuffer.PositionDepthTexture->bind(3);
    // gBuffer.NormalTexture->bind(1);
    // gBuffer.DirectLightingTexture->bind(5);

    auto& cascades = m_hierarchy.getCascades();
    for (size_t i = 0; i < cascades.size(); ++i) {
        const RadianceCascade& cascade = cascades[i];

        // Bind the specific cascade's atlas texture as a writable image
        std::shared_ptr<Texture2D> atlasTexture = cascade.getAtlasTexture(); // Assuming this getter exists
        if (!atlasTexture) {
            GE_CORE_WARN("Cascade {} has no atlas texture, skipping dispatch.", i);
            continue;
        }
        atlasTexture->bindImage(6, 0, false, 0, TextureAccess::WriteOnly, TextureFormat::RGBA16F); // Bind to image unit 6

        // Set the uniform for the current cascade index
        m_computeShader->setInt("u_CurrentCascadeIndex", static_cast<int>(i));

        // Calculate dispatch size based on atlas dimensions
        glm::ivec2 atlasPixelDim = { atlasTexture->getWidth(), atlasTexture->getHeight() };
        if (atlasPixelDim.x == 0 || atlasPixelDim.y == 0) {
             GE_CORE_WARN("Cascade {} has zero dimensions ({}, {}), skipping dispatch.", i, atlasPixelDim.x, atlasPixelDim.y);
             continue;
        }

        const uint32_t localSizeX = 8; // Must match shader
        const uint32_t localSizeY = 8; // Must match shader
        const uint32_t localSizeZ = 1; // Must match shader
        uint32_t numGroupsX = (atlasPixelDim.x + localSizeX - 1) / localSizeX;
        uint32_t numGroupsY = (atlasPixelDim.y + localSizeY - 1) / localSizeY;
        uint32_t numGroupsZ = 1;

        // Dispatch compute shader
        glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);

        // Memory barrier needed? Between dispatches for the same image?
        // Or maybe one barrier after all dispatches?
        // For writing to the same image texture, need a barrier
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        atlasTexture->unbindImage(6); // Unbind image unit
    }

    // Unbind resources
    // gBuffer textures...
    m_cascadeInfoSSBO->unbind(0);
    m_computeShader->unbind();

    // Need a memory barrier before using the atlas textures for sampling elsewhere
     glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}
*/


} // namespace Rapture
