#include "OpenGLVertexBuffer.h"
#include "glad/glad.h"
#include "../../../Logger/Log.h"
#include "../../BufferConversionHelpers.h"
#include "../../../Debug/TracyProfiler.h"

namespace Rapture {

    // VertexBuffer implementation
	VertexBuffer::VertexBuffer(size_t size, BufferUsage usage, const void* data)
		: m_size(size), m_usage(usage), m_isImmutable(false)
	{
		if (GLCapabilities::hasBufferStorage() && usage != BufferUsage::Stream) {
			// Use immutable storage for static and dynamic buffers
			glCreateBuffers(1, &m_rendererId);
			glNamedBufferStorage(m_rendererId, size, data, convertBufferStorageFlags(usage));
			m_isImmutable = true;
			

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
			

		}

        generateBufferHandle();
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

    void VertexBuffer::bindBase(uint32_t bindingPoint) const
    {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_rendererId);
    }



    void VertexBuffer::setData(const void *data, size_t size, size_t offset)
    {
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



    void VertexBuffer::generateBufferHandle()
    {
        if (!GLCapabilities::hasBindlessTextures()) {
            GE_CORE_WARN("VertexBuffer::generateBufferHandle - Bindless buffers not supported");
            return;
        }

        glBindBuffer(GL_ARRAY_BUFFER, m_rendererId);
        glGetBufferParameterui64vNV(GL_ARRAY_BUFFER, GL_BUFFER_GPU_ADDRESS_NV, &m_bufferHandle);
        glMakeBufferResidentNV(GL_ARRAY_BUFFER, GL_READ_ONLY);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        if (m_bufferHandle == 0) {
            GE_CORE_ERROR("VertexBuffer::generateBufferHandle - Failed to generate buffer handle");
        }
    }
}