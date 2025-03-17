#include "OpenGLRendererAPI.h"

#include <glad/glad.h>

namespace Rapture {

	
	void OpenGLRendererAPI::setClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	}
	/*
	void OpenGLRendererAPI::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
	{
		glViewport(x, y, width, height);
	}*/

	void OpenGLRendererAPI::drawIndexed(int indexCount, unsigned int comp_type)
	{
		glDrawElements(GL_TRIANGLES, indexCount, (GLenum)comp_type, nullptr);
	}

	void OpenGLRendererAPI::drawIndexed(int indexCount, unsigned int comp_type, size_t offset)
	{
		glDrawElements(GL_TRIANGLES, indexCount, (GLenum)comp_type, (void*)offset);
	}

	
	void OpenGLRendererAPI::drawNonIndexed(int vertexCount)
	{
		glDrawArrays(GL_TRIANGLES, 0, vertexCount);
	}
	
}