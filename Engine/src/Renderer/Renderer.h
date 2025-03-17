#include "../Scenes/Entity.h"
#include "../Shaders/UniformBuffer.h"
#include <memory>

namespace Rapture
{

	class Renderer
	{
	public:

		static void init();

		static void sumbitScene(const std::shared_ptr<Scene> s);

	private:
		static UniformBuffer* s_cameraUBO;

	};

}