#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <utility>
#include <memory>
#include "../logger/Log.h"
#include "../DataTypes.h"
#include "../RendererAPI.h"

namespace Rapture {
 

	enum class BufferUsage {
		Static,     // Data is set once and used many times
		Dynamic,    // Data is changed occasionally and used many times
		Stream      // Data is changed frequently and used many times
	};

	enum class BufferType {
		Vertex,
		Index,
		Uniform,
		ShaderStorage
	};

	// Wrapper for checking OpenGL capabilities
	class GLCapabilities {
	public:
		static bool hasDSA();
		static bool hasBufferStorage();
		static bool hasDebugMarkers();
	private:
		static bool s_initialized;
		static bool s_hasDSA;
		static bool s_hasBufferStorage;
		static bool s_hasDebugMarkers;
		static void initialize();
	};

	// Buffer base class
	class Buffer {
	public:
		virtual ~Buffer() = default;
		
		virtual void bind() = 0;
		virtual void unbind() = 0;
		
		virtual void setDebugLabel(const std::string& label) = 0;
		virtual unsigned int getID() const = 0;
		
        static std::shared_ptr<Buffer> Create(BufferType type, size_t size, BufferUsage usage, const void* data);

		
        
            
	};

	



}