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

	void Renderer::init()
	{
		RAPTURE_PROFILE_FUNCTION();
		GE_RENDER_INFO("Renderer: Initializing renderer");
		s_cameraUBO = std::make_shared<UniformBuffer>(sizeof(CameraUniform), BufferUsage::Dynamic, nullptr, BASE_BINDING_POINT_IDX);
		s_cameraUBO->bindBase();
		
		// Create lights uniform buffer with the lights binding point
		s_lightsUBO = std::make_shared<UniformBuffer>(sizeof(LightsUniform), BufferUsage::Dynamic, nullptr, LIGHTS_BINDING_POINT_IDX);
		s_lightsUBO->bindBase();
	}

	void Renderer::shutdown()
	{
		GE_RENDER_INFO("Renderer: Shutting down renderer");
		
		// Reset uniform buffers
		s_cameraUBO.reset();
		s_lightsUBO.reset();
		
	}

	void Renderer::sumbitScene(const std::shared_ptr<Scene> s)
	{

		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			GE_CORE_ERROR("OpenGL error in renderer at sumbitScene 0: {0} (0x{1:x})", error, error);
		}

		// Extract entities from scene
		std::vector<entt::entity> meshEntities;
		entt::entity cameraEntity = entt::null;
		std::vector<entt::entity> lightEntities;
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

		// Setup lights
		setupLightsUniforms(s, lightEntities);

		// Render all meshes
		renderMeshes(s, meshEntities, camPos);
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

		// Create properly aligned camera uniform data
		CameraUniform cameraData;
		cameraData.projection_mat = controller_comp.camera.getProjectionMatrix();
		cameraData.view_mat = controller_comp.camera.getViewMatrix();
		
		// Ensure buffer is bound before setting data
		s_cameraUBO->bind();
		
		GLenum error = glGetError();
		if (error != GL_NO_ERROR) {
			GE_CORE_ERROR("OpenGL error after binding UBO: {0} (0x{1:x})", error, error);
			return false;
		}
		
		s_cameraUBO->bindBase();
		
		error = glGetError();
		if (error != GL_NO_ERROR) {
			GE_CORE_ERROR("OpenGL error after bindBase UBO: {0} (0x{1:x})", error, error);
			return false;
		}
		
		s_cameraUBO->setData(&cameraData, sizeof(CameraUniform));

		error = glGetError();
		if (error != GL_NO_ERROR) {
			GE_CORE_ERROR("OpenGL error after setData UBO: {0} (0x{1:x})", error, error);
			// Print info about the buffer
			GE_CORE_ERROR("  UBO ID: {0}, Size: {1}, BindingPoint: {2}", 
				s_cameraUBO->getID(), sizeof(CameraUniform), s_cameraUBO->getBindingPoint());
			return false;
		}

		// Unbind to ensure clean state
		s_cameraUBO->unbind();
		
		// Set camera position for shader use
		camPos = controller_comp.translation;
		camPos.z = -camPos.z;
		
		return true;
	}

	void Renderer::setupLightsUniforms(const std::shared_ptr<Scene> s, const std::vector<entt::entity>& lightEntities)
	{
		RAPTURE_PROFILE_SCOPE("Lights Uniform Setup");
		// Create lights uniform data
		LightsUniform lightsData;
		memset(&lightsData, 0, sizeof(LightsUniform)); // Clear the data first
		
		// Count active lights (up to MAX_LIGHTS)
		uint32_t lightCount = 0;
		
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
				LightData& lightData = lightsData.lights[lightCount];
				
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
		
		lightsData.lightCount = lightCount;
		
		// Upload light data to GPU
		{
			RAPTURE_PROFILE_SCOPE("Light UBO Upload");
			// Ensure lights buffer is bound before setting data
			s_lightsUBO->bind();
			s_lightsUBO->bindBase();
			s_lightsUBO->setData(&lightsData, sizeof(LightsUniform));
			s_lightsUBO->unbind();
		}
	}

	void Renderer::renderMeshes(const std::shared_ptr<Scene> s, 
                             const std::vector<entt::entity>& meshEntities, 
                             const glm::vec3& camPos)
	{
		RAPTURE_PROFILE_SCOPE("Mesh Rendering");
		
		// Count for stats
		int totalDrawCalls = 0;
		int skippedMeshes = 0;
		
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
				
				GLenum error = glGetError();
				if (error != GL_NO_ERROR) {
					GE_RENDER_ERROR("OpenGL error after material bind: {0} (0x{1:x})", error, error);
					
					// Add more detailed error checking to identify the problem
					Shader* shdr = material->getShader();
					if (!shdr) {
						GE_RENDER_ERROR("Material has null shader!");
					} 
					
					auto ubo = material->getUniformBuffer();
					if (!ubo) {
						GE_RENDER_ERROR("Material has null uniform buffer!");
					} 
					
					skippedMeshes++;
					continue;
				}
				
				// Per-object uniform setup
				{
					RAPTURE_PROFILE_SCOPE("Per-Object Uniforms");
					Shader* shdr = material->getShader();
					if (!shdr) {
						GE_RENDER_ERROR("Material shader pointer is null!");
						skippedMeshes++;
						continue;
					}
					
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
			}
		}
		
		// Log render stats occasionally
		static int frameCounter = 0;
		if (++frameCounter % 300 == 0) { // Every 300 frames, log stats
			GE_RENDER_INFO("Renderer stats: {0} draw calls, {1} skipped meshes", totalDrawCalls, skippedMeshes);
			frameCounter = 0;
		}
	}
}