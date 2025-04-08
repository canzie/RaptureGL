#pragma once

namespace Rapture {
	// Wrapper for checking OpenGL capabilities
	class GLCapabilities {
	public:
		static bool hasDSA();
		static bool hasBufferStorage();
		static bool hasDebugMarkers();
        static bool hasBindlessTextures();
	private:
		static bool s_initialized;
		static bool s_hasDSA;
		static bool s_hasBufferStorage;
		static bool s_hasDebugMarkers;
        static bool s_hasBindlessTextures;
		static void initialize();
	};
    
} // namespace Rapture