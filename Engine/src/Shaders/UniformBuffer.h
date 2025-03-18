#pragma once

#include <cstdint>
#include <string>

namespace Rapture
{
	class UniformBuffer
	{
	public:
		virtual ~UniformBuffer() = default;

		// Data update methods
		virtual void updateAllBufferData(const void* data) = 0;
		virtual void updateBufferData(int64_t offset, uint64_t size, const void* data) = 0;
		
		// Buffer binding/state methods
		virtual void bind() const = 0;
		virtual void unbind() const = 0;
		virtual void flush() const = 0;
		
		// Accessors
		virtual uint32_t getRendererID() const = 0;
		virtual uint32_t getBindingPoint() const = 0;
		virtual uint64_t getSize() const = 0;
		virtual const void* getData() const = 0;
		
		// Create a new uniform buffer (factory method)
		static UniformBuffer* create(uint64_t size, uint32_t binding);
	};
} 