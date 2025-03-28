#include <glm/glm.hpp>
#include <memory>


namespace Rapture {

	class OpenGLRendererAPI
	{
	public:
		static void setClearColor(const glm::vec4& color);
		static void clear();

		//void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) ;

		static void drawIndexed(int indexCount, unsigned int comp_type);
		static void drawIndexed(int indexCount, unsigned int comp_type, size_t offset, size_t vertexOffset=0);

		static void drawLine(glm::vec3 start, glm::vec3 end, glm::vec4 color);
		static void drawCube(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color, bool filled = false);
		static void drawQuad(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale, glm::vec4 color);

	};

}