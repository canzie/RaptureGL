#include "OpenGLUniformBuffer.h"
#include <glad/glad.h>
#include <glm/glm.hpp>


namespace Rapture
{

	OpenGLUniformBuffer::OpenGLUniformBuffer(unsigned long long size, unsigned int idx)
	{
		m_size = size;

		glGenBuffers(1, &m_uboID);
		glBindBuffer(GL_UNIFORM_BUFFER, m_uboID);
		glBufferData(GL_UNIFORM_BUFFER, size, NULL, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		glBindBufferRange(GL_UNIFORM_BUFFER, idx, m_uboID, 0, size);

	}
	OpenGLUniformBuffer::~OpenGLUniformBuffer()
	{
		glDeleteBuffers(1, &m_uboID);
	}

	void OpenGLUniformBuffer::updateAllBufferData(const void* data)
	{
		updateBufferData(0, m_size, data);
	}

	void OpenGLUniformBuffer::updateBufferData(long long offset, unsigned long long size, const void* data)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, m_uboID);
		glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
}