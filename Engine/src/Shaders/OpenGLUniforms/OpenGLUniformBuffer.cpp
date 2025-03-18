#include "OpenGLUniformBuffer.h"
#include "../../Logger/Log.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstring>

namespace Rapture
{
	// UniformBuffer factory method implementation
	UniformBuffer* UniformBuffer::create(uint64_t size, uint32_t binding)
	{
		return new OpenGLUniformBuffer(size, binding);
	}

	OpenGLUniformBuffer::OpenGLUniformBuffer(uint64_t size, uint32_t binding)
		: m_size(size), m_binding(binding)
	{
		// Store the parameters
		GE_CORE_INFO("Creating OpenGL Uniform Buffer (Size: {0} bytes, Binding: {1})", size, binding);
		
		// Create and initialize the buffer
		glGenBuffers(1, &m_uboID);
		glBindBuffer(GL_UNIFORM_BUFFER, m_uboID);
		glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// Bind it to the specified binding point
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_uboID);
		
		// Allocate local cache for the data
		m_data = malloc(size);
		if (!m_data) {
			GE_CORE_ERROR("Failed to allocate memory for uniform buffer cache!");
		}
		
		// Zero the buffer initially
		std::memset(m_data, 0, size);

		GE_CORE_INFO("Created OpenGL Uniform Buffer (ID: {0})", m_uboID);
	}
	
	OpenGLUniformBuffer::~OpenGLUniformBuffer()
	{
		// Free the cache memory
		if (m_data) {
			free(m_data);
			m_data = nullptr;
		}
		
		// Delete the buffer
		glDeleteBuffers(1, &m_uboID);
		GE_CORE_INFO("Deleted OpenGL Uniform Buffer (ID: {0})", m_uboID);
	}

	void OpenGLUniformBuffer::updateAllBufferData(const void* data)
	{
		// Update our local data cache
		if (m_data && data) {
			std::memcpy(m_data, data, m_size);
		}
		
		// Bind the buffer and update
		bind();
		glBufferSubData(GL_UNIFORM_BUFFER, 0, m_size, data);
	}

	void OpenGLUniformBuffer::updateBufferData(int64_t offset, uint64_t size, const void* data)
	{
		// Validate parameters
		if (offset < 0 || offset + size > m_size) {
			GE_CORE_ERROR("Invalid buffer update range: offset={0}, size={1}, buffer size={2}", 
				offset, size, m_size);
			return;
		}
		
		// Update our local data cache
		if (m_data && data) {
			std::memcpy(static_cast<char*>(m_data) + offset, data, size);
		}
		
		// Bind the buffer and update
		bind();
		glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
	}
	
	void OpenGLUniformBuffer::bind() const
	{
		glBindBuffer(GL_UNIFORM_BUFFER, m_uboID);
		glBindBufferBase(GL_UNIFORM_BUFFER, m_binding, m_uboID);
	}
	
	void OpenGLUniformBuffer::unbind() const
	{
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	
	void OpenGLUniformBuffer::flush() const
	{
		// Make sure we're bound
		glBindBuffer(GL_UNIFORM_BUFFER, m_uboID);
		
		// Issue synchronization command for the buffer
		glFlush();
	}
}