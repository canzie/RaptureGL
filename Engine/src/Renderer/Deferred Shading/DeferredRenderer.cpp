#include "DeferredRenderer.h"

#include "../../Debug/TracyProfiler.h"
#include "../../Logger/Log.h"
#include "../OpenGLRendererAPI.h"
#include "../Raycast.h"
#include "../../Scenes/Components/Components.h"

#include "../../Buffers/OpenGLBuffers/StorageBuffers/OpenGLSSBOBindingIndices.h"

#include "../../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../ShadowMapping/ShadowBufferLayouts.h"
#include "../PrimitiveShapes.h"
#include <glad/glad.h> // Ensure glad is included

#include "../RadianceCascades/RadianceCascades.h"
#include "../RadianceCascades/RadianceCascadesManager.h"

#include <cmath> // For std::ceil if needed, or use integer division trick

namespace Rapture
{


    std::filesystem::path s_shaderPath = "E:/Dev/Games/LiDAR Game v1/LiDAR-Game/Engine/src/Shaders/GLSL";

    // Define the static member variables
    std::shared_ptr<GBuffer> DeferredRenderer::s_gBuffer = nullptr;
    std::shared_ptr<Framebuffer> DeferredRenderer::s_lightingBuffer = nullptr;
    std::shared_ptr<Quad> DeferredRenderer::s_fullscreenQuad = nullptr;
    std::shared_ptr<Framebuffer> DeferredRenderer::s_indirectLightingBuffer = nullptr;

    std::shared_ptr<UniformBuffer> DeferredRenderer::s_cameraUBO = nullptr;
    std::shared_ptr<UniformBuffer> DeferredRenderer::s_lightsUBO = nullptr;
    std::shared_ptr<ShaderStorageBuffer> DeferredRenderer::s_shadowSSBO = nullptr;

    // Initialize light cache members
    bool DeferredRenderer::s_lightsDirty = true;
    std::vector<entt::entity> DeferredRenderer::s_cachedLightEntities;
    uint32_t DeferredRenderer::s_cachedLightCount = 0;

    std::weak_ptr<Shader> DeferredRenderer::s_lightingPassShader;
    AssetHandle DeferredRenderer::s_lightingPassShaderHandle;

    std::weak_ptr<Shader> DeferredRenderer::s_indirectLightingPassShader;
    AssetHandle DeferredRenderer::s_indirectLightingPassShaderHandle;

    // Shadow mapping static members
    bool DeferredRenderer::s_shadowMapDirty = true;
    bool DeferredRenderer::s_isShadowPass = false;
    std::shared_ptr<ShadowMapBase> DeferredRenderer::s_currentShadowMap = nullptr;

    glm::vec3 DeferredRenderer::s_cameraPosition = glm::vec3(0.0f);
    glm::mat4 DeferredRenderer::s_cameraViewMatrixCache = glm::mat4(1.0f);

    BoundFramebufferType DeferredRenderer::s_currentFramebufferType = BoundFramebufferType::NONE;

    void DeferredRenderer::init()
    {
		RAPTURE_PROFILE_FUNCTION();
		GE_RENDER_INFO("Renderer: Initializing renderer");

		Raycast::init();
		
		// Initialize the CommandQueueBuilder with a thread pool
		CommandQueueBuilder::init(4); // Create 2 worker threads by default
		//RadianceCascades::init();
        auto settings = BuildParams();
        
        // Initialize G-buffer with current window size
        uint32_t width = 1280;
        uint32_t height = 720;
        
        s_gBuffer = std::make_shared<GBuffer>(width, height, true);
        
        // Create framebuffer for lighting pass
        FramebufferSpecification lightingBufferSpec;
        lightingBufferSpec.width = width;
        lightingBufferSpec.height = height;
        // Ensure depth attachment for lighting buffer
        lightingBufferSpec.attachments = { 
            FramebufferTextureFormat::RGBA16F, // Color
            FramebufferTextureFormat::Depth    // Depth 
        };
        s_lightingBuffer = Framebuffer::create(lightingBufferSpec);

        s_indirectLightingBuffer = Framebuffer::create(lightingBufferSpec);

        auto [indShader, indHandle] = AssetManager::importAsset<Shader>(s_shaderPath / "RadianceCascadingCS/indirectLightingPass.vs.glsl");
        s_indirectLightingPassShader = indShader;
        s_indirectLightingPassShaderHandle = indHandle;

        
        
        s_cameraUBO = std::make_shared<UniformBuffer>(sizeof(CameraUniform), BufferUsage::Stream, nullptr, BASE_BINDING_POINT_IDX);
        s_lightsUBO = std::make_shared<UniformBuffer>(sizeof(LightsUniform), BufferUsage::Stream, nullptr, LIGHTS_BINDING_POINT_IDX);
        s_shadowSSBO = std::make_shared<ShaderStorageBuffer>(sizeof(ShadowStorageLayout), BufferUsage::Stream, nullptr);
        // Load deferred shaders
        auto [shader, handle] = AssetManager::importAsset<Shader>(s_shaderPath / "DeferredLightingPass.vert.glsl");
        s_lightingPassShader = shader;
        s_lightingPassShaderHandle = handle;

        setupFullscreenQuad();

        //auto hierarchy = RadianceCascadeHierarchy();
        
        //hierarchy.buildCascades(settings);



        GE_RENDER_INFO("Deferred rendering initialized");
    }


    void DeferredRenderer::shutdown()
    {
		Raycast::shutdown();

		// Shutdown worker threads first to prevent accessing released resources
		CommandQueueBuilder::shutdownWorkers();
        //RadianceCascades::shutdown();

        // Clean up resources
        s_gBuffer.reset();
        s_lightingBuffer.reset();
        s_cameraUBO.reset();
        s_lightsUBO.reset();
        s_shadowSSBO.reset();
        s_lightingPassShader.reset();
        s_lightingPassShaderHandle = 0;

        s_indirectLightingPassShader.reset();
        s_indirectLightingPassShaderHandle = 0;
        s_indirectLightingBuffer.reset();

        // Reset cache flags
        s_lightsDirty = true;
        s_shadowMapDirty = true;
        s_cachedLightEntities.clear();
        s_cachedLightCount = 0;

    }

    void DeferredRenderer::onFrameBegin()
    {
    }

    void DeferredRenderer::onFrameEnd()
    {

    }


    // TODO: make the binding and unbinding of the gbuffer better by storing a flag to indicate if the gbuffer is bound, so it does not have ot be done over and over
    
    void DeferredRenderer::renderQueueAsync(std::shared_ptr<RenderQueue> queue)
    {
        RAPTURE_PROFILE_GPU_SCOPE("Executing Async RenderQueue");
        RAPTURE_PROFILE_SCOPE("Executing Async RenderQueue");

        // Process commands until the queue is both empty and marked as done
        while (!queue->isDone()) {
            // Try to get a command, process it if available
            CommandVariant cmd;
            if (queue->tryServe(cmd)) {
                if (std::holds_alternative<RenderCommand>(cmd)) {
                    if (s_currentFramebufferType == BoundFramebufferType::NONE) {
                        s_currentFramebufferType = BoundFramebufferType::GBUFFER;
                        s_gBuffer->bind();
                    }
                    geometryPassRender(std::get<RenderCommand>(cmd));
                } 
                else if (std::holds_alternative<ShadowPassCommand>(cmd)) {
                    auto shadowPassCmd = std::get<ShadowPassCommand>(cmd);
                    shadowPassRender(shadowPassCmd);

                }
                else if (std::holds_alternative<LightingPassCommand>(cmd)) {
                    // Unbind G-buffer as render target
                    if (s_currentFramebufferType == BoundFramebufferType::GBUFFER) {
                        s_gBuffer->unbind();
                        s_currentFramebufferType = BoundFramebufferType::NONE;
                    }

                    copyDepthBuffer2LightingBuffer();

                    // Now execute the lighting pass which will bind s_lightingBuffer
                    lightingPassRender(std::get<LightingPassCommand>(cmd));

                } else if (std::holds_alternative<SSRCommand>(cmd)) {

                } else if (std::holds_alternative<RadianceCascadesCommand>(cmd)) {
                    radianceCascadesCompute(std::get<RadianceCascadesCommand>(cmd));
                } else if (std::holds_alternative<IndirectLightingPassCommand>(cmd)) {
                    indirectLightingPassRender(std::get<IndirectLightingPassCommand>(cmd));
                } else {
                    GE_RENDER_ERROR("Unknown command type in deferred queue");
                }
            }
        }

        if (s_currentFramebufferType != BoundFramebufferType::NONE) {
            s_currentFramebufferType = BoundFramebufferType::NONE;
            //GE_RENDER_WARN("Framebuffer not unbound before exit");
        }
    }

    void DeferredRenderer::sumbitScene(const std::shared_ptr<Scene> s)
    {
        RAPTURE_PROFILE_FUNCTION();
        RAPTURE_PROFILE_GPU_SCOPE("DeferredRenderer::GeometryPass");

		// Setup camera uniforms and get camera position for shaders
		{
			RAPTURE_PROFILE_SCOPE("Camera Setup");
            //auto& reg = s->getRegistry();
            //auto cams = reg.view<CameraControllerComponent>();
            auto mainCamera = s->getMainCamera();

            //Entity camera_ent(cams.front(), s.get());
            CameraControllerComponent& controller_comp = mainCamera->getComponent<CameraControllerComponent>();

            // Get projection and view matrices
            const glm::mat4& projMat = controller_comp.camera.getProjectionMatrix();
            const glm::mat4& viewMat = controller_comp.camera.getViewMatrix();

            s_cameraViewMatrixCache = projMat*viewMat;

            controller_comp.frustum.update(projMat, viewMat);
            

            s_cameraPosition = controller_comp.translation;

			CameraUniform cameraData;
			cameraData.projection_mat = projMat;
			cameraData.view_mat = viewMat;

				
			s_cameraUBO->setData(&cameraData, sizeof(CameraUniform));
		}



        // Setup lights (uses caching)
        setupLightsUniforms(s);

        // Update shadow matrices for all lights with shadow maps
        updateShadowMatrix(s);



        // the 2 queues should be made at the same time, this only happens when they are called after each other,
        // the other methods are no async so they would be blocking the main thread, so it cannot ask for the other queue
        auto geometryQueue = CommandQueueBuilder::buildGeometryCommandQueueAsync(s, RenderQueueType::DEFERRED);

        std::shared_ptr<RenderQueue> shadowQueue = nullptr;
        if (s_shadowMapDirty) {
            shadowQueue = CommandQueueBuilder::buildGeometryCommandQueueAsync(s, RenderQueueType::SHADOWMAP);
            renderQueueAsync(shadowQueue);

            //s_shadowMapDirty = false;
        }

        renderQueueAsync(geometryQueue);

    }

    // TODO: Dogshit, needs to be giga optimized
    void DeferredRenderer::updateShadowMatrix(const std::shared_ptr<Scene>& scene)
    {
        RAPTURE_PROFILE_FUNCTION();
        
        // Skip update if shadow map isn't dirty
        if (!s_shadowMapDirty) {
            return;
        }

        ShadowStorageLayout shadowLayout;
        shadowLayout.shadowCount = 0;
        
        auto& reg = scene->getRegistry();
        // Get all entities with both LightComponent and ShadowComponent
        auto shadowLightView = reg.view<LightComponent, TransformComponent, ShadowComponent>();
        
        uint32_t shadowCount = 0;
        
        // First pass: find all lights with shadow components and create a mapping
        std::unordered_map<entt::entity, uint32_t> lightIndexMap;
        auto lightView = reg.view<LightComponent>();
        uint32_t lightIdx = 0;
        
        for (auto entityID : lightView) {
            LightComponent& light = lightView.get<LightComponent>(entityID);
            if (light.isActive) {
                lightIndexMap[entityID] = lightIdx++;
            }
        }
        
        // Second pass: populate shadow data with correct light indices
        for (auto entityID : shadowLightView) {
            if (shadowCount >= MAX_SHADOW_CASTERS) {
                GE_RENDER_WARN("Maximum shadow maps ({0}) exceeded", MAX_SHADOW_CASTERS);
                break;
            }
            
            Entity lightEntity(entityID, scene.get());
            LightComponent& light = shadowLightView.get<LightComponent>(entityID);
            TransformComponent& transform = shadowLightView.get<TransformComponent>(entityID);
            ShadowComponent& shadowComp = shadowLightView.get<ShadowComponent>(entityID);
            
            // Skip inactive lights or shadows
            if (!light.isActive || !light.castsShadow || !shadowComp.isActive) {
                continue;
            }
            
            // Get the shadow map from the component
            std::shared_ptr<ShadowMap> shadowMap = shadowComp.shadowMap;
            if (!shadowMap) {
                GE_RENDER_ERROR("Failed to get shadow map for entity {0}", lightEntity.getID());
                continue;
            }

            // Calculate light position and direction
            glm::vec3 lightPos = transform.translation();
            glm::vec3 lightDir;
            
            // Calculate light direction based on light type
            if (light.type == LightType::Directional) {
                // Calculate light direction from rotation
                glm::quat rotationQuat = transform.transforms.getRotationQuat();
                lightDir = glm::normalize(rotationQuat * glm::vec3(0, 0, -1)); // Forward vector
            } 
            else if (light.type == LightType::Spot) {
                // Calculate spot light direction
                glm::quat rotationQuat = transform.transforms.getRotationQuat();
                lightDir = glm::normalize(rotationQuat * glm::vec3(0, 0, -1)); // Forward vector
            } 
            else {
                // Point light - use default direction
                lightDir = glm::vec3(0.0f, -1.0f, 0.0f); // Down direction
            }
        
        // Calculate light view matrix
        glm::vec3 lightUp = glm::vec3(0.0f, 1.0f, 0.0f);
        if (abs(glm::dot(lightDir, lightUp)) > 0.99f) {
            // If light is pointing directly up or down, use a different up vector
            lightUp = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        
        // Create the light's view matrix
        glm::mat4 viewMatrix = glm::lookAt(
            lightPos,               // Light position
            lightPos + lightDir,    // Look at center (Point along the light direction)
            lightUp                 // Up vector
        );
        
            // Calculate projection matrix based on light type
        glm::mat4 lightProj(1.0f);
        
        if (light.type == LightType::Directional) {
            // Get scene bounds or focus on camera frustum
            glm::vec3 sceneCenter = s_cameraPosition; // Use camera position as center point
            float sceneBounds = 50.0f; // Start with a reasonable size based on your scene scale
            
            // Position the light based on scene center and direction
            glm::vec3 shadowCamPos = sceneCenter - lightDir * (sceneBounds * 0.5f);
            
            // Create view matrix centered on the scene, not on the light entity
            viewMatrix = glm::lookAt(
                shadowCamPos,          // Position light relative to scene center
                sceneCenter,           // Look at scene center
                lightUp                // Up vector
            );
            
            // Use appropriate size for your scene
            float orthoSize = sceneBounds;
            // Use near/far planes that encompass your entire scene
            lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, sceneBounds * 2.0f);
        } else {
            // Perspective projection for spot/point light
            float aspect = 1.0f; // Shadow map is square
            
            if (light.type == LightType::Spot) {
                // For spotlights, use more aggressive near plane scaling
                float nearPlane = glm::max(0.1f, light.range * 0.001f); // Much closer near plane
                float farPlane = light.range * 1.2f; // Extend beyond light range for better coverage
                
                // Use slightly wider angle for shadows to avoid edge artifacts
                float shadowConeAngle = light.outerConeAngle * 1.1f; // 10% wider angle for shadows
                float fovRadians = glm::max(shadowConeAngle * 2.0f, glm::radians(5.0f));
                
                lightProj = glm::perspective(fovRadians, aspect, nearPlane, farPlane);
            } else {
                // Point light settings (default)
                float nearPlane = 0.1f;
                lightProj = glm::perspective(glm::radians(90.0f), aspect, nearPlane, light.range);
            }
        }

            // Combine for final light space matrix and set it in the shadow map
            glm::mat4 lightViewProj = lightProj * viewMatrix;
            shadowMap->setWVPMatrix(lightViewProj);

            // Update the frustum
            shadowComp.updateFrustum(viewMatrix, lightProj);
            
            // Store shadow data for the shader
            ShadowBufferData& shadowData = shadowLayout.shadowData[shadowCount];
            shadowData.type = static_cast<int>(light.type);
            shadowData.cascadeCount = 1;
            shadowData.textureIDs[0] = shadowMap->getShadowMapHandle();
            shadowData.cascadeMatrices[0] = lightViewProj;
            shadowData.cascadeSplitsViewSpace[0] = glm::vec4(0.0f);

            
            // Store the light index this shadow map belongs to
            if (lightIndexMap.find(entityID) != lightIndexMap.end()) {
                shadowData.lightIndex = lightIndexMap[entityID];
            } else {
                GE_RENDER_WARN("Light entity not found in index map");
                shadowData.lightIndex = 0; // Default to first light if not found
            }
            
            shadowCount++;
        }
        
        // Third pass: Process entities with CascadedShadowComponent
        auto csmLightView = reg.view<LightComponent, TransformComponent, CascadedShadowComponent>();
        
        // Get the main camera data for CSM calculations
        auto mainCamera = scene->getMainCamera();
        if (!mainCamera) {
            GE_RENDER_WARN("No main camera found for CSM calculations");
        }
        else {
            CameraControllerComponent& cameraComp = mainCamera->getComponent<CameraControllerComponent>();
            const glm::mat4& cameraViewMatrix = cameraComp.camera.getViewMatrix();
            const glm::mat4& cameraProjMatrix = cameraComp.camera.getProjectionMatrix();
            float cameraNearPlane = cameraComp.near_plane;
            float cameraFarPlane = cameraComp.far_plane;
            
            for (auto entityID : csmLightView) {
                if (shadowCount >= MAX_SHADOW_CASTERS) {
                    GE_RENDER_WARN("Maximum shadow maps ({0}) exceeded", MAX_SHADOW_CASTERS);
                    break;
                }
                
                Entity lightEntity(entityID, scene.get());
                LightComponent& light = csmLightView.get<LightComponent>(entityID);
                TransformComponent& transform = csmLightView.get<TransformComponent>(entityID);
                CascadedShadowComponent& csmComp = csmLightView.get<CascadedShadowComponent>(entityID);
                
                // Skip inactive lights or shadows
                if (!light.isActive || !light.castsShadow || !csmComp.isActive) {
                    continue;
                }
                
                // CSM only supports directional lights
                if (light.type != LightType::Directional) {
                    GE_RENDER_WARN("Cascaded shadow mapping only supports directional lights");
                    continue;
                }
                
                // Get the CSM from the component
                std::shared_ptr<CascadedShadowMapping> csmMap = csmComp.cascadedShadowMapping;
                if (!csmMap) {
                    GE_RENDER_ERROR("Failed to get cascaded shadow map for entity {0}", lightEntity.getID());
                    continue;
                }
                
                // Calculate light direction
                glm::quat rotationQuat = transform.transforms.getRotationQuat();
                glm::vec3 lightDir = glm::normalize(rotationQuat * glm::vec3(0, 0, -1)); // Forward vector
                
                // Use the CSM's calculateCascades method to compute all cascade matrices
                auto cascadeData = csmMap->calculateCascades(
                    lightDir,
                    cameraViewMatrix,
                    cameraProjMatrix,
                    cameraNearPlane,
                    cameraFarPlane,
                    ProjectionType::Perspective // Assuming perspective camera
                );

                //csmComp.updateFrustum();

                
                // Store CSM data for the shader
                ShadowBufferData& shadowData = shadowLayout.shadowData[shadowCount];
                shadowData.type = static_cast<int>(light.type);
                shadowData.cascadeCount = 4;
                
                // Check if the shadow map uses a texture array
                if (csmMap->getShadowMap()->hasDepthTextureArray()) {
                    // For texture arrays, we only need to store the handle once
                    // All cascades are part of the same texture array
                    uint64_t depthArrayHandle = csmMap->getShadowMap()->getDepthTextureArrayHandle();
                    
                    if (depthArrayHandle == 0) {
                        GE_RENDER_ERROR("CSM depth texture array handle is invalid");
                    } else {
                        // Store the same texture handle for all cascades
                        // The shader will use gl_Layer or array index to access the right slice
                        shadowData.textureIDs[0] = depthArrayHandle;
                        
                        // Mark this as a texture array by setting a special flag in the last component
                        // -1.0 in w component of first cascade split indicates texture array
                        shadowData.cascadeSplitsViewSpace[0].w = -1.0f;
                        
                    }
                } else {
                    // Legacy mode: Get individual texture handles for each cascade
                    auto cascadeTextureHandles = csmMap->getCascadeTextureHandles();
                    
                    // Store separate texture handles for each cascade
                    for (uint32_t i = 0; i < shadowData.cascadeCount && i < MAX_CASCADES; i++) {
                        shadowData.textureIDs[i] = cascadeTextureHandles[i];
                    }
                }

                // Store each cascade's data
                for (uint32_t i = 0; i < shadowData.cascadeCount && i < MAX_CASCADES; i++) {
                    // Set the view-projection matrix for this cascade
                    shadowData.cascadeMatrices[i] = cascadeData[i].lightViewProj;
                    
                    // Store cascade split depth in view space (xyz components)
                    shadowData.cascadeSplitsViewSpace[i].x = cascadeData[i].farPlane;
                }
                
                // Store the light index this shadow map belongs to
                if (lightIndexMap.find(entityID) != lightIndexMap.end()) {
                    shadowData.lightIndex = lightIndexMap[entityID];
                } else {
                    GE_RENDER_WARN("Light entity not found in index map");
                    shadowData.lightIndex = 0; // Default to first light if not found
                }
                
                shadowCount++;
            }
        }
        
        shadowLayout.shadowCount = shadowCount;
        
        // Update the shadow storage buffer
        s_shadowSSBO->setData(&shadowLayout, sizeof(ShadowStorageLayout));
    }

    void DeferredRenderer::copyDepthBuffer2LightingBuffer()
    {
        RAPTURE_PROFILE_SCOPE("Blit Depth Buffer");

        if (!s_gBuffer || !s_lightingBuffer) {
            GE_RENDER_ERROR("DeferredRenderer::copyDepthBuffer2LightingBuffer - GBuffer or LightingBuffer is null");
            return;
        }

        // Assuming GBuffer and Framebuffer have getFramebufferID(), getWidth(), getHeight()
        uint32_t gBufferFBO = s_gBuffer->getFramebufferID(); // Need getFramebufferID() on GBuffer
        uint32_t lightingFBO = s_lightingBuffer->getFramebufferID(); // Need getFramebufferID() on Framebuffer
        uint32_t width = s_gBuffer->getSpecification().width; // Need getWidth() on GBuffer
        uint32_t height = s_gBuffer->getSpecification().height; // Need getHeight() on GBuffer

        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBufferFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lightingFBO);
        glBlitFramebuffer(0, 0, width, height,
                            0, 0, width, height,
                            GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        // Unbind framebuffers (or let subsequent binds handle it)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); 
                    
    }

    void DeferredRenderer::geometryPassRender(const RenderCommand &cmd)
    {
        RAPTURE_PROFILE_GPU_SCOPE("Executing geometryPassRender");
        RAPTURE_PROFILE_SCOPE("Executing geometryPassRender");
        
        // Use const references to avoid copies
        const auto& mesh = cmd.mesh;
        const auto& material = cmd.material;
        const auto& localTransform = cmd.transform;
        const auto& meshData = mesh->getMeshData();

        if (!mesh || !material) {
            GE_RENDER_ERROR("GeometryPassRender: Mesh or material is null");
            return;
        }

        meshData.vao->bind();
        
        if (s_isShadowPass) {
            // Use shadow map shader for shadow pass
            

            
            // Calculate and set model-view-projection matrix for this object
            // The light WVP (view and projection) is already set in the shadow map
            
            // We just need to multiply by the model matrix here
            s_currentShadowMap->setShaderUniforms(localTransform);
            
            // Additional parameters for skinning if needed
            auto shader = s_currentShadowMap->getShader();
            if (!shader) {
                GE_RENDER_ERROR("Shadow pass shader is null");
                meshData.vao->unbind();
                return;
            }
            shader->setFloat("u_SkinningEnabled", cmd.isSkeletal ? 1.0f : 0.0f);
            
            {
                RAPTURE_PROFILE_SCOPE("Draw Call");
                RAPTURE_PROFILE_GPU_SCOPE("Draw Call");
                
                // Draw shadow map
                OpenGLRendererAPI::drawIndexed(meshData.indexCount, meshData.indexType, 
                    meshData.indexAllocation->offsetBytes, meshData.vertexOffsetInVertices);
            }


        }
        else {
            // Normal geometry pass
            material->bind(ShaderRenderPassType::GEOMETRY);
            
            std::shared_ptr<Shader> const shdr = material->getGeometryPassShader();
            if (!shdr) {
                GE_RENDER_ERROR("GeometryPassRender: Geometry pass shader is null");
                meshData.vao->unbind();
                material->unbind();
                return;
            }

            shdr->setFloat("u_SkinningEnabled", cmd.isSkeletal ? 1.0f : 0.0f);
            shdr->setMat4("u_model", localTransform);
            {
                RAPTURE_PROFILE_SCOPE("Draw Call");
                RAPTURE_PROFILE_GPU_SCOPE("Draw Call");
                // Single draw call - avoid function call overhead by directly accessing members
                OpenGLRendererAPI::drawIndexed(meshData.indexCount, meshData.indexType, 
                    meshData.indexAllocation->offsetBytes, meshData.vertexOffsetInVertices);
            }

            // Clean state once
            material->unbind();
        }
        
        meshData.vao->unbind();
    }

    // lightningpasscommand is empty for now, as it is better to store the shader in the deferredrenderer class
    // could still be used for specific lighting passes variables later
    void DeferredRenderer::lightingPassRender(const LightingPassCommand &cmd)
    {
        RAPTURE_PROFILE_FUNCTION();
        RAPTURE_PROFILE_GPU_SCOPE("LightingPassRender");
        
        std::shared_ptr<Shader> boundShader = nullptr;
        {
            RAPTURE_PROFILE_SCOPE("Shader Setup & Uniforms");

            if (auto shader = s_lightingPassShader.lock()) {
                shader->bind();
                boundShader = shader;
            } else {
                shader = AssetManager::getAsset<Shader>(s_lightingPassShaderHandle);
                if (shader) {
                    s_lightingPassShader = shader;
                    shader->bind();
                    boundShader = shader;
                } else {
                    GE_RENDER_ERROR("DeferredRenderer::LightingPassRender - Shader is null");
                    return;
                }
            }



            // Set Camera Position uniform
            if (boundShader) {
                boundShader->setVec3("u_CameraPosition", s_cameraPosition);
            }

        
            s_lightingBuffer->bind(false);
            s_currentFramebufferType = BoundFramebufferType::LIGHTING_BUFFER;
		    
            // will be enabled next time we bind again, should also never be called when not binded
            s_lightingBuffer->disableDepthTesting();

        }

        {
            RAPTURE_PROFILE_SCOPE("Bind G-Buffer Textures");
            s_gBuffer->bindTextures();
        }

        {
            RAPTURE_PROFILE_SCOPE("wait for Shadow SSBO");
            s_shadowSSBO->barrier();
            //s_shadowSSBO->bind();
        }
        s_shadowSSBO->bindBase(0);

        {
            RAPTURE_PROFILE_SCOPE("Lighting Pass");
            // TODO: Implement lighting pass specifics (e.g., setting light uniforms)
            renderFullscreenQuad(boundShader); // Render the quad to apply lighting shader
        }


        {
            RAPTURE_PROFILE_SCOPE("Unbind G-Buffer Textures");
            s_gBuffer->unbindTextures();
        }


        {
            RAPTURE_PROFILE_SCOPE("Unbind Shader");
            // Use the captured boundShader pointer to unbind
            if (boundShader) {
                boundShader->unBind();
            } else {
                GE_RENDER_WARN("DeferredRenderer::LightingPassRender - Shader changed during lighting pass, unable to unbind the bound shader");
            }
        }

        s_lightingBuffer->unbind();
        s_currentFramebufferType = BoundFramebufferType::NONE;
    }
    
    inline void DeferredRenderer::shadowPassRender(const ShadowPassCommand &cmd)
    {
        RAPTURE_PROFILE_FUNCTION();
        RAPTURE_PROFILE_GPU_SCOPE("ShadowPassRender");

        auto shadowMapVariant = cmd.shadowMap;

        if (cmd.commandType == CommandExectionPhase::NONE) {
            GE_CORE_ERROR("ShadowPassRender: Command type is NONE");
            return;
        }

        if (std::holds_alternative<std::shared_ptr<ShadowMap>>(shadowMapVariant)) {
            auto shadowMap = std::get<std::shared_ptr<ShadowMap>>(shadowMapVariant);

            if (cmd.commandType == CommandExectionPhase::BEGIN_PASS) {
                shadowMap->bind();
                s_currentShadowMap = shadowMap; // Set the current shadow map for use in the geometry pass
                s_isShadowPass = true;  
                s_currentFramebufferType = BoundFramebufferType::SHADOW_MAP;
            }
            else if (cmd.commandType == CommandExectionPhase::END_PASS) {
                shadowMap->unbind();
                s_currentShadowMap = nullptr; // Clear the current shadow map reference
                s_isShadowPass = false;
                s_currentFramebufferType = BoundFramebufferType::NONE;
            }
        }
        else if (std::holds_alternative<std::shared_ptr<CascadedShadowMapping>>(shadowMapVariant)) {
            auto shadowMap = std::get<std::shared_ptr<CascadedShadowMapping>>(shadowMapVariant);
            
            if (!shadowMap) {
                GE_CORE_ERROR("ShadowPassRender: CascadedShadowMapping pointer is null");
                return;
            }


            if (cmd.commandType == CommandExectionPhase::BEGIN_PASS) {
                shadowMap->bind();
                s_currentShadowMap = shadowMap; // Set the current shadow map for use in the geometry pass
                s_isShadowPass = true;  
                s_currentFramebufferType = BoundFramebufferType::SHADOW_MAP;
            }
            else if (cmd.commandType == CommandExectionPhase::END_PASS) {
                shadowMap->unbind();
                s_currentShadowMap = nullptr; // Clear the current shadow map reference
                s_isShadowPass = false;
                s_currentFramebufferType = BoundFramebufferType::NONE;
            }        
        }
        else {
            GE_CORE_ERROR("ShadowPassRender: Unhandled shadow map variant type");
        }
    }
    inline void DeferredRenderer::radianceCascadesCompute(const RadianceCascadesCommand & cmd)
    {
        
        RAPTURE_PROFILE_FUNCTION();
        RAPTURE_PROFILE_GPU_SCOPE("RadianceCascadesCompute_MultiDispatch");

        // --- Basic Validation ---
        if (!cmd.radianceCascadesShader) {
            GE_RENDER_ERROR("DeferredRenderer::radianceCascadesCompute - Shader is null");
            return;
        }

        if (!cmd.cascadeHierarchy) {
            GE_RENDER_ERROR("DeferredRenderer::radianceCascadesCompute - Cascade hierarchy is null");
            return;
        }
        if (!cmd.cascadeSSBO) {
            GE_RENDER_ERROR("DeferredRenderer::radianceCascadesCompute - Cascade SSBO is null");
            return;
        }

        cmd.radianceCascadesShader->bind();
        cmd.cascadeSSBO->bindBase(0); // Bind SSBO to binding point 0


        //s_gBuffer->bindTexturesCompute();
        s_gBuffer->bindTextures();
        s_lightingBuffer->bindTextures(5);



        // Consider adding a try-catch block for extra safety during debugging
        auto& cascades = cmd.cascadeHierarchy->getCascades();
        for (size_t i = 0; i < cascades.size(); ++i) {
            const RadianceCascade& cascade = cascades[i];

            // Bind the specific cascade's atlas texture as a writable image
            std::shared_ptr<Texture2D> atlasTexture = cascade.getAtlasTexture(); // Assuming this getter exists
            if (!atlasTexture) {
                GE_CORE_WARN("Cascade {} has no atlas texture, skipping dispatch.", i);
                continue;
            }

            atlasTexture->bindCompute(6);

            // Set the uniform for the current cascade index
            cmd.radianceCascadesShader->setInt("u_CurrentCascadeIndex", static_cast<int>(i));
            cmd.radianceCascadesShader->setInt("u_screenDimensionsX", static_cast<int>(s_gBuffer->getSpecification().width));
            cmd.radianceCascadesShader->setInt("u_screenDimensionsY", static_cast<int>(s_gBuffer->getSpecification().height));

            // Calculate dispatch size based on atlas dimensions
            glm::ivec2 atlasPixelDim = { atlasTexture->getWidth(), atlasTexture->getHeight() };
            if (atlasPixelDim.x == 0 || atlasPixelDim.y == 0) {
                GE_CORE_WARN("Cascade {} has zero dimensions ({}, {}), skipping dispatch.", i, atlasPixelDim.x, atlasPixelDim.y);
                continue;
            }

            const uint32_t localSizeX = 16; // Must match shader
            const uint32_t localSizeY = 16; // Must match shader
            const uint32_t localSizeZ = 1; // Must match shader
            uint32_t numGroupsX = (atlasPixelDim.x + localSizeX - 1) / localSizeX;
            uint32_t numGroupsY = (atlasPixelDim.y + localSizeY - 1) / localSizeY;
            uint32_t numGroupsZ = 1;

            // Dispatch compute shader
            cmd.radianceCascadesShader->dispatchCompute(numGroupsX, numGroupsY, numGroupsZ);

            // Memory barrier needed? Between dispatches for the same image?
            // Or maybe one barrier after all dispatches?
            // For writing to the same image texture, need a barrier

        }

        // Unbind resources
        // gBuffer textures...
        cmd.radianceCascadesShader->unBind();
        s_gBuffer->unbindTextures();
        // Need a memory barrier before using the atlas textures for sampling elsewhere
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        
    }

    inline void DeferredRenderer::indirectLightingPassRender(const IndirectLightingPassCommand &cmd)
    {
        RAPTURE_PROFILE_FUNCTION();
        RAPTURE_PROFILE_GPU_SCOPE("IndirectLightingPassRender");


        auto shader = s_indirectLightingPassShader.lock();

        if (!shader) {
            GE_RENDER_ERROR("DeferredRenderer::indirectLightingPassRender - Shader is null");
            return;
        }

        shader->bind();

        s_indirectLightingBuffer->bind();
        s_gBuffer->bindTextures();

        auto hierarchy = cmd.cascadeHierarchy;
        auto ssbo = cmd.cascadeSSBO;


        ssbo->bindBase(0);
        


        shader->setInt("u_NumCascades", static_cast<int>(hierarchy->getNumCascades()));
        shader->setInt("u_screenDimensionsX", static_cast<int>(s_gBuffer->getSpecification().width));
        shader->setInt("u_screenDimensionsY", static_cast<int>(s_gBuffer->getSpecification().height));
        shader->setVec3("u_CameraPosition", s_cameraPosition);

        ssbo->barrier();

        renderFullscreenQuad(shader);

        s_gBuffer->unbindTextures();
        shader->unBind();
        s_indirectLightingBuffer->unbind();
        s_currentFramebufferType = BoundFramebufferType::NONE;
    }

    void DeferredRenderer::setupFullscreenQuad()
    {
        RAPTURE_PROFILE_FUNCTION();
        // Create a simple Quad covering the full screen in Normalized Device Coordinates (NDC)
        // Position (-1, -1), Scale (2, 2)
        // No rotation, default color (often ignored in shader), no texture by default
        PrimitiveConfig quadConfig;
        quadConfig.position = glm::vec3(0.0f, 0.0f, 0.0f); // Bottom-left corner in NDC
        quadConfig.scale = glm::vec3(2.0f, 2.0f, 1.0f); // Spans the full screen (width 2, height 2)
        quadConfig.useTexCoords = true; // Ensure texture coordinates are generated
        quadConfig.createDefaultMaterial = false; // We don't need the default material

        s_fullscreenQuad = std::make_shared<Quad>(quadConfig);

        // Optional: If the quad needs a specific material for the lighting pass (though often not needed
        // as the lighting shader samples the GBuffer), you could create and set it here.
        // For now, we assume the lighting shader handles everything.
    }

    void DeferredRenderer::renderFullscreenQuad(std::shared_ptr<Shader> shader){
        RAPTURE_PROFILE_FUNCTION();
        RAPTURE_PROFILE_GPU_SCOPE("RenderFullscreenQuad");

        if (!s_fullscreenQuad || !s_fullscreenQuad->getMesh()) {
            GE_RENDER_ERROR("DeferredRenderer::renderFullscreenQuad - Fullscreen quad or its mesh is null");
            return;
        }

        // The lighting pass shader should already be bound from lightingPassRender
        // We just need to bind the quad's VAO and draw it.

        auto meshData = s_fullscreenQuad->getMesh()->getMeshData();
        meshData.vao->bind();
        
        glm::mat4 modelMatrix = glm::mat4(1.0f);

        // Apply translation, rotation, and scale
        modelMatrix = glm::translate(modelMatrix, s_fullscreenQuad->getPosition());
            
        // Apply rotation (X, Y, Z order)
        modelMatrix = glm::rotate(modelMatrix, glm::radians(s_fullscreenQuad->getRotation().x), glm::vec3(1.0f, 0.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(s_fullscreenQuad->getRotation().y), glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(s_fullscreenQuad->getRotation().z), glm::vec3(0.0f, 0.0f, 1.0f));
            
        modelMatrix = glm::scale(modelMatrix, s_fullscreenQuad->getScale());

    
            

        shader->setMat4("u_model", modelMatrix);
        shader->setMat4("u_cameraViewMatrix", s_cameraViewMatrixCache);

        {
            RAPTURE_PROFILE_SCOPE("Draw Fullscreen Quad");
            RAPTURE_PROFILE_GPU_SCOPE("Draw Fullscreen Quad");
            // Use the mesh data directly to draw
            OpenGLRendererAPI::drawIndexed(meshData.indexCount, meshData.indexType, 
                meshData.indexAllocation->offsetBytes, meshData.vertexOffsetInVertices);
        }
        meshData.vao->unbind();
    }

    // New method to handle light setup, similar to Renderer::setupLightsUniforms
    void DeferredRenderer::setupLightsUniforms(const std::shared_ptr<Scene> s)
    {
        RAPTURE_PROFILE_SCOPE("Deferred Lights Uniform Setup");
        auto& reg = s->getRegistry();

        // Declare LightsUniform locally
        LightsUniform lightsData; 
        memset(&lightsData, 0, sizeof(LightsUniform));

        // Collect light data
        auto lightView = reg.view<LightComponent, TransformComponent>();
        uint32_t lightCount = 0;
        //s_cachedLightEntities.clear(); // Clear cache before refilling

        {
            RAPTURE_PROFILE_SCOPE("Deferred Light Data Collection");
            for (auto entityID : lightView)
            {
                if (lightCount >= MAX_LIGHTS) {
                     GE_CORE_WARN("DeferredRenderer: Maximum number of lights ({}) exceeded.", MAX_LIGHTS);
                    break;
                }
                
                Entity lightEntity(entityID, s.get());
                TransformComponent& transform = lightView.get<TransformComponent>(entityID);
                LightComponent& light = lightView.get<LightComponent>(entityID);

                if (!light.isActive) continue;
                
                if (light.hasChanged() || transform.hasChanged()) {
                    if (light.castsShadow) {
                        s_shadowMapDirty = true;
                    }                
                } else {
                    // can only do continue if i fix the persistent mapping
                    // right now it gets reset and flushes each frame ...
                    //continue;
                }


                //s_cachedLightEntities.push_back(entityID); // Cache entity handle

                // Fill light data
                LightData& lightData = lightsData.lights[lightCount]; // Use local lightsData
                
                lightData.position = glm::vec4(transform.translation(), static_cast<float>(light.type));
                lightData.color = glm::vec4(light.color, light.intensity);
                
                // Direction calculation (same as in Renderer.cpp)
                if (light.type == LightType::Directional || light.type == LightType::Spot)
                {
					// Convert Euler angles (already in radians) to direction vector
					glm::vec3 euler = transform.rotation();
					// Use rotation directly as it's stored in radians
					glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), euler.z, glm::vec3(0, 0, 1)) *
									  glm::rotate(glm::mat4(1.0f), euler.y, glm::vec3(0, 1, 0)) *
									  glm::rotate(glm::mat4(1.0f), euler.x, glm::vec3(1, 0, 0));
					

                    glm::quat rotationQuat = transform.transforms.getRotationQuat();
                    glm::vec3 lightDir = glm::normalize(rotationQuat * glm::vec3(0, 0, -1)); // Forward vector

					lightData.direction = glm::vec4(lightDir, light.range);
                } else {
                    lightData.direction = glm::vec4(0.0f, 0.0f, 0.0f, light.range); 
                }
                
                // Cone angles
                 if (light.type == LightType::Spot) {
                    // Convert degrees to radians then cosine for shader
                    lightData.coneAngles = glm::vec4(glm::cos(light.innerConeAngle), glm::cos(light.outerConeAngle), 0.0f, 0.0f);
                } else {
                    lightData.coneAngles = glm::vec4(0.0f);
                }
                
                lightCount++;
            }
        }
        
        lightsData.lightCount = lightCount; // Use local lightsData
        s_cachedLightCount = lightCount;
        
        // Update the UBO - setData will handle persistent mapping internally
        s_lightsUBO->setData(&lightsData, sizeof(LightsUniform)); // Always call setData
        
        s_lightsDirty = false; // Mark as clean after update

    }
}
