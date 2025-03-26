#include "OpenGLStorageBuffer.h"
#include "glad/glad.h"
#include "../../../logger/Log.h"
#include "../../BufferConversionHelpers.h"
#include "../../../Debug/Profiler.h"

namespace Rapture {

	// ShaderStorageBuffer implementation
	ShaderStorageBuffer::ShaderStorageBuffer(size_t size, BufferUsage usage, const void* data)
		: m_size(size), m_usage(usage), m_isImmutable(false), m_isMapped(false)
	{
		if (GLCapabilities::hasBufferStorage()) {
			// Create buffer with immutable storage
			glCreateBuffers(1, &m_rendererId);
			GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT;
			if (usage == BufferUsage::Stream) {
				flags |= GL_MAP_PERSISTENT_BIT;
			}
			glNamedBufferStorage(m_rendererId, size, data, flags);
			m_isImmutable = true;
		} else {
			// Fall back to traditional buffer
			if (GLCapabilities::hasDSA()) {
				glCreateBuffers(1, &m_rendererId);
				glNamedBufferData(m_rendererId, size, data, convertBufferUsage(usage));
			} else {
				glGenBuffers(1, &m_rendererId);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
				glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, convertBufferUsage(usage));
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}
		}
	}

	ShaderStorageBuffer::~ShaderStorageBuffer() {
		if (m_isMapped) {
			unmap();
		}
		glDeleteBuffers(1, &m_rendererId);
	}

	void ShaderStorageBuffer::bind() {
		RAPTURE_PROFILE_SCOPE("StorageBuffer Bind");
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
	}

	void ShaderStorageBuffer::unbind() {
		RAPTURE_PROFILE_SCOPE("StorageBuffer Unbind");
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void ShaderStorageBuffer::bindBase(unsigned int index) {
		RAPTURE_PROFILE_SCOPE("StorageBuffer BindBase");
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, m_rendererId);
	}

	void ShaderStorageBuffer::setData(const void* data, size_t size, size_t offset) {
		if (offset + size > m_size) {
			GE_CORE_ERROR("Buffer overflow: Trying to write {0} bytes at offset {1} in SSBO of size {2}", 
				size, offset, m_size);
			return;
		}

		if (GLCapabilities::hasDSA()) {
			glNamedBufferSubData(m_rendererId, offset, size, data);
		} else {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
	}

	void* ShaderStorageBuffer::map(size_t offset, size_t size) {
		if (m_isMapped) {
			GE_CORE_WARN("Shader storage buffer already mapped");
			return nullptr;
		}

		if (size == 0) size = m_size - offset;
		
		void* ptr = nullptr;
		if (GLCapabilities::hasDSA()) {
			ptr = glMapNamedBufferRange(m_rendererId, offset, size, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
		} else {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
			ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
		
		if (ptr) {
			m_isMapped = true;
		} else {
			GE_CORE_ERROR("Failed to map shader storage buffer");
		}
		
		return ptr;
	}

	void ShaderStorageBuffer::unmap() {
		if (!m_isMapped) return;
		
		if (GLCapabilities::hasDSA()) {
			glUnmapNamedBuffer(m_rendererId);
		} else {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
		
		m_isMapped = false;
	}

	void ShaderStorageBuffer::setDebugLabel(const std::string& label) {
		if (GLCapabilities::hasDebugMarkers()) {
			glObjectLabel(GL_BUFFER, m_rendererId, -1, label.c_str());
		}
	}

}
