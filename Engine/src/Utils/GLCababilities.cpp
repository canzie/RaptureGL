#include "GLCapabilities.h"
#include "../Logger/Log.h"

#include "glad/glad.h"
#include "GLFW/glfw3.h"

namespace Rapture {

	// GLCapabilities implementation
	bool GLCapabilities::s_initialized = false;
	bool GLCapabilities::s_hasDSA = false;
	bool GLCapabilities::s_hasBufferStorage = false;
	bool GLCapabilities::s_hasDebugMarkers = false;
	bool GLCapabilities::s_hasBindlessTextures = false;

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
		
        s_hasBindlessTextures = glfwExtensionSupported("GL_ARB_bindless_texture") || 
						  (GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 4));

		GE_CORE_INFO("OpenGL Capabilities:");
		GE_CORE_INFO("  Direct State Access (DSA): {0}", s_hasDSA ? "Yes" : "No");
		GE_CORE_INFO("  Buffer Storage: {0}", s_hasBufferStorage ? "Yes" : "No");
		GE_CORE_INFO("  Debug Markers: {0}", s_hasDebugMarkers ? "Yes" : "No");
		GE_CORE_INFO("  Bindless Textures: {0}", s_hasBindlessTextures ? "Yes" : "No");
		
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

	bool GLCapabilities::hasBindlessTextures()
	{
		if (!s_initialized) initialize();
		return s_hasBindlessTextures;
	}

} // namespace Rapture
