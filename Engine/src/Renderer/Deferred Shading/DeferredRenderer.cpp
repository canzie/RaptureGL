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
    void* DeferredRenderer::s_persistentLightsBufferPtr = nullptr; // Initialize static member

    // Initialize light cache members
    bool DeferredRenderer::s_lightsDirty = true;
    std::vector<entt::entity> DeferredRenderer::s_cachedLightEntities;
    uint32_t DeferredRenderer::s_cachedLightCount = 0;

    std::weak_ptr<Shader> DeferredRenderer::s_lightingPassShader;
    AssetHandle DeferredRenderer::s_lightingPassShaderHandle;

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
        
        s_cameraUBO = std::make_shared<UniformBuffer>(sizeof(CameraUniform), BufferUsage::Stream, nullptr, BASE_BINDING_POINT_IDX);
        s_lightsUBO = std::make_shared<UniformBuffer>(sizeof(LightsUniform), BufferUsage::Stream, nullptr, LIGHTS_BINDING_POINT_IDX);


        // Load deferred shaders
        auto [shader, handle] = AssetManager::importAsset<Shader>(s_shaderPath / "DeferredLightingPass.vert.glsl");
        s_lightingPassShader = shader;
        s_lightingPassShaderHandle = handle;

        setupFullscreenQuad();

        GE_CORE_INFO("Deferred rendering initialized");
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

        // Reset cache flags
        s_lightsDirty = true;
        s_cachedLightEntities.clear();
        s_cachedLightCount = 0;


        s_lightsUBO->unmap();
        s_persistentLightsBufferPtr = nullptr;
    }

    void DeferredRenderer::onFrameBegin()
    {
        // Not needed for now
    }

    void DeferredRenderer::onFrameEnd()
    {
        // Not needed for now
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
                } else if (std::holds_alternative<LightingPassCommand>(cmd)) {
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

            Entity camera_ent(cams.front(), s.get());
            CameraControllerComponent& controller_comp = camera_ent.getComponent<CameraControllerComponent>();

            // Get projection and view matrices
            const glm::mat4& projMat = controller_comp.camera.getProjectionMatrix();
            const glm::mat4& viewMat = controller_comp.camera.getViewMatrix();

			CameraUniform cameraData;
			cameraData.projection_mat = projMat;
			cameraData.view_mat = viewMat;
				
			s_cameraUBO->setData(&cameraData, sizeof(CameraUniform));
		}

        // Setup lights (uses caching)
        setupLightsUniforms(s);

        auto geometryQueue = CommandQueueBuilder::buildGeometryCommandQueueAsync(s, RenderQueueType::DEFERRED);
        renderQueueAsync(geometryQueue);


    }



    void DeferredRenderer::geometryPassRender(const RenderCommand &cmd)
    {
        RAPTURE_PROFILE_GPU_SCOPE("Executing geometryPassRender");
        RAPTURE_PROFILE_SCOPE("Executing geometryPassRender");
        
        // Use const references to avoid copies
        const auto& mesh = cmd.mesh;
        const auto& material = cmd.material;
        const auto& transform = cmd.transform;
        const auto& meshData = mesh->getMeshData();

        if (!mesh || !material) {
            GE_RENDER_ERROR("GeometryPassRender: Mesh or material is null");
            return;
        }

        meshData.vao->bind();
        material->bind(ShaderRenderPassType::GEOMETRY);
        
        std::shared_ptr<Shader> const shdr = material->getGeometryPassShader();
        if (!shdr) {
            GE_RENDER_ERROR("GeometryPassRender: Geometry pass shader is null");
            meshData.vao->unbind();
            material->unbind();
            return;
        }

        shdr->setFloat("u_SkinningEnabled", cmd.isSkeletal ? 1.0f : 0.0f);
        shdr->setMat4("u_model", transform);
        {
            RAPTURE_PROFILE_SCOPE("Draw Call");
            RAPTURE_PROFILE_GPU_SCOPE("Draw Call");
            // Single draw call - avoid function call overhead by directly accessing members
            OpenGLRendererAPI::drawIndexed(meshData.indexCount, meshData.indexType, 
                meshData.indexAllocation->offsetBytes, meshData.vertexOffsetInVertices);
        }

        // Clean state once
        material->unbind();
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
                glm::vec3 cameraPosition(1.0f, 1.0f, 1.0f);
                boundShader->setVec3("u_CameraPosition", cameraPosition);
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

        // TODO: Implement proper caching/dirty checking if needed.
        // For now, update every frame like the provided Renderer example.
        s_lightsDirty = true; 

        if (!s_lightsDirty && s_persistentLightsBufferPtr) {
           // return; // Skip update if not dirty and using persistent mapping
        }

        // Use persistent mapping if available
        LightsUniform* lightsDataPtr = nullptr;
        if (s_persistentLightsBufferPtr) {
            lightsDataPtr = static_cast<LightsUniform*>(s_persistentLightsBufferPtr);
            memset(lightsDataPtr, 0, sizeof(LightsUniform)); // Clear buffer memory
        } else {
            // Fallback if persistent mapping not available
            static LightsUniform lightsData; 
            memset(&lightsData, 0, sizeof(LightsUniform));
            lightsDataPtr = &lightsData;
        }

        // Collect light data
        auto lightView = reg.view<LightComponent, TransformComponent>();
        uint32_t lightCount = 0;
        s_cachedLightEntities.clear(); // Clear cache before refilling

        {
            RAPTURE_PROFILE_SCOPE("Deferred Light Data Collection");
            for (auto entityID : lightView)
            {
                if (lightCount >= MAX_LIGHTS) {
                     GE_CORE_WARN("DeferredRenderer: Maximum number of lights ({}) exceeded.", MAX_LIGHTS);
                    break;
                }
                
                Entity lightEntity(entityID, s.get());
                TransformComponent& transform = lightEntity.getComponent<TransformComponent>();
                LightComponent& light = lightEntity.getComponent<LightComponent>();
                
                if (!light.isActive) continue;
                
                s_cachedLightEntities.push_back(entityID); // Cache entity handle

                // Fill light data
                LightData& lightData = lightsDataPtr->lights[lightCount];
                
                lightData.position = glm::vec4(transform.translation(), static_cast<float>(light.type));
                lightData.color = glm::vec4(light.color, light.intensity);
                
                // Direction calculation (same as in Renderer.cpp)
                if (light.type == LightType::Directional || light.type == LightType::Spot)
                {
					// Convert Euler angles to direction vector
					glm::vec3 euler = transform.rotation();
					glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), euler.z, glm::vec3(0, 0, 1)) *
									  glm::rotate(glm::mat4(1.0f), euler.y, glm::vec3(0, 1, 0)) *
									  glm::rotate(glm::mat4(1.0f), euler.x, glm::vec3(1, 0, 0));
					
					glm::vec3 direction = glm::vec3(rotMat * glm::vec4(0, 0, -1, 0)); // Forward vector
					lightData.direction = glm::vec4(direction, light.range);
                } else {
                    lightData.direction = glm::vec4(0.0f, 0.0f, 0.0f, light.range); 
                }
                
                // Cone angles
                 if (light.type == LightType::Spot) {
                    // Convert degrees to radians then cosine for shader
                    lightData.coneAngles = glm::vec4(glm::cos(glm::radians(light.innerConeAngle)), glm::cos(glm::radians(light.outerConeAngle)), 0.0f, 0.0f);
                } else {
                    lightData.coneAngles = glm::vec4(0.0f);
                }
                
                lightCount++;
            }
        }
        
        lightsDataPtr->lightCount = lightCount;
        s_cachedLightCount = lightCount;
        
        // Update the UBO
        if (s_persistentLightsBufferPtr) {
            s_lightsUBO->flush(); // Flush changes if using persistent mapping
        } else {
            s_lightsUBO->setData(lightsDataPtr, sizeof(LightsUniform)); // Fallback update
        }

        s_lightsDirty = false; // Mark as clean after update
    }
}
