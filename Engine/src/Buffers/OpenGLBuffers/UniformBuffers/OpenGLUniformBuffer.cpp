#include "OpenGLUniformBuffer.h"
#include "glad/glad.h"
#include "../../../logger/Log.h"
#include "../../BufferConversionHelpers.h"
#include "../../../Debug/TracyProfiler.h"



namespace Rapture {



	// UniformBuffer implementation with CPU cache
	UniformBuffer::UniformBuffer(size_t size, BufferUsage usage, const void* data, unsigned int bindingPoint)
		: m_size(size), m_usage(usage), m_isImmutable(false), m_isMapped(false), m_bindingPoint(bindingPoint), m_persistentlyMappedPtr(nullptr)
	{
		RAPTURE_PROFILE_FUNCTION();

		if (GLCapabilities::hasBufferStorage()) {
			// Create buffer with immutable storage
			glCreateBuffers(1, &m_rendererId);
			// Add GL_DYNAMIC_STORAGE_BIT to allow updates even with immutable storage
			GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_DYNAMIC_STORAGE_BIT;
            bool isPersistent = false; // Track if persistent flag is set
			if (usage == BufferUsage::Stream) {
				flags |= GL_MAP_PERSISTENT_BIT;
				isPersistent = true;
				// Optional: Add coherent bit if desired and available, avoids manual flushing
				// if (GLCapabilities::hasCoherentMapping()) {
				//     flags |= GL_MAP_COHERENT_BIT;
				// }
			}
			
			// Clear any previous OpenGL errors
			while (glGetError() != GL_NO_ERROR);
			glNamedBufferStorage(m_rendererId, size, data, flags);
			// Check for errors
			GLenum error = glGetError();
			if (error != GL_NO_ERROR) {
				GE_CORE_ERROR("UNIFORM BUFFER: Error creating buffer storage: {0} (0x{1:x})", error, error);
			}
			m_isImmutable = true;

			// Map persistently if requested and possible
			if (m_isImmutable && isPersistent) {
				GLbitfield mapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
				 // If coherent: mapFlags |= GL_MAP_COHERENT_BIT;

				if (GLCapabilities::hasDSA()) {
					 m_persistentlyMappedPtr = glMapNamedBufferRange(m_rendererId, 0, m_size, mapFlags);
				} else {
					 glBindBuffer(GL_UNIFORM_BUFFER, m_rendererId);
					 m_persistentlyMappedPtr = glMapBufferRange(GL_UNIFORM_BUFFER, 0, m_size, mapFlags);
					 glBindBuffer(GL_UNIFORM_BUFFER, 0); // Unbind after mapping
				}

				if (!m_persistentlyMappedPtr) {
					GE_CORE_ERROR("UNIFORM BUFFER: Failed to persistently map buffer (ID: {0})", m_rendererId);
					// Decide how to handle failure: maybe fall back to non-persistent?
				} else {
					 GE_CORE_INFO("UNIFORM BUFFER: Persistently mapped buffer (ID: {0})", m_rendererId);
				}
			}
		} else {
			// Fall back to traditional buffer
			if (GLCapabilities::hasDSA()) {
				glCreateBuffers(1, &m_rendererId);
				
				// Clear any previous OpenGL errors
				while (glGetError() != GL_NO_ERROR);
				
				glNamedBufferData(m_rendererId, size, data, convertBufferUsage(usage));
				
				// Check for errors
				GLenum error = glGetError();
				if (error != GL_NO_ERROR) {
					GE_CORE_ERROR("UNIFORM BUFFER: Error creating buffer data: {0} (0x{1:x})", error, error);
				}
			} else {
				glGenBuffers(1, &m_rendererId);
				glBindBuffer(GL_UNIFORM_BUFFER, m_rendererId);
				
				// Clear any previous OpenGL errors
				while (glGetError() != GL_NO_ERROR);
				
				glBufferData(GL_UNIFORM_BUFFER, size, data, convertBufferUsage(usage));
				
				// Check for errors
				GLenum error = glGetError();
				if (error != GL_NO_ERROR) {
					GE_CORE_ERROR("UNIFORM BUFFER: Error creating legacy buffer: {0} (0x{1:x})", error, error);
				}
				
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
			}
		}

		GE_CORE_INFO("UNIFORM BUFFER: Created UniformBuffer (ID: {0}, Size: {1} bytes)", m_rendererId, size);
		
		// Immediately bind to the specified binding point
		bindBase();
	}

	UniformBuffer::~UniformBuffer() {
		RAPTURE_PROFILE_FUNCTION();
		
		if (m_isMapped) {
			unmap();
		}
		glDeleteBuffers(1, &m_rendererId);
		GE_CORE_INFO("UNIFORM BUFFER: Deleted UniformBuffer (ID: {0})", m_rendererId);
	}

	void UniformBuffer::bind() {
		RAPTURE_PROFILE_FUNCTION();
		glBindBuffer(GL_UNIFORM_BUFFER, m_rendererId);
	}

	void UniformBuffer::unbind() {
		RAPTURE_PROFILE_FUNCTION();
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void UniformBuffer::bindBase(unsigned int bindingPoint) {
		RAPTURE_PROFILE_FUNCTION();
		m_bindingPoint = bindingPoint;
		glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_rendererId);
	}

	void UniformBuffer::bindBase() {
		RAPTURE_PROFILE_FUNCTION();
		glBindBufferBase(GL_UNIFORM_BUFFER, m_bindingPoint, m_rendererId);
	}


	void UniformBuffer::setData(const void* data, size_t size, size_t offset) {
		RAPTURE_PROFILE_FUNCTION();
		
		if (offset + size > m_size) {
			GE_CORE_ERROR("UNIFORM BUFFER: Buffer overflow: Trying to write {0} bytes at offset {1} in UBO of size {2}", 
				size, offset, m_size);
			return;
		}



		if (m_persistentlyMappedPtr) {
			RAPTURE_PROFILE_SCOPE("Write Persistent Buffer");
			// Write directly to the persistently mapped pointer
			memcpy(static_cast<char*>(m_persistentlyMappedPtr) + offset, data, size);

			// Flush the range to make writes visible to the GPU
			// (Only needed if GL_MAP_COHERENT_BIT was NOT used)
			if (GLCapabilities::hasDSA()) {
				glFlushMappedNamedBufferRange(m_rendererId, offset, size);
			} else {
				glBindBuffer(GL_UNIFORM_BUFFER, m_rendererId);
				glFlushMappedBufferRange(GL_UNIFORM_BUFFER, offset, size);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
			}
		} else if (m_isImmutable) {
			RAPTURE_PROFILE_SCOPE("Map and Write Immutable Buffer (Temporary)");
			// Use the original temporary map/unmap logic
			void* mappedPtr = nullptr;
			if (GLCapabilities::hasDSA()) {
				mappedPtr = glMapNamedBufferRange(m_rendererId, offset, size, GL_MAP_WRITE_BIT);
			} else {
				glBindBuffer(GL_UNIFORM_BUFFER, m_rendererId);
				mappedPtr = glMapBufferRange(GL_UNIFORM_BUFFER, offset, size, GL_MAP_WRITE_BIT);
				// Keep bound until unmap
			}

			if (!mappedPtr) {
				GE_CORE_ERROR("UNIFORM BUFFER: Failed to map buffer for writing");
				// Unbind if non-DSA path failed after bind
				 if (!GLCapabilities::hasDSA()) glBindBuffer(GL_UNIFORM_BUFFER, 0);
				return;
			}

			memcpy(mappedPtr, data, size);

			if (GLCapabilities::hasDSA()) {
				glUnmapNamedBuffer(m_rendererId);
			} else {
				glUnmapBuffer(GL_UNIFORM_BUFFER);
				glBindBuffer(GL_UNIFORM_BUFFER, 0); // Now unbind
			}
		} else {
			RAPTURE_PROFILE_SCOPE("BufferSubData Update");
			// Use glBufferSubData (existing logic)
			if (GLCapabilities::hasDSA()) {
				glNamedBufferSubData(m_rendererId, offset, size, data);
			} else {
				glBindBuffer(GL_UNIFORM_BUFFER, m_rendererId);
				glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
			}
		}
		

	}

	void* UniformBuffer::map(size_t offset, size_t size) {
		RAPTURE_PROFILE_FUNCTION();
		
		if (m_isMapped) {
			GE_CORE_WARN("UNIFORM BUFFER: Uniform buffer already mapped");
			return nullptr;
		}

		if (size == 0) size = m_size - offset;
		
		void* ptr = nullptr;
		if (GLCapabilities::hasDSA()) {
			ptr = glMapNamedBufferRange(m_rendererId, offset, size, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
		} else {
			glBindBuffer(GL_UNIFORM_BUFFER, m_rendererId);
			ptr = glMapBufferRange(GL_UNIFORM_BUFFER, offset, size, GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}
		
		if (ptr) {
			m_isMapped = true;
		} else {
			GE_CORE_ERROR("UNIFORM BUFFER: Failed to map uniform buffer");
		}
		
		return ptr;
	}

	void UniformBuffer::unmap() {
		RAPTURE_PROFILE_FUNCTION();
		
		if (!m_isMapped) return;
		
		if (GLCapabilities::hasDSA()) {
			glUnmapNamedBuffer(m_rendererId);
		} else {
			glBindBuffer(GL_UNIFORM_BUFFER, m_rendererId);
			glUnmapBuffer(GL_UNIFORM_BUFFER);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}
		
		m_isMapped = false;
	}

	void UniformBuffer::flush() const {
		// Make sure we're bound
		//glBindBuffer(GL_UNIFORM_BUFFER, m_rendererId);
		
		// Issue synchronization for GPU-side changes
		//glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
		//glFlush();
	}

	void UniformBuffer::setDebugLabel(const std::string& label) {
		if (GLCapabilities::hasDebugMarkers()) {
			glObjectLabel(GL_BUFFER, m_rendererId, -1, label.c_str());
		}
	}


}
