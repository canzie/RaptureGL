#include "Renderer.h"
#include "../Scenes/Components/Components.h"
#include "OpenGLRendererAPI.h"
#include <glm/gtx/transform.hpp>
#include "../Logger/Log.h"
#include "../Shaders/OpenGLUniforms/UniformBindingPointIndices.h"
#include "../Materials/MaterialUniformLayouts.h"
#include "../Buffers/BufferPools.h"
#include "glad/glad.h"
#include "../Debug/Profiler.h"

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
	
	// Bounding box visualization
	std::shared_ptr<Mesh> Renderer::s_boundingBoxMesh = nullptr;
	std::shared_ptr<Material> Renderer::s_boundingBoxMaterial = nullptr;
	glm::vec3 Renderer::s_boundingBoxColor = glm::vec3(0.0f, 1.0f, 0.0f); // Default green

	// Frustum culling
	Frustum Renderer::s_frustum;
	bool Renderer::s_frustumCullingEnabled = true; // Enabled by default
	uint32_t Renderer::s_entitiesCulled = 0;

	void Renderer::init()
	{
		RAPTURE_PROFILE_FUNCTION();
		GE_RENDER_INFO("Renderer: Initializing renderer");
		
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
		
		// Initialize bounding box mesh
		s_boundingBoxMesh = createBoundingBoxMesh();
		if (!s_boundingBoxMesh) {
			GE_CORE_ERROR("Failed to create bounding box mesh");
		}
		
		// Initialize bounding box material
		try {
			// Create solid material with base color vector
			s_boundingBoxMaterial = MaterialLibrary::createSolidMaterial("BoundingBoxMaterial", s_boundingBoxColor);
			if (!s_boundingBoxMaterial) {
				GE_CORE_ERROR("Failed to create bounding box material");
			}
			else {
				// Verify the material has a valid shader
				if (!s_boundingBoxMaterial->getShader()) {
					GE_CORE_ERROR("Bounding box material has null shader");
				}
				else {
					// Force set the initial color to ensure it's set properly
					if (s_boundingBoxMaterial->hasParameter("color")) {
						// Use the correct setter based on what the material expects
						// Check if the parameter is a vec3 or vec4
						try {
							// Try to get current value as vec3
							s_boundingBoxMaterial->getParameter("color").asVec3();
							// If we get here, it's a vec3
							s_boundingBoxMaterial->setVec3("color", s_boundingBoxColor);
						}
						catch (...) {
							// If asVec3 throws, it's not a vec3, assume vec4
							s_boundingBoxMaterial->setVec4("color", glm::vec4(s_boundingBoxColor, 1.0f));
						}
					} else {
						GE_CORE_ERROR("Bounding box material doesn't have a 'color' parameter");
					}
					GE_RENDER_INFO("Successfully initialized bounding box resources");
				}
			}
		}
		catch (const std::exception& e) {
			GE_CORE_ERROR("Exception creating bounding box material: {0}", e.what());
		}

	}

	void Renderer::shutdown()
	{
		GE_RENDER_INFO("Renderer: Shutting down renderer");
		
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
		
		// Reset bounding box resources
		s_boundingBoxMesh.reset();
		s_boundingBoxMaterial.reset();
	}

	void Renderer::sumbitScene(const std::shared_ptr<Scene> s)
	{
		RAPTURE_PROFILE_FUNCTION();

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
		
		// Extract entities from scene
		extractSceneData(s, meshEntities, cameraEntity, lightEntities);
		
		// Skip if no camera
		if (cameraEntity == entt::null) {
			GE_RENDER_ERROR("No camera found in scene");
			return;
		}

		// Setup camera uniforms and get camera position for shaders
		glm::vec3 camPos;
		if (!setupCameraUniforms(s, cameraEntity, camPos)) {
			return;
		}

		// Update frustum for culling
		if (s_frustumCullingEnabled) {
			s_frustum.update(s_cachedProjectionMatrix, s_cachedViewMatrix);
		}

		// Setup lights - will be skipped if no changes detected
		setupLightsUniforms(s, lightEntities);

		// Render all meshes
		renderMeshes(s, meshEntities, camPos);

		// Log culling stats occasionally
		static int frameCounter = 0;
		if (s_frustumCullingEnabled && ++frameCounter % 300 == 0) {
			GE_RENDER_INFO("Frustum culling: {0} entities culled out of {1} ({2:.1f}%)",
				s_entitiesCulled, meshEntities.size(),
				meshEntities.size() > 0 ? (100.0f * s_entitiesCulled / meshEntities.size()) : 0.0f);
			frameCounter = 0;
		}
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
		
		// Update the material color
		if (s_boundingBoxMaterial) {
			try {
				// Check what parameter type is expected for the color
				if (s_boundingBoxMaterial->hasParameter("color")) {
					// Use the correct setter based on what the material expects
					try {
						// Try to get current value as vec3
						s_boundingBoxMaterial->getParameter("color").asVec3();
						// If we get here, it's a vec3
						s_boundingBoxMaterial->setVec3("color", color);
					}
					catch (...) {
						// If asVec3 throws, it's not a vec3, assume vec4
						s_boundingBoxMaterial->setVec4("color", glm::vec4(color, 1.0f));
					}
				} else {
					GE_RENDER_ERROR("Cannot update color - material doesn't have a 'color' parameter");
				}
			}
			catch (const std::exception& e) {
				GE_RENDER_ERROR("Exception setting bounding box color: {0}", e.what());
			}
		}
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
		auto meshes = reg.view<TransformComponent, MeshComponent>();
		auto cams = reg.view<CameraControllerComponent>();
		auto lights = reg.view<TransformComponent, LightComponent>();
		
		// Cache entity IDs to avoid registry lookups during rendering
		for (auto ent : meshes) {
			meshEntities.push_back(ent);
		}
		
		if (!cams.empty()) {
			cameraEntity = cams.front();
		}
		
		for (auto ent : lights) {
			lightEntities.push_back(ent);
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
			// Get transform and mesh components
			auto& transform = e.getComponent<TransformComponent>();
			
			// Get local bounding box
			const BoundingBox& localBox = boundingBoxComp.localBoundingBox;
			
			// Update the world bounding box based on the model matrix
			boundingBoxComp.worldBoundingBox = localBox.transform(transform.transformMatrix());
			boundingBoxComp.needsUpdate = false;
		}
		
		// Test if the bounding box is visible in the frustum
		FrustumResult result = s_frustum.testBoundingBox(boundingBoxComp.worldBoundingBox);
		
		// Count culled entities for stats
		if (result == FrustumResult::Outside) {
			s_entitiesCulled++;
			return false;
		}
		
		return true;
	}

	void Renderer::renderMeshes(const std::shared_ptr<Scene> s, 
                             const std::vector<entt::entity>& meshEntities, 
                             const glm::vec3& camPos)
	{
		RAPTURE_PROFILE_SCOPE("Mesh Rendering");
		
		// Count for stats
		int totalDrawCalls = 0;
		int skippedMeshes = 0;
		int boundingBoxesDrawn = 0;
		
		for (auto ent : meshEntities)
		{
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
				if (mesh.hasComponent<BoundingBoxComponent>() && 
					mesh.getComponent<BoundingBoxComponent>().isVisible) {
					
					// Bind the bounding box material
					if (s_boundingBoxMaterial) {
						s_boundingBoxMaterial->bind();
						
						// Draw the bounding box
						drawBoundingBox(s, mesh);
						
						// Unbind the material
						s_boundingBoxMaterial->unbind();
						
						boundingBoxesDrawn++;
					}
				}
			}
		}
		
		// Log render stats occasionally
		static int frameCounter = 0;
		if (++frameCounter % 300 == 0) { // Every 300 frames, log stats
			GE_RENDER_INFO("Renderer stats: {0} draw calls, {1} skipped meshes, {2} bounding boxes", 
				totalDrawCalls, skippedMeshes, boundingBoxesDrawn);
			frameCounter = 0;
		}
	}
	
	std::shared_ptr<Mesh> Renderer::createBoundingBoxMesh()
	{
		// Keep this log since it only runs once during initialization
		GE_RENDER_INFO("Creating bounding box mesh");
		
		// Create a unit cube mesh for bounding box visualization
		auto mesh = std::make_shared<Mesh>();
		if (!mesh) {
			GE_RENDER_ERROR("Failed to create bounding box mesh");
			return nullptr;
		}
		
		try {
			// Vertices for a unit cube (centered at origin with size 1)
			std::vector<float> vertices = {
				// Front face
				-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f, // Bottom-left
				 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f, // Bottom-right
				 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f, // Top-right
				-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f, // Top-left
				
				// Back face
				-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f, // Bottom-left
				 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f, // Bottom-right
				 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f, // Top-right
				-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f  // Top-left
			};
			
			// Indices for a cube
			std::vector<uint32_t> indices = {
				// Front face
				0, 1, 2, 2, 3, 0,
				// Back face
				4, 5, 6, 6, 7, 4,
				// Left face
				0, 3, 7, 7, 4, 0,
				// Right face
				1, 5, 6, 6, 2, 1,
				// Bottom face
				0, 4, 5, 5, 1, 0,
				// Top face
				2, 6, 7, 7, 3, 2
			};
			
			// Create a buffer layout
			BufferLayout layout;
			layout.buffer_attribs = {
				{ "POSITION", GL_FLOAT, "VEC3", 0 },
				{ "NORMAL", GL_FLOAT, "VEC3", sizeof(float) * 3 },
				{ "TEXCOORD_0", GL_FLOAT, "VEC2", sizeof(float) * 6 }
			};
			layout.isInterleaved = true;
			layout.vertexSize = sizeof(float) * 8;
			
			// Verify data is valid
			if (vertices.empty() || indices.empty()) {
				GE_RENDER_ERROR("Bounding box mesh data is empty");
				return nullptr;
			}
			
			// Set mesh data
			mesh->setMeshData(
				layout,
				vertices.data(),
				vertices.size() * sizeof(float),
				indices.data(),
				indices.size() * sizeof(uint32_t),
				indices.size(),
				GL_UNSIGNED_INT
			);
			
			// Verify that the mesh data was set correctly
			auto meshData = mesh->getMeshData();
			if (!meshData.vao) {
				GE_RENDER_ERROR("Bounding box mesh VAO is null after creation");
				return nullptr;
			}
			
			if (meshData.indexCount == 0) {
				GE_RENDER_ERROR("Bounding box mesh has zero indices after creation");
				return nullptr;
			}
			
			// Keep this success log as it only runs once during initialization
			GE_RENDER_INFO("Successfully created bounding box mesh with {0} vertices, {1} indices", 
				vertices.size() / 8, meshData.indexCount);
				
			return mesh;
		}
		catch (const std::exception& e) {
			GE_RENDER_ERROR("Exception creating bounding box mesh: {0}", e.what());
			return nullptr;
		}
	}
	
	void Renderer::drawBoundingBox(const std::shared_ptr<Scene> s, Entity entity)
	{
		if (!entity || !entity.hasComponent<BoundingBoxComponent>()) {
			GE_RENDER_ERROR("Entity missing BoundingBoxComponent in drawBoundingBox");
			return;
		}
		
		if (!s_boundingBoxMesh || !s_boundingBoxMaterial) {
			GE_RENDER_ERROR("Bounding box resources not properly initialized");
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
		
		// Create a scaling and translation matrix for the unit cube
		glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), center) * glm::scale(glm::mat4(1.0f), size);
		
		// Set the model matrix in the shader
		Shader* shader = s_boundingBoxMaterial->getShader();
		if (!shader) {
			GE_RENDER_ERROR("Null shader in drawBoundingBox");
			return;
		}
		
		shader->setMat4("u_model", modelMatrix);
		
		// Get mesh data safely
		auto meshdata = s_boundingBoxMesh->getMeshData();
		if (!meshdata.vao || meshdata.indexCount == 0) {
			GE_RENDER_ERROR("Invalid mesh data in drawBoundingBox");
			return;
		}
		
		// Draw the cube
		try {
			// Remove verbose coordinates log
			// GE_RENDER_INFO("Drawing bounding box: min({0},{1},{2}), max({3},{4},{5})",
			//	min.x, min.y, min.z, max.x, max.y, max.z);
			
			OpenGLRendererAPI::drawIndexed(
				meshdata.indexCount, 
				meshdata.indexType, 
				meshdata.indexAllocation->offsetBytes, 
				meshdata.vertexOffsetInVertices);
		}
		catch (const std::exception& e) {
			GE_RENDER_ERROR("Exception in drawIndexed: {0}", e.what());
		}
	}
}