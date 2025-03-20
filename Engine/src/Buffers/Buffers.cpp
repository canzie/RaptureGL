#include "Buffers.h"
#include "glad/glad.h"
#include <iostream>
#include "GLFW/glfw3.h"
#include "OpenGLBuffers/VertexBuffers/OpenGLVertexBuffer.h"
#include "OpenGLBuffers/IndexBuffers/OpenGLIndexBuffer.h"
#include "OpenGLBuffers/UniformBuffers/OpenGLUniformBuffer.h"
#include "OpenGLBuffers/StorageBuffers/OpenGLStorageBuffer.h"


namespace Rapture {

	// GLCapabilities implementation
	bool GLCapabilities::s_initialized = false;
	bool GLCapabilities::s_hasDSA = false;
	bool GLCapabilities::s_hasBufferStorage = false;
	bool GLCapabilities::s_hasDebugMarkers = false;

	void GLCapabilities::initialize() {
		if (s_initialized) return;

		// Check for DSA extension
		s_hasDSA = glfwExtensionSupported("GL_ARB_direct_state_access") || 
				   (GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5));
		
		// Check for buffer storage extension
		s_hasBufferStorage = glfwExtensionSupported("GL_ARB_buffer_storage") || 
						   (GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 4));
		
		// Check for debug markers
		s_hasDebugMarkers = glfwExtensionSupported("GL_KHR_debug") || 
						  (GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 3));
		
		GE_CORE_INFO("OpenGL Capabilities:");
		GE_CORE_INFO("  Direct State Access (DSA): {0}", s_hasDSA ? "Yes" : "No");
		GE_CORE_INFO("  Buffer Storage: {0}", s_hasBufferStorage ? "Yes" : "No");
		GE_CORE_INFO("  Debug Markers: {0}", s_hasDebugMarkers ? "Yes" : "No");
		
		s_initialized = true;
	}

	bool GLCapabilities::hasDSA() {
		if (!s_initialized) initialize();
		return s_hasDSA;
	}

	bool GLCapabilities::hasBufferStorage() {
		if (!s_initialized) initialize();
		return s_hasBufferStorage;
	}

	bool GLCapabilities::hasDebugMarkers() {
		if (!s_initialized) initialize();
		return s_hasDebugMarkers;
	}



	// Buffer factory method
	std::shared_ptr<Buffer> Buffer::Create(BufferType type, size_t size, BufferUsage usage, const void* data) {
		switch (type) {
			case BufferType::Vertex:
				return std::make_shared<VertexBuffer>(size, usage, data);
			case BufferType::Index:
				return std::make_shared<IndexBuffer>(size, GL_UNSIGNED_INT, usage, data);
			case BufferType::Uniform:
				return std::make_shared<UniformBuffer>(size, usage, data);
			case BufferType::ShaderStorage:
				return std::make_shared<ShaderStorageBuffer>(size, usage, data);
			default:
				GE_CORE_ERROR("Unknown buffer type");
				return nullptr;
		}
	}




}