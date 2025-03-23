#include "../Scenes/Entity.h"
#include "../Buffers/OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include <memory>

namespace Rapture
{

	class Renderer
	{
	public:

		static void init();

		static void sumbitScene(const std::shared_ptr<Scene> s);

	private:
		static std::shared_ptr<UniformBuffer> s_cameraUBO;
		static std::shared_ptr<UniformBuffer> s_lightsUBO;

	};

}