#include "DeferredRenderer.h"

#include "../../Debug/TracyProfiler.h"
#include "../../Logger/Log.h"
#include "../OpenGLRendererAPI.h"
#include "../Raycast.h"
#include "../../Scenes/Components/Components.h"

#include "../../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../PrimitiveShapes.h"
#include <glad/glad.h> // Ensure glad is included

namespace Rapture
{


    std::filesystem::path s_shaderPath = "E:/Dev/Games/LiDAR Game v1/LiDAR-Game/Engine/src/Shaders/GLSL";

    // Define the static member variables
    std::shared_ptr<GBuffer> DeferredRenderer::s_gBuffer = nullptr;
    std::shared_ptr<Framebuffer> DeferredRenderer::s_lightingBuffer = nullptr;
    std::shared_ptr<Quad> DeferredRenderer::s_fullscreenQuad = nullptr;

    std::shared_ptr<UniformBuffer> DeferredRenderer::s_cameraUBO = nullptr;
    std::shared_ptr<UniformBuffer> DeferredRenderer::s_lightsUBO = nullptr;

    // Initialize light cache members
    bool DeferredRenderer::s_lightsDirty = true;
    std::vector<entt::entity> DeferredRenderer::s_cachedLightEntities;
    uint32_t DeferredRenderer::s_cachedLightCount = 0;

    std::weak_ptr<Shader> DeferredRenderer::s_lightingPassShader;
    AssetHandle DeferredRenderer::s_lightingPassShaderHandle;

    // Shadow mapping static members
    std::shared_ptr<ShadowMap> DeferredRenderer::s_shadowMap = nullptr;
    bool DeferredRenderer::s_shadowMapDirty = true;
    std::shared_ptr<Entity> DeferredRenderer::s_shadowCastingLight = nullptr;
    glm::mat4 DeferredRenderer::s_lightWVPMatrix = glm::mat4(1.0f);
    bool DeferredRenderer::s_isShadowPass = false;

    glm::vec3 DeferredRenderer::s_cameraPosition = glm::vec3(0.0f);

    void DeferredRenderer::init()
    {
		RAPTURE_PROFILE_FUNCTION();
		GE_RENDER_INFO("Renderer: Initializing renderer");

		Raycast::init();
		
		// Initialize the CommandQueueBuilder with a thread pool
		CommandQueueBuilder::init(2); // Create 2 worker threads by default
		

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
        
        // Initialize shadow map (2048x2048 is a common size for shadow maps)
        setupShadowMap();
        
        s_cameraUBO = std::make_shared<UniformBuffer>(sizeof(CameraUniform), BufferUsage::Stream, nullptr, BASE_BINDING_POINT_IDX);
        s_lightsUBO = std::make_shared<UniformBuffer>(sizeof(LightsUniform), BufferUsage::Stream, nullptr, LIGHTS_BINDING_POINT_IDX);


        // Load deferred shaders
        auto [shader, handle] = AssetManager::importAsset<Shader>(s_shaderPath / "DeferredLightingPass.vert.glsl");
        s_lightingPassShader = shader;
        s_lightingPassShaderHandle = handle;

        setupFullscreenQuad();

        GE_CORE_INFO("Deferred rendering initialized");
    }

    void DeferredRenderer::setupShadowMap()
    {
        RAPTURE_PROFILE_FUNCTION();

        if (s_shadowMap) {
            GE_CORE_INFO("Shadow map already initialized");
            return;
        }

        // Create shadow map with 2048x2048 resolution (can be adjusted for performance/quality)
        uint32_t shadowMapSize = 2048;
        s_shadowMap = std::make_shared<ShadowMap>(shadowMapSize, shadowMapSize);
        
        // Mark shadow map as dirty to ensure it's updated on first use
        s_shadowMapDirty = true;
        
        GE_CORE_INFO("Shadow mapping initialized with {}x{} resolution", shadowMapSize, shadowMapSize);
    }

    void DeferredRenderer::shutdown()
    {
		Raycast::shutdown();

		// Shutdown worker threads first to prevent accessing released resources
		CommandQueueBuilder::shutdownWorkers();

        // Clean up resources
        s_gBuffer.reset();
        s_lightingBuffer.reset();
        s_cameraUBO.reset();
        s_lightsUBO.reset();
        s_lightingPassShader.reset();
        s_lightingPassShaderHandle = 0;
        
        // Clean up shadow mapping resources
        s_shadowMap.reset();
        s_shadowCastingLight.reset();

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
        
        s_gBuffer->bind();

        // Process commands until the queue is both empty and marked as done
        while (!queue->isDone()) {
            // Try to get a command, process it if available
            CommandVariant cmd;
            if (queue->tryServe(cmd)) {
                if (std::holds_alternative<RenderCommand>(cmd)) {
                    geometryPassRender(std::get<RenderCommand>(cmd));
                } 
                else if (std::holds_alternative<ShadowPassCommand>(cmd)) {
                    // one shadow pass command will be at the begining and the end to toggle the shadow pass
                    s_isShadowPass = !s_isShadowPass;
                    
                    // Handle binding/unbinding of correct framebuffer
                    if (s_isShadowPass) {
                        // Bind shadow map
                        s_shadowMap->bind();
                    } else {
                        // Exiting shadow pass - unbind shadow map
                        s_shadowMap->unbind();
                    }
                }
                else if (std::holds_alternative<LightingPassCommand>(cmd)) {
                    // Geometry pass finished, G-Buffer is complete.

                    // Unbind G-buffer as render target
                    s_gBuffer->unbind();

                    // *** Blit Depth Buffer ***
                    {
                        RAPTURE_PROFILE_SCOPE("Blit Depth Buffer");
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
                    // *** End Blit Depth Buffer ***

                    // Now execute the lighting pass which will bind s_lightingBuffer
                    lightingPassRender(std::get<LightingPassCommand>(cmd));
                } else if (std::holds_alternative<SSRCommand>(cmd)) {

                } else {
                    GE_CORE_ERROR("Unknown command type in deferred queue");
                }
            }
        }
    }

    void DeferredRenderer::sumbitScene(const std::shared_ptr<Scene> s)
    {
        RAPTURE_PROFILE_FUNCTION();
        RAPTURE_PROFILE_GPU_SCOPE("DeferredRenderer::GeometryPass");

		// Setup camera uniforms and get camera position for shaders
		{
			RAPTURE_PROFILE_SCOPE("Camera Setup");
            auto& reg = s->getRegistry();
            auto cams = reg.view<CameraControllerComponent>();

            //Entity camera_ent(cams.front(), s.get());
            CameraControllerComponent& controller_comp = cams.get<CameraControllerComponent>(cams.front());

            // Get projection and view matrices
            const glm::mat4& projMat = controller_comp.camera.getProjectionMatrix();
            const glm::mat4& viewMat = controller_comp.camera.getViewMatrix();

            s_cameraPosition = controller_comp.translation;

			CameraUniform cameraData;
			cameraData.projection_mat = projMat;
			cameraData.view_mat = viewMat;

				
			s_cameraUBO->setData(&cameraData, sizeof(CameraUniform));
		}

        // Setup lights (uses caching)
        setupLightsUniforms(s);

        updateShadowMatrix(s);



        // the 2 queues should be made at the same time, this only happens when they are called after each other,
        // the other methods are no async so they would be blocking the main thread, so it cannot ask for the other queue
        auto geometryQueue = CommandQueueBuilder::buildGeometryCommandQueueAsync(s, RenderQueueType::DEFERRED);
        std::shared_ptr<RenderQueue> shadowQueue = nullptr;
        if (s_shadowMapDirty) {
            shadowQueue = CommandQueueBuilder::buildGeometryCommandQueueAsync(s, RenderQueueType::SHADOWMAP);
            renderQueueAsync(shadowQueue);

            s_shadowMapDirty = false;

        }

        renderQueueAsync(geometryQueue);

    }

    // TODO: Dogshit, needs to be giga optimized
    void DeferredRenderer::updateShadowMatrix(const std::shared_ptr<Scene>& scene)
    {
        RAPTURE_PROFILE_FUNCTION();
        
        // Skip update if shadow map isn't dirty and we have a valid shadow casting light
        if (!s_shadowMapDirty && s_shadowCastingLight && s_shadowCastingLight->isValid()) {
            return;
        }

        
        auto& reg = scene->getRegistry();
        auto lightView = reg.view<LightComponent, TransformComponent>();
        
        // Find a suitable light for shadow casting (preferably a directional light)
        Entity selectedLight;
        glm::vec3 lightPos;
        glm::vec3 lightDir;
        
        for (auto entityID : lightView) {
            Entity lightEntity(entityID, scene.get());
            LightComponent& light = lightView.get<LightComponent>(entityID);
            
            if (!light.castsShadow) continue;

            // Prefer directional lights for shadows
            if (light.isActive && light.type == LightType::Directional) {
                selectedLight = lightEntity;
                
                // Get light transform
                TransformComponent& transform = lightView.get<TransformComponent>(entityID);
                lightPos = transform.translation();
                
                // Calculate light direction from rotation (already in radians)
                // Use quaternion for more robust direction calculation
                glm::quat rotationQuat = transform.transforms.getRotationQuat(); // Access Transforms member
                lightDir = glm::normalize(rotationQuat * glm::vec3(0, 0, -1)); // Forward vector in local space

                break;
            }
            
            // Fallback to spot or point lights if no directional lights found
            if (!selectedLight.isValid() && light.isActive) {
                selectedLight = lightEntity;
                
                TransformComponent& transform = lightView.get<TransformComponent>(entityID);
                lightPos = transform.translation();
                
                if (light.type == LightType::Spot) {
                    // Calculate spot light direction using quaternion
                    glm::quat rotationQuat = transform.transforms.getRotationQuat(); // Access Transforms member
                    lightDir = glm::normalize(rotationQuat * glm::vec3(0, 0, -1)); // Forward vector

                } else {
                    // Point light has no direction, so use a default
                    lightDir = glm::vec3(0.0f, -1.0f, 0.0f); // Down direction
                }
            }
        }

        
        // If no suitable light found, return
        if (!selectedLight.isValid()) {
            GE_RENDER_WARN("No suitable light found for shadow mapping");
            return;
        }
        
        // Store the selected light for next frame reference
        s_shadowCastingLight = std::make_shared<Entity>(selectedLight);
        
        // Calculate light view matrix
        glm::vec3 lightUp = glm::vec3(0.0f, 1.0f, 0.0f);
        if (abs(glm::dot(lightDir, lightUp)) > 0.99f) {
            // If light is pointing directly up or down, use a different up vector
            lightUp = glm::vec3(0.0f, 0.0f, 1.0f);
        }
        
        // Create the light's view matrix
        const glm::mat4 viewMatrix = glm::lookAt(
            lightPos,               // Light position
            lightPos + lightDir,    // Look at center (Point along the light direction)
            lightUp                 // Up vector
        );
        
        // Calculate orthographic projection matrix for directional lights
        // or perspective for spot/point lights
        glm::mat4 lightProj(1.0f);
        
        LightComponent& light = selectedLight.getComponent<LightComponent>();
        
        if (light.type == LightType::Directional) {
            // Orthographic projection for directional light
            // These values may need adjustment based on your scene scale
            float orthoSize = 10.0f;
            lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 1000.0f);
        } else {
            // Perspective projection for spot/point light
            // Use the light's actual outer cone angle for FOV
            float aspect = 1.0f; // Shadow map is square
            float nearPlane = 0.1f;
            // Use outerConeAngle * 2 for FOV, but ensure it's > 0
            float fovRadians = (light.type == LightType::Spot) ? glm::max(light.outerConeAngle * 2.0f, glm::radians(1.0f)) : glm::radians(90.0f);
            lightProj = glm::perspective(fovRadians, aspect, nearPlane, light.range);
        }
        
        // Combine for final light space matrix
        s_lightWVPMatrix = lightProj * viewMatrix;
        
        // Update the shadow map shader with the new matrix
        s_shadowMap->setWVPMatrix(s_lightWVPMatrix);
        
        // Mark shadow map as no longer dirty
        //s_shadowMapDirty = false;
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
            auto shadowShader = s_shadowMap->getShader();
            if (!shadowShader) {
                GE_RENDER_ERROR("Shadow pass shader is null");
                meshData.vao->unbind();
                return;
            }
            
            shadowShader->bind();
            
            // Calculate and set model-view-projection matrix for this object
            // The light WVP (view and projection) is already set in the shadow map
            // We just need to multiply by the model matrix here
            glm::mat4 mvp = s_lightWVPMatrix * localTransform;
            shadowShader->setMat4("gWVP", mvp);
            
            // Additional parameters for skinning if needed
            shadowShader->setFloat("u_SkinningEnabled", cmd.isSkeletal ? 1.0f : 0.0f);
            
            {
                RAPTURE_PROFILE_SCOPE("Draw Call");
                RAPTURE_PROFILE_GPU_SCOPE("Draw Call");
                // Draw shadow map
                OpenGLRendererAPI::drawIndexed(meshData.indexCount, meshData.indexType, 
                    meshData.indexAllocation->offsetBytes, meshData.vertexOffsetInVertices);
            }
            
            // Unbind shader
            shadowShader->unBind();
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
                
                // Set light space matrix for shadow mapping
                boundShader->setMat4("u_LightSpaceMatrix", s_lightWVPMatrix);
            }
            if (s_shadowMap) {
                s_shadowMap->bindForReading();
            }
        
            s_lightingBuffer->bind(false);
		    
            // will be enabled next time we bind again, should also never be called when not binded
            s_lightingBuffer->disableDepthTesting();

        }

        {
            RAPTURE_PROFILE_SCOPE("Bind G-Buffer Textures");
            s_gBuffer->bindTextures();
        }

        {
            RAPTURE_PROFILE_SCOPE("Lighting Pass");
            // TODO: Implement lighting pass specifics (e.g., setting light uniforms)
            renderFullscreenQuad(); // Render the quad to apply lighting shader
        }


            if (s_shadowMap) {
                s_shadowMap->unbindForReading();
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

    void DeferredRenderer::renderFullscreenQuad(){
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
            

        s_lightingPassShader.lock()->setMat4("u_model", modelMatrix);

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
					
					glm::vec3 direction = glm::normalize(glm::vec3(rotMat * glm::vec4(0, 0, -1, 0))); // Forward vector
					lightData.direction = glm::vec4(direction, light.range);
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
