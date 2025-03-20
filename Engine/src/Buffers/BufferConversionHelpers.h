#pragma once

#include "Buffers.h"
#include "glad/glad.h"

namespace Rapture {

    // Helper functions for buffer usage
	inline GLenum convertBufferUsage(BufferUsage usage) {
		switch (usage) {
			case BufferUsage::Static: return GL_STATIC_DRAW;
			case BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
			case BufferUsage::Stream: return GL_STREAM_DRAW;
			default: return GL_STATIC_DRAW;
		}
	}

	inline GLenum convertBufferStorageFlags(BufferUsage usage) {
		switch (usage) {
			case BufferUsage::Static: 
				return GL_DYNAMIC_STORAGE_BIT;
			case BufferUsage::Dynamic: 
				return GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT;
			case BufferUsage::Stream: 
				return GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
			default: 
				return GL_DYNAMIC_STORAGE_BIT;
		}
	}

} // namespace Rapture