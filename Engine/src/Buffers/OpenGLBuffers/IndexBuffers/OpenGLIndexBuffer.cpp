#include "OpenGLIndexBuffer.h"
#include "glad/glad.h"
#include "../../../logger/Log.h"
#include "../../BufferConversionHelpers.h"
#include "../../../Debug/Profiler.h"

namespace Rapture {

    // IndexBuffer implementation
	IndexBuffer::IndexBuffer(size_t size, unsigned int componentType, BufferUsage usage, const void* data)
		: m_componentType(componentType), m_size(size), m_usage(usage), m_isImmutable(false), m_idx_last_element(0)
	{
		// Calculate element count based on component type
		int typeSize = 1;
		switch (componentType) {
			case GL_BYTE: case GL_UNSIGNED_BYTE: typeSize = 1; break;
			case GL_UNSIGNED_SHORT: case GL_SHORT: typeSize = 2; break;
			case GL_FLOAT: case GL_UNSIGNED_INT: typeSize = 4; break;
		}
		m_count = size / typeSize;

		if (GLCapabilities::hasBufferStorage() && usage != BufferUsage::Stream) {
			// Use immutable storage
			glCreateBuffers(1, &m_rendererId);
			glNamedBufferStorage(m_rendererId, size, data, convertBufferStorageFlags(usage));
			m_isImmutable = true;
			
			if (data != nullptr) {
				m_idx_last_element = size;
			}
		} else {
			// Fall back to traditional buffers
			if (GLCapabilities::hasDSA()) {
				glCreateBuffers(1, &m_rendererId);
				glNamedBufferData(m_rendererId, size, data, convertBufferUsage(usage));
			} else {
				glGenBuffers(1, &m_rendererId);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererId);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, convertBufferUsage(usage));
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}
			
			if (data != nullptr) {
				m_idx_last_element = size;
			}
		}
	}

	IndexBuffer::IndexBuffer(const std::vector<unsigned char>& data, unsigned int componentType, BufferUsage usage)
		: IndexBuffer(data.size(), componentType, usage, data.data())
	{
	}

	IndexBuffer::~IndexBuffer() {
		glDeleteBuffers(1, &m_rendererId);
	}

	void IndexBuffer::bind() {
		RAPTURE_PROFILE_SCOPE("IndexBuffer Bind");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererId);
	}

	void IndexBuffer::unbind() {
		RAPTURE_PROFILE_SCOPE("IndexBuffer Unbind");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void IndexBuffer::setData(const void* data, size_t size, size_t offset) {
		if (offset + size > m_size) {
			GE_CORE_ERROR("Buffer overflow: Trying to write {0} bytes at offset {1} in buffer of size {2}", 
				size, offset, m_size);
			return;
		}

		if (GLCapabilities::hasDSA()) {
			glNamedBufferSubData(m_rendererId, offset, size, data);
		} else {
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_rendererId);
			glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, data);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
	}

	void IndexBuffer::setData(const std::vector<unsigned char>& data, size_t offset) {
		setData(data.data(), data.size(), offset);
	}

	void IndexBuffer::setDebugLabel(const std::string& label) {
		if (GLCapabilities::hasDebugMarkers()) {
			glObjectLabel(GL_BUFFER, m_rendererId, -1, label.c_str());
		}
	}

	// Legacy method for compatibility
	void IndexBuffer::addSubIndices(std::vector<unsigned char>& indices) {
		if (indices.size() + m_idx_last_element > m_size) {
			GE_CORE_ERROR("Buffers/Buffer.cpp --- no space left in IndexBuffer({0})---", m_rendererId);
			GE_CORE_ERROR("{0}, {1}", indices.size() + m_idx_last_element, m_size);
			return;
		}
		
		setData(indices.data(), indices.size(), m_idx_last_element);
		m_idx_last_element += indices.size();
	}

}
