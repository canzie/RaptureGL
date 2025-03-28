#include "Renderer.h"
#include "../Scenes/Components/Components.h"
#include "OpenGLRendererAPI.h"
#include <glm/gtx/transform.hpp>
#include "../Logger/Log.h"
#include "../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../Materials/MaterialUniformLayouts.h"
#include "../Buffers/BufferPools.h"
#include "glad/glad.h"
#include "../Debug/TracyProfiler.h"
#include "Raycast.h"
#include "PrimitiveShapes.h"
#include "../Materials/MaterialLibrary.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Rapture
{

	std::shared_ptr<UniformBuffer> Renderer::s_cameraUBO = nullptr;
	std::shared_ptr<UniformBuffer> Renderer::s_lightsUBO = nullptr;
	
	// Cache for camera data to avoid redundant uploads
	bool Renderer::s_cameraDataInitialized = false;
	glm::mat4 Renderer::s_cachedProjectionMatrix = glm::mat4(1.0f);
	glm::mat4 Renderer::s_cachedViewMatrix = glm::mat4(1.0f);
	void* Renderer::s_persistentCameraBufferPtr = nullptr;
	
	// Cache for lights data
	bool Renderer::s_lightsDataInitialized = false;
	void* Renderer::s_persistentLightsBufferPtr = nullptr;
	std::vector<entt::entity> Renderer::s_cachedLightEntities;
	uint32_t Renderer::s_cachedLightCount = 0;
	bool Renderer::s_lightsDirty = true;
	

	glm::vec3 Renderer::s_boundingBoxColor = glm::vec3(0.0f, 1.0f, 0.0f); // Default green

	// Frustum culling
	Frustum Renderer::s_frustum;
	bool Renderer::s_frustumCullingEnabled = true; // Enabled by default
	uint32_t Renderer::s_entitiesCulled = 0;

	std::vector<Rapture::Entity> Renderer::s_visibleEntities;

	void Renderer::init()
	{
		RAPTURE_PROFILE_FUNCTION();
		GE_RENDER_INFO("Renderer: Initializing renderer");

		Raycast::init();
		
		// Create camera UBO with persistent mapping capability
		s_cameraUBO = std::make_shared<UniformBuffer>(sizeof(CameraUniform), BufferUsage::Stream, nullptr, BASE_BINDING_POINT_IDX);
		s_cameraUBO->bindBase();
		
		// Create persistent mapping for camera data to avoid map/unmap overhead
		s_persistentCameraBufferPtr = s_cameraUBO->map(0, sizeof(CameraUniform));
		if (!s_persistentCameraBufferPtr) {
			GE_CORE_ERROR("Failed to create persistent mapping for camera buffer");
		}
		
		// Create lights uniform buffer with the lights binding point and persistent mapping
		s_lightsUBO = std::make_shared<UniformBuffer>(sizeof(LightsUniform), BufferUsage::Stream, nullptr, LIGHTS_BINDING_POINT_IDX);
		s_lightsUBO->bindBase();
		
		// Create persistent mapping for lights data
		s_persistentLightsBufferPtr = s_lightsUBO->map(0, sizeof(LightsUniform));
		if (!s_persistentLightsBufferPtr) {
			GE_CORE_ERROR("Failed to create persistent mapping for lights buffer");
		}
		
		// Initialize the shared resources for BoundingBoxComponent
		BoundingBoxComponent::initSharedResources();
		
		// Set the default bounding box color
		setBoundingBoxColor(glm::vec3(0.0f, 1.0f, 0.0f)); // Default green
		

		

	}

	void Renderer::shutdown()
	{
		GE_RENDER_INFO("Renderer: Shutting down renderer");

		Raycast::shutdown();
		
		// Shutdown the shared resources for BoundingBoxComponent
		BoundingBoxComponent::shutdownSharedResources();
		
		// Unmap persistent buffers before destroying them
		if (s_persistentCameraBufferPtr && s_cameraUBO) {
			s_cameraUBO->unmap();
			s_persistentCameraBufferPtr = nullptr;
		}
		
		if (s_persistentLightsBufferPtr && s_lightsUBO) {
			s_lightsUBO->unmap();
			s_persistentLightsBufferPtr = nullptr;
		}
		
		// Reset uniform buffers
		s_cameraUBO.reset();
		s_lightsUBO.reset();
		

	}

	void Renderer::sumbitScene(const std::shared_ptr<Scene> s)
	{

        RAPTURE_PROFILE_GPU_SCOPE("Renderer::SubmitScene");

		// Reset culling counter for this frame
		s_entitiesCulled = 0;

		// Extract entities from scene - only once per frame
		static std::vector<entt::entity> meshEntities;
		static entt::entity cameraEntity = entt::null;
		static std::vector<entt::entity> lightEntities;
		
		// Clear previous entities
		meshEntities.clear();
		lightEntities.clear();
		cameraEntity = entt::null;
		s_visibleEntities.clear();

		// Extract entities from scene
		{
			RAPTURE_PROFILE_SCOPE("Scene Data Extraction");
			extractSceneData(s, meshEntities, cameraEntity, lightEntities);
		}
		
		// Skip if no camera
		if (cameraEntity == entt::null) {
			GE_RENDER_ERROR("No camera found in scene");
			return;
		}

		// Setup camera uniforms and get camera position for shaders
		glm::vec3 camPos;
		{
			RAPTURE_PROFILE_SCOPE("Camera Setup");
			if (!setupCameraUniforms(s, cameraEntity, camPos)) {
				return;
			}
		}

		// Update frustum for culling
		if (s_frustumCullingEnabled) {
			RAPTURE_PROFILE_SCOPE("Frustum Update");
			s_frustum.update(s_cachedProjectionMatrix, s_cachedViewMatrix);
		}

		// Setup lights - will be skipped if no changes detected
		{
			RAPTURE_PROFILE_SCOPE("Lights Setup");
			setupLightsUniforms(s, lightEntities);
		}

		// Render all meshes
		{
			RAPTURE_PROFILE_SCOPE("Mesh Rendering");
			renderMeshes(s, meshEntities, camPos);
		}

        Raycast::onFrameEnd(s_visibleEntities);

        
        /*
		// Log culling stats occasionally
		static int frameCounter = 0;
		if (s_frustumCullingEnabled && ++frameCounter % 300 == 0) {
			GE_RENDER_INFO("Frustum culling: {0} entities culled out of {1} ({2:.1f}%)",
				s_entitiesCulled, meshEntities.size(),
				meshEntities.size() > 0 ? (100.0f * s_entitiesCulled / meshEntities.size()) : 0.0f);
			frameCounter = 0;
		}
        */
	}

	void Renderer::showBoundingBox(Entity entity, bool show)
	{
		if (!entity || !entity.hasComponent<BoundingBoxComponent>()) {
			return;
		}
		
		auto& boundingBoxComp = entity.getComponent<BoundingBoxComponent>();
		boundingBoxComp.isVisible = show;
		
		// Mark the bounding box for update if needed
		if (show && entity.hasComponent<TransformComponent>()) {
			boundingBoxComp.markForUpdate();
		}
	}
	
	void Renderer::hideBoundingBox(Entity entity)
	{
		if (!entity || !entity.hasComponent<BoundingBoxComponent>()) {
			return;
		}
		
		entity.getComponent<BoundingBoxComponent>().isVisible = false;
	}
	
	void Renderer::toggleBoundingBox(Entity entity)
	{
		if (!entity || !entity.hasComponent<BoundingBoxComponent>()) {
			return;
		}
		
		auto& boundingBoxComp = entity.getComponent<BoundingBoxComponent>();
		boundingBoxComp.isVisible = !boundingBoxComp.isVisible;
		
		// Mark for update if becoming visible
		if (boundingBoxComp.isVisible && entity.hasComponent<TransformComponent>()) {
			boundingBoxComp.markForUpdate();
		}
	}
	
	bool Renderer::isBoundingBoxVisible(Entity entity)
	{
		if (!entity || !entity.hasComponent<BoundingBoxComponent>()) {
			return false;
		}
		
		return entity.getComponent<BoundingBoxComponent>().isVisible;
	}
	
	void Renderer::setBoundingBoxColor(const glm::vec3& color)
	{
		s_boundingBoxColor = color;
		

	}
	
	glm::vec3 Renderer::getBoundingBoxColor()
	{
		return s_boundingBoxColor;
	}

	void Renderer::enableFrustumCulling(bool enable)
	{
		s_frustumCullingEnabled = enable;
		GE_RENDER_INFO("Frustum culling {0}", enable ? "enabled" : "disabled");
	}

	void Renderer::disableFrustumCulling()
	{
		s_frustumCullingEnabled = false;
		GE_RENDER_INFO("Frustum culling disabled");
	}

	void Renderer::toggleFrustumCulling()
	{
		s_frustumCullingEnabled = !s_frustumCullingEnabled;
		GE_RENDER_INFO("Frustum culling {0}", s_frustumCullingEnabled ? "enabled" : "disabled");
	}

	bool Renderer::isFrustumCullingEnabled()
	{
		return s_frustumCullingEnabled;
	}

	void Renderer::extractSceneData(const std::shared_ptr<Scene> s, 
								  std::vector<entt::entity>& meshEntities,
								  entt::entity& cameraEntity,
								  std::vector<entt::entity>& lightEntities)
	{
		RAPTURE_PROFILE_SCOPE("Scene Data Access");
		auto& reg = s->getRegistry();
		
		{
			RAPTURE_PROFILE_SCOPE("Mesh View Creation");
			auto meshes = reg.view<TransformComponent, MeshComponent>();
			auto cams = reg.view<CameraControllerComponent>();
			auto lights = reg.view<TransformComponent, LightComponent>();
			
			// Cache entity IDs to avoid registry lookups during rendering
			{
				RAPTURE_PROFILE_SCOPE("Mesh Entity Collection");
				for (auto ent : meshes) {
					meshEntities.push_back(ent);
				}
			}
			
			if (!cams.empty()) {
				cameraEntity = cams.front();
			}
			
			{
				RAPTURE_PROFILE_SCOPE("Light Entity Collection");
				for (auto ent : lights) {
					lightEntities.push_back(ent);
				}
			}
		}
	}

	bool Renderer::setupCameraUniforms(const std::shared_ptr<Scene> s, entt::entity cameraEntity, glm::vec3& camPos)
	{
		RAPTURE_PROFILE_SCOPE("Camera Uniform Setup");
		Entity camera_ent(cameraEntity, s.get());
		CameraControllerComponent& controller_comp = camera_ent.getComponent<CameraControllerComponent>();

		// Get projection and view matrices
		const glm::mat4& projMat = controller_comp.camera.getProjectionMatrix();
		const glm::mat4& viewMat = controller_comp.camera.getViewMatrix();
		
		// Check if the matrices have changed to avoid unnecessary updates
		bool matricesChanged = !s_cameraDataInitialized || 
							  projMat != s_cachedProjectionMatrix || 
							  viewMat != s_cachedViewMatrix;
		
		// Only update the buffer if matrices have changed
		if (matricesChanged) {
			// Update cached matrices
			s_cachedProjectionMatrix = projMat;
			s_cachedViewMatrix = viewMat;
			s_cameraDataInitialized = true;
			
			// Update camera buffer with new data
			if (s_persistentCameraBufferPtr) {
				// Use persistent mapping for faster updates
				CameraUniform* mappedData = static_cast<CameraUniform*>(s_persistentCameraBufferPtr);
				mappedData->projection_mat = projMat;
				mappedData->view_mat = viewMat;
				
				// No need to unmap with persistent mapping
				// Flush changes to ensure they're visible to the GPU
				s_cameraUBO->flush();
			}
			else {
				// Fallback to traditional setData if persistent mapping failed
				CameraUniform cameraData;
				cameraData.projection_mat = projMat;
				cameraData.view_mat = viewMat;
				
				s_cameraUBO->setData(&cameraData, sizeof(CameraUniform));
			}
		}
		
		// Set camera position for shader use
		camPos = controller_comp.translation;
		camPos.z = -camPos.z;
		
		return true;
	}

	void Renderer::setupLightsUniforms(const std::shared_ptr<Scene> s, const std::vector<entt::entity>& lightEntities)
	{
		RAPTURE_PROFILE_SCOPE("Lights Uniform Setup");
		
		// We'll update the lights every frame to ensure component data changes are reflected
		// Caching the entity list for debug purposes only
		s_cachedLightEntities = lightEntities;
		
		// Count active lights (up to MAX_LIGHTS)
		uint32_t lightCount = 0;
		
		// Use persistent mapping if available
		LightsUniform* lightsDataPtr = nullptr;
		if (s_persistentLightsBufferPtr) {
			lightsDataPtr = static_cast<LightsUniform*>(s_persistentLightsBufferPtr);
			// Clear the memory by setting it to zero
			memset(lightsDataPtr, 0, sizeof(LightsUniform));
		} else {
			// Fallback to traditional update
			static LightsUniform lightsData;
			memset(&lightsData, 0, sizeof(LightsUniform));
			lightsDataPtr = &lightsData;
		}
		
		// Collect light data
		{
			RAPTURE_PROFILE_SCOPE("Light Data Collection");
			for (auto entityID : lightEntities)
			{
				if (lightCount >= MAX_LIGHTS) break;
				
				Entity lightEntity(entityID, s.get());
				TransformComponent& transform = lightEntity.getComponent<TransformComponent>();
				LightComponent& light = lightEntity.getComponent<LightComponent>();
				
				if (!light.isActive) continue;
				
				// Fill light data
				LightData& lightData = lightsDataPtr->lights[lightCount];
				
				// Position and type
				lightData.position = glm::vec4(transform.translation(), static_cast<float>(light.type));
				
				// Color and intensity
				lightData.color = glm::vec4(light.color, light.intensity);
				
				// Direction (for directional/spot lights) and range
				if (light.type == LightType::Directional || light.type == LightType::Spot)
				{
					// Convert Euler angles to direction vector
					glm::vec3 euler = transform.rotation();
					glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), euler.z, glm::vec3(0, 0, 1)) *
									  glm::rotate(glm::mat4(1.0f), euler.y, glm::vec3(0, 1, 0)) *
									  glm::rotate(glm::mat4(1.0f), euler.x, glm::vec3(1, 0, 0));
					
					glm::vec3 direction = glm::vec3(rotMat * glm::vec4(0, 0, -1, 0)); // Forward vector
					lightData.direction = glm::vec4(direction, light.range);
				}
				else
				{
					lightData.direction = glm::vec4(0.0f, 0.0f, 0.0f, light.range);
				}
				
				// Cone angles for spot lights
				if (light.type == LightType::Spot)
				{
					lightData.coneAngles = glm::vec4(light.innerConeAngle, light.outerConeAngle, 0.0f, 0.0f);
				}
				else
				{
					lightData.coneAngles = glm::vec4(0.0f);
				}
				
				lightCount++;
			}
		}
		
		lightsDataPtr->lightCount = lightCount;
		
		// Cache the light count
		s_cachedLightCount = lightCount;
		
		// If using persistent mapping, flush the changes
		if (s_persistentLightsBufferPtr) {
			s_lightsUBO->flush();
		} else {
			// Fallback to traditional update
			s_lightsUBO->setData(lightsDataPtr, sizeof(LightsUniform));
		}
	}

	bool Renderer::isEntityVisible(const std::shared_ptr<Scene>& s, entt::entity entity)
	{
		// If frustum culling is disabled, all entities are visible
		if (!s_frustumCullingEnabled) {
			return true;
		}

		RAPTURE_PROFILE_SCOPE("Frustum Culling");
		
		// Get the entity as Entity class
		Entity e(entity, s.get());
		
		// Check if the entity has a BoundingBoxComponent
		if (!e.hasComponent<BoundingBoxComponent>()) {
			// If there's no bounding box, we can't perform culling, so consider it visible
			return true;
		}
		
		// Get the bounding box
		auto& boundingBoxComp = e.getComponent<BoundingBoxComponent>();
		
		// Check if the bounding box needs to be updated
		if (boundingBoxComp.needsUpdate && e.hasComponent<TransformComponent>()) {
			RAPTURE_PROFILE_SCOPE("Bounding Box Update");
			// Get transform and mesh components
			auto& transform = e.getComponent<TransformComponent>();
			
			// Get local bounding box
			const BoundingBox& localBox = boundingBoxComp.localBoundingBox;
			
			// Update the world bounding box based on the model matrix
			boundingBoxComp.worldBoundingBox = localBox.transform(transform.transformMatrix());
			boundingBoxComp.needsUpdate = false;
		}
		
		// Test if the bounding box is visible in the frustum
		{
			RAPTURE_PROFILE_SCOPE("Frustum Test");
			FrustumResult result = s_frustum.testBoundingBox(boundingBoxComp.worldBoundingBox);
			
			// Count culled entities for stats
			if (result == FrustumResult::Outside) {
				s_entitiesCulled++;
				return false;
			}
		}
		
		s_visibleEntities.push_back(e);
		return true;
	}

	void Renderer::renderMeshes(const std::shared_ptr<Scene> s, 
							 const std::vector<entt::entity>& meshEntities, 
							 const glm::vec3& camPos)
	{
		
		// Count for stats
		int totalDrawCalls = 0;
		int skippedMeshes = 0;
		int boundingBoxesDrawn = 0;
		int processedEntities = 0;
		int culledEntities = 0;
		
		for (auto ent : meshEntities)
		{
			processedEntities++;
			
			// In EnTT, entity 0 is not null, but entt::null is FFFFFFFF
			if ((uint32_t)ent == 0xFFFFFFFF) {
				GE_RENDER_ERROR("Entity is null (0xFFFFFFFF), skipping");
				skippedMeshes++;
				continue;
			}
			
			// Entity validation
			{
				RAPTURE_PROFILE_SCOPE("Entity Validation");
				Entity mesh(ent, s.get());
				
				if (!mesh.hasComponent<MeshComponent>()) {
					GE_RENDER_ERROR("Entity doesn't have MeshComponent, skipping");
					skippedMeshes++;
					continue;
				}

				// Perform frustum culling
				if (!isEntityVisible(s, ent)) {
					culledEntities++;
					skippedMeshes++;
					continue;
				}
				
				MeshComponent& meshComp = mesh.getComponent<MeshComponent>();
				
				// Skip rendering if the mesh is still loading
				if (meshComp.isLoading) {
					skippedMeshes++;
					continue;
				}
				
				auto meshe = meshComp.mesh;
				
				// Add proper null checking for mesh and vao
				if (!meshe) {
					GE_RENDER_ERROR("Null mesh encountered during rendering - entity ID: {0:x}", (uint32_t)ent);
					skippedMeshes++;
					continue;
				}
				
				MaterialComponent& mat = mesh.getComponent<MaterialComponent>();
				auto& material = mat.material;
				if (!material) {
					GE_RENDER_WARN("Renderer: Entity has no valid material assigned");
					skippedMeshes++;
					continue;
				}
				
				auto meshdata = meshComp.mesh->getMeshData();
				auto vao = meshdata.vao;
				
				// Resource binding
				{
					RAPTURE_PROFILE_SCOPE("Resource Binding");
					// Bind the VAO
					vao->bind();
					// Bind the material (which also binds the shader)
					material->bind();
				}
				
				// Per-object uniform setup
				{
					RAPTURE_PROFILE_SCOPE("Per-Object Uniforms");
					Shader* shdr = material->getShader();
					
					// Set the camera position uniform
					shdr->setVec3("u_camPos", camPos);
					
					// Calculate combined transform with parent hierarchy
					glm::mat4 modelMatrix = mesh.getComponent<TransformComponent>().transformMatrix();
					shdr->setMat4("u_model", modelMatrix);
				}
				
				// Draw call
				{
					RAPTURE_PROFILE_SCOPE("Draw Call");
					OpenGLRendererAPI::drawIndexed(meshdata.indexCount, meshdata.indexType, 
						meshdata.indexAllocation->offsetBytes, meshdata.vertexOffsetInVertices);
					totalDrawCalls++;
				}
				
				// Resource unbinding
				{
					RAPTURE_PROFILE_SCOPE("Resource Unbinding");
					// Unbind material after rendering this entity
					material->unbind();
					
					// Unbind VAO too for clean state management
					vao->unbind();
				}

				// Draw bounding box if enabled for this entity
				if (auto& boundingBoxComp = mesh.getComponent<BoundingBoxComponent>();
					boundingBoxComp.isVisible) {
					RAPTURE_PROFILE_SCOPE("Bounding Box Draw");
                    RAPTURE_PROFILE_GPU_SCOPE("Bounding Box Draw");
					// Bind the bounding box material
					if (boundingBoxComp.s_visualizationMesh) {
						boundingBoxComp.s_visualizationMaterial->bind();
						auto meshdata = boundingBoxComp.s_visualizationMesh->getMeshData();
                        meshdata.vao->bind();
						// Draw the bounding box
						drawBoundingBox(s, mesh);
						meshdata.vao->unbind();
						// Unbind the material
						boundingBoxComp.s_visualizationMaterial->unbind();
						
						boundingBoxesDrawn++;
					}
				}
			}
		}
		

	}
	
	
	void Renderer::drawBoundingBox(const std::shared_ptr<Scene> s, Entity entity)
	{
		if (!entity || !entity.hasComponent<BoundingBoxComponent>()) {
			GE_RENDER_ERROR("Entity missing BoundingBoxComponent in drawBoundingBox");
			return;
		}
		
		auto& boundingBoxComp = entity.getComponent<BoundingBoxComponent>();
		
		// Only draw if the bounding box is valid
		if (!boundingBoxComp.worldBoundingBox.isValid()) {
			GE_RENDER_WARN("Invalid world bounding box in drawBoundingBox");
			return;
		}
		
		// Get the bounding box dimensions and center
		glm::vec3 min = boundingBoxComp.worldBoundingBox.getMin();
		glm::vec3 max = boundingBoxComp.worldBoundingBox.getMax();
		
		// Safety check for NaN or infinity values
		if (glm::any(glm::isnan(min)) || glm::any(glm::isnan(max)) ||
			glm::any(glm::isinf(min)) || glm::any(glm::isinf(max))) {
			GE_RENDER_ERROR("Invalid bounding box coordinates (NaN or infinity detected)");
			return;
		}
		
		glm::vec3 size = max - min;
		glm::vec3 center = (min + max) * 0.5f;
		
		// Safety check for zero size
		if (glm::length(size) < 0.0001f) {
			GE_RENDER_WARN("Bounding box size too small, skipping rendering");
			return;
		}
		
		// Check if the shared resources are available
		if (!BoundingBoxComponent::s_visualizationMesh || !BoundingBoxComponent::s_visualizationMaterial) {
			GE_RENDER_ERROR("Bounding box visualization resources not initialized");
			return;
		}
		
		// Update material color to match the current bounding box color
		BoundingBoxComponent::s_visualizationMaterial->setVec3("color", s_boundingBoxColor);
		
		// Set up transformation matrix directly
		glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), center) * 
							   glm::scale(glm::mat4(1.0f), size);
		
		// Draw the cube in wireframe mode
		try {
			auto material = BoundingBoxComponent::s_visualizationMaterial;
			auto mesh = BoundingBoxComponent::s_visualizationMesh;
			auto vao = mesh->getMeshData().vao;
			if (vao && material) {
				material->bind();
				material->getShader()->setMat4("u_model", modelMatrix);
				vao->bind();

				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				glDrawElements(GL_LINES, mesh->getMeshData().indexCount, GL_UNSIGNED_INT, (void*)mesh->getMeshData().indexAllocation->offsetBytes);
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				
				vao->unbind();
				material->unbind();
			}
		}
		catch (const std::exception& e) {
			GE_RENDER_ERROR("Exception in drawBoundingBox: {0}", e.what());
		}
	}

    void Renderer::drawLine(const Line& line) {
        RAPTURE_PROFILE_FUNCTION();
        

        auto mesh = line.getMesh();
        auto material = line.getMaterial();
        if (!material || !mesh) {
            GE_RENDER_ERROR("Cannot draw line: null material or mesh");
            return;
        }
        
        material->bind();
        auto shader = material->getShader();
        
        // We need to manually set the projection and view matrices
        // because primitive shapes don't go through the standard rendering pipeline
        shader->setMat4("u_proj", s_cachedProjectionMatrix); 
        shader->setMat4("u_view", s_cachedViewMatrix);
        
        // The line vertices are in world space, so we use identity model matrix
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        shader->setMat4("u_model", modelMatrix);
        
        // Draw the line using GL_LINES
        auto vao = mesh->getMeshData().vao;
        if (vao) {
            vao->bind();
                glDrawElementsBaseVertex(GL_LINES, mesh->getMeshData().indexCount, GL_UNSIGNED_INT, (void*)mesh->getMeshData().indexAllocation->offsetBytes, mesh->getMeshData().vertexOffsetInVertices);
            vao->unbind();
        }

    }

    void Renderer::drawCube(const Cube& cube) {
        RAPTURE_PROFILE_FUNCTION();
        
        // Bind the material
        auto material = cube.getMaterial();
        if (material) {
            material->bind();
            
            // Create transformation matrix
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            
            // Apply translation, rotation, and scale
            modelMatrix = glm::translate(modelMatrix, cube.getPosition());
            
            // Apply rotation (X, Y, Z order)
            modelMatrix = glm::rotate(modelMatrix, glm::radians(cube.getRotation().x), glm::vec3(1.0f, 0.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, glm::radians(cube.getRotation().y), glm::vec3(0.0f, 1.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, glm::radians(cube.getRotation().z), glm::vec3(0.0f, 0.0f, 1.0f));
            
            modelMatrix = glm::scale(modelMatrix, cube.getScale());
            
            // Set the model matrix
            material->getShader()->setMat4("u_model", modelMatrix);
        }
        
        // Draw the cube
        auto mesh = cube.getMesh();
        if (mesh) {
            auto vao = mesh->getMeshData().vao;
            if (vao) {
                vao->bind();
                if (cube.isFilled()) {
                    glDrawElements(GL_TRIANGLES, mesh->getMeshData().indexCount, GL_UNSIGNED_INT, (void*)mesh->getMeshData().indexAllocation->offsetBytes);
                } else {
                    glDrawElements(GL_LINES, mesh->getMeshData().indexCount, GL_UNSIGNED_INT, (void*)mesh->getMeshData().indexAllocation->offsetBytes);
                }
                vao->unbind();
            }
        }
    }

    void Renderer::drawQuad(const Quad& quad) {
        RAPTURE_PROFILE_FUNCTION();
        
        // Bind the material
        auto material = quad.getMaterial();
        if (material) {
            material->bind();
            
            // Create transformation matrix
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            
            // Apply translation, rotation, and scale
            modelMatrix = glm::translate(modelMatrix, quad.getPosition());
            
            // Apply rotation (X, Y, Z order)
            modelMatrix = glm::rotate(modelMatrix, glm::radians(quad.getRotation().x), glm::vec3(1.0f, 0.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, glm::radians(quad.getRotation().y), glm::vec3(0.0f, 1.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, glm::radians(quad.getRotation().z), glm::vec3(0.0f, 0.0f, 1.0f));
            
            modelMatrix = glm::scale(modelMatrix, quad.getScale());
            
            // Set the model matrix
            material->getShader()->setMat4("u_model", modelMatrix);
        }
        
        // Draw the quad
        auto mesh = quad.getMesh();
        if (mesh) {
            auto vao = mesh->getMeshData().vao;
            if (vao) {
                vao->bind();
                glDrawElements(GL_TRIANGLES, mesh->getMeshData().indexCount, GL_UNSIGNED_INT, (void*)mesh->getMeshData().indexAllocation->offsetBytes);
                vao->unbind();
            }
        }
    }
}