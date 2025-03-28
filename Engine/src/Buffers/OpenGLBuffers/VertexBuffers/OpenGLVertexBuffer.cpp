#include "OpenGLVertexBuffer.h"
#include "glad/glad.h"
#include "../../../logger/Log.h"
#include "../../BufferConversionHelpers.h"
#include "../../../Debug/TracyProfiler.h"

namespace Rapture {

    // VertexBuffer implementation
	VertexBuffer::VertexBuffer(size_t size, BufferUsage usage, const void* data)
		: m_size(size), m_usage(usage), m_isImmutable(false), m_idx_last_element(0)
	{
		if (GLCapabilities::hasBufferStorage() && usage != BufferUsage::Stream) {
			// Use immutable storage for static and dynamic buffers
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
				glBindBuffer(GL_ARRAY_BUFFER, m_rendererId);
				glBufferData(GL_ARRAY_BUFFER, size, data, convertBufferUsage(usage));
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
			
			if (data != nullptr) {
				m_idx_last_element = size;
			}
		}
	}

	VertexBuffer::VertexBuffer(const std::vector<unsigned char>& data, BufferUsage usage)
		: VertexBuffer(data.size(), usage, data.data())
	{
	}

	VertexBuffer::~VertexBuffer() {
		glDeleteBuffers(1, &m_rendererId);
	}

	void VertexBuffer::bind() {
		RAPTURE_PROFILE_SCOPE("VertexBuffer Bind");
		glBindBuffer(GL_ARRAY_BUFFER, m_rendererId);
	}

	void VertexBuffer::unbind() {
		RAPTURE_PROFILE_SCOPE("VertexBuffer Unbind");
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void VertexBuffer::setData(const void* data, size_t size, size_t offset) {
		if (offset + size > m_size) {
			GE_CORE_ERROR("Buffer overflow: Trying to write {0} bytes at offset {1} in buffer of size {2}", 
				size, offset, m_size);
			return;
		}

		if (GLCapabilities::hasDSA()) {
			glNamedBufferSubData(m_rendererId, offset, size, data);
		} else {
			glBindBuffer(GL_ARRAY_BUFFER, m_rendererId);
			glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}

	void VertexBuffer::setData(const std::vector<unsigned char>& data, size_t offset) {
		setData(data.data(), data.size(), offset);
	}

	void VertexBuffer::setDebugLabel(const std::string& label) {
		if (GLCapabilities::hasDebugMarkers()) {
			glObjectLabel(GL_BUFFER, m_rendererId, -1, label.c_str());
		}
	}

	// Legacy method implementations for backward compatibility
	void VertexBuffer::addSubData(std::vector<unsigned char>& binary_data) {
		if (binary_data.size() + m_idx_last_element > m_size) {
			GE_CORE_ERROR("Buffers/Buffer.cpp --- no space left in VertexBuffer({0})---", m_rendererId);
			GE_CORE_ERROR("given:{0}Bytes, max:{1}Bytes", binary_data.size() + m_idx_last_element, m_size);
			return;
		}

		m_premature_buffer_data.insert(m_premature_buffer_data.end(), binary_data.begin(), binary_data.end());
		m_idx_last_element += binary_data.size();
	}

	void VertexBuffer::pushData2Buffer(std::vector<std::vector<std::pair<size_t, size_t>>> premature_buffer_layout) {
		if (m_size != m_idx_last_element)
			GE_CORE_WARN("Data Pushed to Buffer has remaining free space: {0}Bytes", m_size - m_idx_last_element);

		std::vector<unsigned char> reordered_data;
		reordered_data.reserve(m_premature_buffer_data.size());

		// Re-order the data in the correct format
		for (int attrib = 0; attrib < premature_buffer_layout.size(); attrib++) {
			auto& attr_data = premature_buffer_layout[attrib];
			for (int i = 0; i < attr_data.size(); i++) {
				reordered_data.insert(reordered_data.end(), 
					m_premature_buffer_data.begin() + attr_data[i].first,
					m_premature_buffer_data.begin() + attr_data[i].second);
			}
		}

		setData(reordered_data.data(), reordered_data.size(), 0);
		m_premature_buffer_data.clear();
	}

}