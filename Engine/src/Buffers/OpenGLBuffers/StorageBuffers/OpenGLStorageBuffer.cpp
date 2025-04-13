#include "OpenGLStorageBuffer.h"
#include "glad/glad.h"
#include "../../../logger/Log.h"
#include "../../BufferConversionHelpers.h"
#include "../../../Debug/TracyProfiler.h"
#include "../../../Utils/GLCapabilities.h"

namespace Rapture {

	// ShaderStorageBuffer implementation
	ShaderStorageBuffer::ShaderStorageBuffer(size_t size, BufferUsage usage, const void* data)
		: m_size(size), m_usage(usage), m_isImmutable(false), m_isMapped(false), m_persistentlyMappedPtr(nullptr)
	{
		RAPTURE_PROFILE_FUNCTION();

		if (GLCapabilities::hasBufferStorage()) {
			// Create buffer with immutable storage
			glCreateBuffers(1, &m_rendererId);
			// Add GL_DYNAMIC_STORAGE_BIT for updates
			GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_DYNAMIC_STORAGE_BIT;
            bool isPersistent = false;
			if (usage == BufferUsage::Stream) {
				flags |= GL_MAP_PERSISTENT_BIT;
                isPersistent = true;
			}
            
			glNamedBufferStorage(m_rendererId, size, data, flags);

			m_isImmutable = true;

            // Map persistently if requested and possible
			if (m_isImmutable && isPersistent) {
				GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
				 // If coherent: mapFlags |= GL_MAP_COHERENT_BIT;

				if (GLCapabilities::hasDSA()) {
					 m_persistentlyMappedPtr = glMapNamedBufferRange(m_rendererId, 0, m_size, mapFlags);
				} else {
					 glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
					 m_persistentlyMappedPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, m_size, mapFlags);
					 glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind after mapping
				}

				if (!m_persistentlyMappedPtr) {
					GE_CORE_ERROR("SSBO: Failed to persistently map buffer (ID: {0})", m_rendererId);
				} else {
					 GE_CORE_INFO("SSBO: Persistently mapped buffer (ID: {0})", m_rendererId);
				}
			}
		} else {
			// Fall back to traditional mutable buffer
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
		GE_CORE_INFO("SSBO: Created ShaderStorageBuffer (ID: {0}, Size: {1} bytes)", m_rendererId, size);
	}

	ShaderStorageBuffer::~ShaderStorageBuffer() {
		RAPTURE_PROFILE_FUNCTION();
		// Unmap persistent pointer if it exists
        if (m_persistentlyMappedPtr && m_isImmutable) {
             if (GLCapabilities::hasDSA()) {
				glUnmapNamedBuffer(m_rendererId);
			} else {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
				glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}
            m_persistentlyMappedPtr = nullptr;
        }

        // Unmap temporary mapping if active
		if (m_isMapped) {
			unmap();
		}
		glDeleteBuffers(1, &m_rendererId);
		GE_CORE_INFO("SSBO: Deleted ShaderStorageBuffer (ID: {0})", m_rendererId);
	}

	void ShaderStorageBuffer::bind() {
		RAPTURE_PROFILE_SCOPE("StorageBuffer Bind");
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
	}

	void ShaderStorageBuffer::unbind() {
		RAPTURE_PROFILE_SCOPE("StorageBuffer Unbind");
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void ShaderStorageBuffer::bindBase(unsigned int bindingPoint) {
		RAPTURE_PROFILE_SCOPE("StorageBuffer BindBase");
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_rendererId);
	}

	void ShaderStorageBuffer::setData(const void* data, size_t size, size_t offset) {
		RAPTURE_PROFILE_FUNCTION();
		if (offset + size > m_size) {
			GE_CORE_ERROR("Buffer overflow: Trying to write {0} bytes at offset {1} in SSBO of size {2}", 
				size, offset, m_size);
			return;
		}

        if (m_isMapped) { // Check temporary mapping
            GE_CORE_ERROR("Attempting setData on a temporarily mapped SSBO! Unmap first.");
            return;
        }

        if (m_persistentlyMappedPtr) {
            RAPTURE_PROFILE_SCOPE("Write Persistent SSBO");
			// Write directly to the persistently mapped pointer
			memcpy(static_cast<char*>(m_persistentlyMappedPtr) + offset, data, size);

			// Flush the range to make writes visible to the GPU
			// (Only needed if GL_MAP_COHERENT_BIT was NOT used)
			if (GLCapabilities::hasDSA()) {
				glFlushMappedNamedBufferRange(m_rendererId, offset, size);
			} else {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
				glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size);
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}
        } else if (m_isImmutable) {
			RAPTURE_PROFILE_SCOPE("Map and Write Immutable SSBO (Temporary)");
			// Use temporary map/unmap for immutable non-persistent buffers
			void* mappedPtr = map(offset, size); // Use the map/unmap methods
            if (!mappedPtr) return; // map() handles error logging

            memcpy(static_cast<char*>(mappedPtr) + offset, data, size); // Offset into the mapped range

            unmap(); // Use the map/unmap methods

		} else {
			RAPTURE_PROFILE_SCOPE("BufferSubData SSBO Update");
			// Use glBufferSubData for mutable buffers
			if (GLCapabilities::hasDSA()) {
				glNamedBufferSubData(m_rendererId, offset, size, data);

			} else {
				glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);

				glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			}
		}
	}

	// map() is now only for temporary mapping
	void* ShaderStorageBuffer::map(size_t offset, size_t size) {
		RAPTURE_PROFILE_FUNCTION();

        // Cannot temporarily map if persistently mapped
        if (m_persistentlyMappedPtr) {
            GE_CORE_ERROR("Cannot temporarily map a persistently mapped SSBO.");
            return nullptr;
        }

		if (m_isMapped) {
			GE_CORE_WARN("Shader storage buffer already temporarily mapped");
			return nullptr;
		}

		if (size == 0) size = m_size - offset;
		
		void* ptr = nullptr;
		if (GLCapabilities::hasDSA()) {
			ptr = glMapNamedBufferRange(m_rendererId, offset, size, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
		} else {
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_rendererId);
			ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
			// Keep bound until unmap
		}
		
		if (ptr) {
			m_isMapped = true;
		} else {

            // Unbind if non-DSA path failed after bind
			if (!GLCapabilities::hasDSA()) glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
		
		return ptr;
	}

	// unmap() is now only for temporary mapping
	void ShaderStorageBuffer::unmap() {
		RAPTURE_PROFILE_FUNCTION();
		if (!m_isMapped) return;
		
		if (GLCapabilities::hasDSA()) {
			glUnmapNamedBuffer(m_rendererId);
		} else {
			// Should still be bound from map() call
			glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Now unbind
		}
		
		m_isMapped = false;
	}

    void ShaderStorageBuffer::barrier()
    {
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    void ShaderStorageBuffer::setDebugLabel(const std::string& label) {
		if (GLCapabilities::hasDebugMarkers()) {
			glObjectLabel(GL_BUFFER, m_rendererId, -1, label.c_str());
		}
	}

}
