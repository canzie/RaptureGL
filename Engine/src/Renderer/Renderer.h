#include "../Scenes/Entity.h"
#include "../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include <memory>
#include <vector>

namespace Rapture
{

	class Renderer
	{
	public:
		// Initialize the renderer and its subsystems
		static void init();
		
		// Shutdown the renderer and its subsystems
		static void shutdown();

		static void sumbitScene(const std::shared_ptr<Scene> s);

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

		static std::shared_ptr<UniformBuffer> s_cameraUBO;
		static std::shared_ptr<UniformBuffer> s_lightsUBO;
	};

}