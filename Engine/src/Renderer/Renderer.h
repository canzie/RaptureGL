#pragma once

#include "RendererAPI.h"
#include "../Scenes/Scene.h"
#include "../Scenes/Entity.h"
#include "../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include "../Mesh/Mesh.h"
#include "../Materials/Material.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include "Frustum.h"

namespace Rapture
{
	// Forward declaration
	class Mesh;

	class Renderer
	{
	public:
		// Initialize the renderer and its subsystems
		static void init();
		
		// Shutdown the renderer and its subsystems
		static void shutdown();

		static void sumbitScene(const std::shared_ptr<Scene> s);
		
		// Bounding box rendering methods
		// Show a bounding box for an entity
		static void showBoundingBox(Entity entity, bool show = true);
		
		// Hide a bounding box for an entity
		static void hideBoundingBox(Entity entity);
		
		// Toggle bounding box visibility for an entity
		static void toggleBoundingBox(Entity entity);
		
		// Check if bounding box is visible for an entity
		static bool isBoundingBoxVisible(Entity entity);
		
		// Set the bounding box color
		static void setBoundingBoxColor(const glm::vec3& color);
		
		// Get the bounding box color
		static glm::vec3 getBoundingBoxColor();
		
		// Frustum culling methods
		static void enableFrustumCulling(bool enable = true);
		static void disableFrustumCulling();
		static void toggleFrustumCulling();
		static bool isFrustumCullingEnabled();

	private:
		// Extract scene entities for rendering
		static void extractSceneData(const std::shared_ptr<Scene> s, 
			std::vector<entt::entity>& meshEntities,
			entt::entity& cameraEntity,
			std::vector<entt::entity>& lightEntities);

		// Setup camera uniform buffer
		static bool setupCameraUniforms(const std::shared_ptr<Scene> s, entt::entity cameraEntity, glm::vec3& camPos);
		
		// Setup lights uniform buffer
		static void setupLightsUniforms(const std::shared_ptr<Scene> s, const std::vector<entt::entity>& lightEntities);
		
		// Render all meshes in the scene
		static void renderMeshes(const std::shared_ptr<Scene> s, 
			const std::vector<entt::entity>& meshEntities, 
			const glm::vec3& camPos);
			
		// Render visible bounding boxes
		static void renderBoundingBoxes(const std::shared_ptr<Scene> s,
			const glm::vec3& camPos);
			
		// Create a wireframe cube mesh for bounding box visualization
		static std::shared_ptr<Mesh> createBoundingBoxMesh();
		
		// Draw a single bounding box
		static void drawBoundingBox(const std::shared_ptr<Scene> s, Entity entity);

		// Uniform buffers
		static std::shared_ptr<UniformBuffer> s_cameraUBO;
		static std::shared_ptr<UniformBuffer> s_lightsUBO;
		
		// Camera data caching for optimization
		static bool s_cameraDataInitialized;
		static glm::mat4 s_cachedProjectionMatrix;
		static glm::mat4 s_cachedViewMatrix;
		static void* s_persistentCameraBufferPtr;
		
		// Lights data caching for optimization
		static bool s_lightsDataInitialized;
		static void* s_persistentLightsBufferPtr;
		static std::vector<entt::entity> s_cachedLightEntities;
		static uint32_t s_cachedLightCount;
		static bool s_lightsDirty;
		
		// Bounding box visualization
		static std::shared_ptr<Mesh> s_boundingBoxMesh;
		static std::shared_ptr<Material> s_boundingBoxMaterial;
		static glm::vec3 s_boundingBoxColor;

		// Frustum culling
		static Frustum s_frustum;
		static bool s_frustumCullingEnabled;
		static uint32_t s_entitiesCulled; // Counter for debug/stats

		// Helper methods for rendering
		static bool isEntityVisible(const std::shared_ptr<Scene>& s, entt::entity entity);
	};

}