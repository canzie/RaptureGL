#pragma once

#include "RendererAPI.h"
#include "../Scenes/Scene.h"
#include "../Scenes/Entity.h"
#include "../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include "../Mesh/Mesh.h"
#include "../Materials/Material.h"
#include "PrimitiveShapes.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include "Frustum.h"
#include "RenderQueue.h"
#include "Deferred Shading/GBuffer.h"

namespace Rapture
{
	// Forward declaration
	class Mesh;
    class Material;
    class Sphere; // Forward declare Sphere

	class Renderer
	{
	public:
		// Initialize the renderer and its subsystems
		static void init();
		
		// Shutdown the renderer and its subsystems
		static void shutdown();

		static void sumbitScene(const std::shared_ptr<Scene> s);
		
        static void drawSprites(const std::shared_ptr<Scene> s);
        
		// Drawing functions that take shape objects as parameters
        static void drawLine(const Line& line);
        static void drawCube(const Cube& cube);
        static void drawQuad(const Quad& quad, const glm::mat4& transform=glm::mat4(1.0f));
        static void drawSphere(std::shared_ptr<Sphere> sphere);


		// Bounding box rendering methods
		// Show a bounding box for an entity
		static void showBoundingBox(Entity entity, bool show = true);
		
		// Hide a bounding box for an entity
		static void hideBoundingBox(Entity& entity);
		
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

        // Scene processing methods
        static void processScene(const std::shared_ptr<Scene> s);
        static void processSceneAsync(const std::shared_ptr<Scene> s);
        
        // Queue rendering methods
        static void renderQueue(RenderQueue* queue);
        static void renderQueueAsync(std::shared_ptr<RenderQueue> queue);
        
        static inline void renderMesh(const RenderCommand& cmd);

        static void drawAllBoundingBoxes(std::shared_ptr<Scene> s);
		// Draw a bounding box for a specific entity
		static void drawBoundingBox(Entity entity);

        // Draw multiple bounding boxes efficiently using instanced rendering
        // Assumes the provided transforms correctly position and scale a unit cube for each instance
        static void drawInstancedBoundingBoxes(const std::vector<std::pair<glm::vec3, glm::vec3>>& bounds, const std::vector<glm::mat4>& transforms);

        // Draw multiple instances of a base sphere at specified positions.
        static void drawInstancedSpheres(const Sphere& baseSphere, const std::vector<glm::vec3>& positions);

	private:
		// Static reusable line for debug purposes
		static std::unique_ptr<Line> s_debugLightLine;

		// Extract scene entities for rendering
		static void extractSceneData(const std::shared_ptr<Scene> s, 
			entt::entity& cameraEntity,
			std::vector<entt::entity>& lightEntities);

		// Setup camera uniform buffer
		static bool setupCameraUniforms(const std::shared_ptr<Scene> s);
			
		// Setup lights uniform buffer
		static void setupLightsUniforms(const std::shared_ptr<Scene> s, 
			const std::vector<entt::entity>& lightEntities);
		
		// Check if an entity is visible (frustum culling)
		static bool isEntityVisible(const std::shared_ptr<Scene>& s, entt::entity entity);
		

		

		
		// Uniform buffers for camera and lights
		static std::shared_ptr<UniformBuffer> s_cameraUBO;
		static std::shared_ptr<UniformBuffer> s_lightsUBO;
		static std::shared_ptr<UniformBuffer> s_cameraPositionUBO;
		// Camera uniform caching
		static bool s_cameraDataInitialized;
		static glm::mat4 s_cachedProjectionMatrix;
		static glm::mat4 s_cachedViewMatrix;
		static void* s_persistentCameraBufferPtr;
		
		// Lights uniform caching
		static bool s_lightsDataInitialized;
		static void* s_persistentLightsBufferPtr;
		static std::vector<entt::entity> s_cachedLightEntities;
		static uint32_t s_cachedLightCount;
		static bool s_lightsDirty;
		
		// Bounding box visualization color
		static glm::vec3 s_boundingBoxColor;
		
		// Frustum culling
		static Frustum s_frustum;
		static bool s_frustumCullingEnabled;
		static uint32_t s_entitiesCulled;
		
		// Visible entities for the current frame
		static std::vector<Rapture::Entity> s_visibleEntities;


	};

} // namespace Rapture